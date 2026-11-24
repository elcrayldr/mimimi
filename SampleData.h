#pragma once

#include "DSP/Filters/ZdfSvfFilter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace cray
{
    /**
     * Immutable decoded audio sample buffer shared across zones and voices.
     * Loaded exclusively on background/message threads; read-only on the audio thread.
     */
    struct SampleBuffer
    {
        juce::String              filePath;
        juce::String              name;
        double                    sampleRate = 44100.0;
        int                       numChannels = 1;
        int                       numSamples  = 0;
        float                     peakAmplitude = 0.0f;
        juce::AudioBuffer<float>  audio;
    };

    /**
     * Compiled, read-only zone snapshot consumed by SampleEngine on the audio thread.
     */
    struct CompiledSampleZone
    {
        juce::String                  zoneId;
        std::shared_ptr<const SampleBuffer> sample;

        int           layerIndex       = 0;
        int           rootNote         = 60;
        int           lowKey           = 0;
        int           highKey          = 127;
        int           lowVelocity      = 1;
        int           highVelocity     = 127;

        int           pitchSemitones   = 0;
        float         fineTuneCents    = 0.0f;
        bool          reverse          = false;

        int           startSample      = 0;
        int           endSample        = 0;
        bool          loopEnabled      = false;
        int           loopStartSample  = 0;
        int           loopEndSample    = 0;
        int           loopCrossfade    = 256;

        int           fadeInSamples    = 0;
        int           fadeOutSamples   = 0;

        float         gainLinear       = 1.0f; // includes gainDb + normalizeGainDb
        float         pan              = 0.0f; // [-1, +1]

        float         attackSec        = 0.005f;
        float         decaySec         = 0.25f;
        float         sustainLevel     = 1.0f;
        float         releaseSec       = 0.20f;

        bool          filterEnabled    = false;
        SvfFilterType filterType       = SvfFilterType::LowPass12;
        float         filterCutoffHz   = 18000.0f;
        float         filterResonance  = 0.1f;

        bool matchesNote(int midiNote, int velocity) const noexcept
        {
            return sample != nullptr
                && sample->numSamples > 1
                && midiNote >= lowKey && midiNote <= highKey
                && velocity >= lowVelocity && velocity <= highVelocity;
        }
    };

    /**
     * Immutable compiled instrument snapshot published atomically to the audio thread.
     * Uses an atomic audioRefCount so the message thread never frees a snapshot
     * while any audio voice still holds a pointer to it.
     */
    struct CompiledSampleInstrument
    {
        juce::String                    instrumentId;
        juce::String                    instrumentName;
        std::vector<CompiledSampleZone> zones;
        std::vector<bool>               layerEnabled;
        std::vector<float>              layerGain;
        mutable std::atomic<int>        audioRefCount{0};

        void retainAudio() const noexcept
        {
            audioRefCount.fetch_add(1, std::memory_order_acq_rel);
        }

        void releaseAudio() const noexcept
        {
            audioRefCount.fetch_sub(1, std::memory_order_acq_rel);
        }
    };
}
