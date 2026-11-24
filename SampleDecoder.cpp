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

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
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
