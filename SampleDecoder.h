#pragma once

#include "Engine/SampleData.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <mutex>

namespace cray
{
    struct SampleDecodeResult
    {
        std::shared_ptr<const SampleBuffer> buffer;
        bool         success      = false;
        bool         isMissing    = false;
        juce::String errorMessage;
    };

    /**
     * Thread-safe sample decoder & weak cache supporting WAV, AIFF, and FLAC.
     * Built on JUCE AudioFormatManager so additional formats can be registered cleanly.
     */
    class SampleDecoder
    {
    public:
        SampleDecoder();

        /** Register an additional JUCE AudioFormat for future format expansion. */
        void registerFormat(std::unique_ptr<juce::AudioFormat> format);

        bool isSupportedAudioFile(const juce::File& file) const;
        juce::StringArray getSupportedExtensions() const;

        /** Decode (or return cached) immutable SampleBuffer. Never call from the audio thread. */
        SampleDecodeResult loadSample(const juce::File& file);

        /** Compute deterministic content hash for duplicate detection. */
        static juce::String computeFileHash(const juce::File& file);

        void clearCache();

    private:
        struct CacheEntry
        {
            juce::int64 modificationTimeMs = 0;
            std::weak_ptr<const SampleBuffer> weakBuffer;
        };

        juce::AudioFormatManager formatManager_;
        std::mutex               mutex_;
        std::map<juce::String, CacheEntry> cache_;
    };
}
