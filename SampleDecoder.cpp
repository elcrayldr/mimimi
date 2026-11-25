#include "Library/SampleDecoder.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    SampleDecoder::SampleDecoder()
    {
        formatManager_.registerFormat(new juce::WavAudioFormat(), true);
        formatManager_.registerFormat(new juce::AiffAudioFormat(), false);
       #if JUCE_USE_FLAC
        formatManager_.registerFormat(new juce::FlacAudioFormat(), false);
       #endif
    }

    void SampleDecoder::registerFormat(std::unique_ptr<juce::AudioFormat> format)
    {
        if (format != nullptr)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            formatManager_.registerFormat(format.release(), false);
        }
    }

    bool SampleDecoder::isSupportedAudioFile(const juce::File& file) const
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
    }

    juce::StringArray SampleDecoder::getSupportedExtensions() const
    {
        return { ".wav", ".aif", ".aiff", ".flac" };
    }

    SampleDecodeResult SampleDecoder::loadSample(const juce::File& file)
    {
        SampleDecodeResult result;

        if (!file.existsAsFile())
        {
            result.isMissing    = true;
            result.errorMessage = "Sample file not found: " + file.getFullPathName();
            return result;
        }

        const auto canonicalPath = file.getFullPathName();
        const auto modTime       = file.getLastModificationTime().toMilliseconds();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(canonicalPath);
            if (it != cache_.end() && it->second.modificationTimeMs == modTime)
            {
                if (auto existing = it->second.weakBuffer.lock())
                {
                    result.buffer  = existing;
                    result.success = true;
                    return result;
                }
            }
        }

        std::unique_ptr<juce::AudioFormatReader> reader;
        {
            // JUCE AudioFormatManager is mutable when formats are registered. Protect
            // reader creation against concurrent registerFormat() calls, then decode
            // outside the lock so large files never hold the cache/format mutex.
            std::lock_guard<std::mutex> lock(mutex_);
            reader.reset(formatManager_.createReaderFor(file));
        }
        if (reader == nullptr)
        {
            result.errorMessage = "Unsupported or corrupted audio file: " + file.getFileName();
            return result;
        }

        if (reader->lengthInSamples <= 0 || reader->numChannels <= 0)
        {
            result.errorMessage = "Empty audio file: " + file.getFileName();
            return result;
        }

        // Bound decoded RAM to approximately 256 MiB for stereo float audio.
        // Reject oversized files instead of silently truncating the source.
        constexpr juce::int64 kMaxDecodedFrames = 32'000'000LL;
        if (reader->lengthInSamples > kMaxDecodedFrames)
        {
            result.errorMessage = "Sample exceeds the 256 MiB decoded-audio limit: " + file.getFileName();
            return result;
        }

        const int numSamples  = static_cast<int>(reader->lengthInSamples);
        const int numChannels = static_cast<int>(std::min<unsigned int>(reader->numChannels, 2u));

        auto buf = std::make_shared<SampleBuffer>();
        buf->filePath    = canonicalPath;
        buf->name        = file.getFileNameWithoutExtension();
        buf->sampleRate  = (reader->sampleRate > 1.0) ? reader->sampleRate : 44100.0;
        buf->numChannels = numChannels;
        buf->numSamples  = numSamples;
        buf->audio.setSize(numChannels, numSamples, false, true, false);

        if (!reader->read(&buf->audio, 0, numSamples, 0, true, numChannels > 1))
        {
            result.errorMessage = "Failed reading audio samples from: " + file.getFileName();
            return result;
        }

        // Float WAVs can legally contain NaN/Inf payloads. Sanitize them once during
        // background decode so one corrupt sample cannot poison an entire voice/mix bus.
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buf->audio.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (!std::isfinite(channelData[i]))
                    channelData[i] = 0.0f;
            }
        }

        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max(peak, buf->audio.getMagnitude(ch, 0, numSamples));
        buf->peakAmplitude = peak;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_[canonicalPath] = CacheEntry{ modTime, buf };
        }

        result.buffer  = buf;
        result.success = true;
        return result;
    }


    std::optional<int> SampleDecoder::detectRootNote(const juce::File& file)
    {
        if (!file.existsAsFile() || !isSupportedAudioFile(file))
            return std::nullopt;

        std::unique_ptr<juce::AudioFormatReader> reader;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            reader.reset(formatManager_.createReaderFor(file));
        }
        if (reader == nullptr || reader->lengthInSamples < 2048 || reader->sampleRate < 4000.0)
            return std::nullopt;

        // Analyse a short mono excerpt away from the full-file decode path. This keeps
        // root detection inexpensive and avoids loading long ambience/sample files twice.
        constexpr double analysisRate = 11025.0;
        const int stride = std::max(1, static_cast<int>(std::round(reader->sampleRate / analysisRate)));
        const int sourceFrames = static_cast<int>(std::min<juce::int64>(
            reader->lengthInSamples, static_cast<juce::int64>(stride) * 8192));
        const int frames = sourceFrames / stride;
        if (frames < 1024)
            return std::nullopt;

        juce::AudioBuffer<float> source(static_cast<int>(std::min<unsigned int>(reader->numChannels, 2u)), sourceFrames);
        if (!reader->read(&source, 0, sourceFrames, 0, true, source.getNumChannels() > 1))
            return std::nullopt;

        std::vector<float> mono(static_cast<size_t>(frames), 0.0f);
        double mean = 0.0;
        for (int i = 0; i < frames; ++i)
        {
            const int sourceIndex = i * stride;
            float value = source.getSample(0, sourceIndex);
            if (source.getNumChannels() > 1)
                value = 0.5f * (value + source.getSample(1, sourceIndex));
            mono[static_cast<size_t>(i)] = value;
            mean += value;
        }
        mean /= static_cast<double>(frames);

        double energy = 0.0;
        for (auto& value : mono)
        {
            value = static_cast<float>(value - mean);
            energy += static_cast<double>(value) * value;
        }
        if (energy < 1.0e-7)
            return std::nullopt;

        // Normalised autocorrelation across a musical range of roughly 35–1500 Hz.
        const int minLag = 7;
        const int maxLag = std::min(315, frames / 3);
        double bestCorrelation = 0.0;
        int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double cross = 0.0, leftEnergy = 0.0, rightEnergy = 0.0;
            const int count = frames - lag;
            for (int i = 0; i < count; ++i)
            {
                const double a = mono[static_cast<size_t>(i)];
                const double b = mono[static_cast<size_t>(i + lag)];
                cross += a * b;
                leftEnergy += a * a;
                rightEnergy += b * b;
            }

            const double denom = std::sqrt(leftEnergy * rightEnergy);
            const double correlation = (denom > 1.0e-12) ? (cross / denom) : 0.0;
            if (correlation > bestCorrelation)
            {
                bestCorrelation = correlation;
                bestLag = lag;
            }
        }

        // Conservative threshold: ambiguous/noisy/percussive samples remain manually editable
        // instead of being assigned a misleading pitch.
        if (bestLag == 0 || bestCorrelation < 0.78)
            return std::nullopt;

        const double frequency = analysisRate / static_cast<double>(bestLag);
        const int midiNote = static_cast<int>(std::round(69.0 + 12.0 * std::log2(frequency / 440.0)));
        if (midiNote < 0 || midiNote > 127)
            return std::nullopt;

        return midiNote;
    }

    juce::String SampleDecoder::computeFileHash(const juce::File& file)
    {
        if (!file.existsAsFile())
            return {};

        juce::FileInputStream stream(file);
        if (!stream.openedOk())
            return {};

        // 64-bit FNV-1a streamed across file contents for fast deterministic duplicate detection
        uint64_t hash = 14695981039346656037ULL;
        uint8_t chunk[16384];
        while (!stream.isExhausted())
        {
            const int bytesRead = stream.read(chunk, sizeof(chunk));
            if (bytesRead <= 0)
                break;

            for (int i = 0; i < bytesRead; ++i)
            {
                hash ^= chunk[i];
                hash *= 1099511628211ULL;
            }
        }
        return juce::String::toHexString(static_cast<juce::int64>(hash));
    }

    void SampleDecoder::clearCache()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }
}
