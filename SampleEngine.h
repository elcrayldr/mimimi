#pragma once

#include "Engine/SampleData.h"
#include "DSP/Envelopes/AhdsrEnvelope.h"
#include "DSP/Filters/ZdfSvfFilter.h"
#include "DSP/Modulation/ModMatrix.h"
#include <array>

namespace cray
{
    struct SampleGlobalParams
    {
        float gainLinear     = 1.0f;
        float pan            = 0.0f;
        int   pitchSemis     = 0;
        float fineCents      = 0.0f;
        float velSensitivity = 0.75f;
    };

    /**
     * Polyphonic multi-layer sample voice (up to 8 simultaneous zone layers per note).
     * Zero allocations, lock-free, real-time safe.
     */
    class SampleVoice
    {
    public:
        static constexpr int kMaxLayersPerVoice = 8;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void startNote(const CompiledSampleInstrument* instrument,
                       int midiNote,
                       float velocity01,
                       bool isLegato) noexcept;

        void releaseNote() noexcept;
        void fastKill() noexcept;

        void renderSample(const SampleGlobalParams& globalParams,
                          float currentNoteFloat,
                          float pitchBendSemis,
                          const ModOffsets& modOffsets,
                          float& outLeft,
                          float& outRight) noexcept;

        bool  isActive() const noexcept;
        bool  isReleasing() const noexcept;
        float getEnvLevel() const noexcept;

    private:
        struct ZonePlayer
        {
            const CompiledSampleZone* zone = nullptr;
            bool          active           = false;
            double        playhead         = 0.0;
            int64_t       samplesRendered  = 0;
            AhdsrEnvelope env;
            ZdfSvfFilter  filter;
        };

        void releasePinnedInstrument() noexcept;
        static float readZoneSample(const SampleBuffer& buf, int channel, double pos) noexcept;

        double                          sampleRate_         = 44100.0;
        const CompiledSampleInstrument* pinnedInstrument_   = nullptr;
        float                           velocity_           = 1.0f;
        float                           layerGains_[kMaxLayersPerVoice]{};
        std::array<ZonePlayer, kMaxLayersPerVoice> players_;
    };
}
