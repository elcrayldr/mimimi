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

        // Shared global voice controls also process sample playback.
        float ampAttack = 0.006f;
        float ampDecay = 0.30f;
        float ampSustain = 0.80f;
        float ampRelease = 0.25f;
        bool filterEnabled = true;
        SvfFilterType filterType = SvfFilterType::LowPass24;
        float filterCutoffHz = 14000.0f;
        float filterResonance = 0.15f;
        float filterKeyTrack = 0.25f;
        float filterEnvAmount = 0.35f;
        float filterAttack = 0.01f;
        float filterDecay = 0.40f;
        float filterSustain = 0.35f;
        float filterRelease = 0.35f;
        float lfo1ToPitch = 0.0f;
        float lfo1ToCutoff = 0.0f;
        float lfo1ToAmp = 0.0f;
        float lfo1Value = 0.0f;
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
        void updateEnvelopes(const SampleGlobalParams& globalParams) noexcept;

        void renderSample(const SampleGlobalParams& globalParams,
                          float currentNoteFloat,
                          float pitchBendSemis,
                          const ModOffsets& modOffsets,
                          float& outLeft,
                          float& outRight) noexcept;

        bool  isActive() const noexcept;
        bool  isReleasing() const noexcept;
        float getEnvLevel() const noexcept;
        float getFilterEnvLevel() const noexcept { return filterEnv_.getCurrentLevel(); }
        float getVelocity() const noexcept { return velocity_; }

    private:
        struct ZonePlayer
        {
            const CompiledSampleZone* zone = nullptr;
            bool          active           = false;
            double        playhead         = 0.0;
            int64_t       samplesRendered  = 0;
            AhdsrEnvelope env;
            ZdfSvfFilter  filter;

            // Filter coefficients are expensive (tan/pow); the modulation sources are
            // block-rate here, so recalculate only when their effective inputs change.
            bool          filterParamsValid = false;
            float         lastCutoffMod = 0.0f;
            float         lastResonanceMod = 0.0f;
            SvfFilterType lastFilterType = SvfFilterType::LowPass12;
        };

        void releasePinnedInstrument() noexcept;
        static float readZoneSample(const SampleBuffer& buf, int channel, double pos) noexcept;

        double                          sampleRate_         = 44100.0;
        const CompiledSampleInstrument* pinnedInstrument_   = nullptr;
        AhdsrEnvelope                   ampEnv_;
        AhdsrEnvelope                   filterEnv_;
        ZdfSvfFilter                    globalFilter_;
        bool                            globalFilterParamsValid_ = false;
        float                           lastGlobalCutoff_ = 0.0f;
        float                           lastGlobalResonance_ = 0.0f;
        float                           velocity_           = 1.0f;
        float                           layerGains_[kMaxLayersPerVoice]{};
        std::array<ZonePlayer, kMaxLayersPerVoice> players_;
    };
}
