#pragma once

#include "Common/ProductInfo.h"
#include "DSP/Oscillators/UnisonOscillatorBank.h"
#include "DSP/Envelopes/AhdsrEnvelope.h"
#include "DSP/Filters/ZdfSvfFilter.h"
#include "DSP/Modulation/ModMatrix.h"
#include <array>

namespace cray
{
    struct SynthOscParams
    {
        bool        enabled   = false;
        OscWaveform waveform  = OscWaveform::Saw;
        float       level     = 0.8f;
        float       pan       = 0.0f;
        int         pitchSemi = 0;
        float       fineCents = 0.0f;
        int         unison    = 1;
        float       detune    = 0.25f;
        float       spread    = 0.50f;
    };

    struct SynthGlobalParams
    {
        std::array<SynthOscParams, ProductInfo::kNumOscillators> oscs;

        float         ampAttack    = 0.006f;
        float         ampDecay     = 0.30f;
        float         ampSustain   = 0.80f;
        float         ampRelease   = 0.25f;

        bool          filterEnabled   = true;
        SvfFilterType filterType      = SvfFilterType::LowPass24;
        float         filterCutoffHz  = 14000.0f;
        float         filterResonance = 0.15f;
        float         filterKeyTrack  = 0.25f;
        float         filterEnvAmount = 0.35f;

        float         filterAttack    = 0.01f;
        float         filterDecay     = 0.40f;
        float         filterSustain   = 0.35f;
        float         filterRelease   = 0.35f;

        float         velSensitivity  = 0.75f;
        float         lfo1ToPitch     = 0.0f;
        float         lfo1ToCutoff    = 0.0f;
        float         lfo1ToAmp       = 0.0f;
    };

    /**
     * Single polyphonic synthesizer voice (4 unison-capable oscillators, dual ADSR, ZDF-SVF filter).
     * Zero allocations, real-time safe.
     */
    class SynthVoice
    {
    public:
        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void startNote(int midiNote, float velocity01, float currentNoteFloat, bool isLegato, uint32_t seed) noexcept;
        void releaseNote() noexcept;
        void fastKill() noexcept;

        void updateEnvelopes(const SynthGlobalParams& params) noexcept;

        void renderSample(const SynthGlobalParams& params,
                          const ModMatrix& modMatrix,
                          float currentNoteFloat,
                          float pitchBendSemis,
                          float lfo1Val,
                          float lfo2Val,
                          float modWheelVal,
                          float aftertouchVal,
                          float& outLeft,
                          float& outRight) noexcept;

        bool  isActive() const noexcept { return ampEnv_.isActive(); }
        bool  isReleasing() const noexcept { return ampEnv_.isReleasing(); }
        float getEnvLevel() const noexcept { return ampEnv_.getCurrentLevel(); }

    private:
        double sampleRate_ = 44100.0;
        int    midiNote_   = 60;
        float  velocity_   = 1.0f;

        std::array<UnisonOscillatorBank, ProductInfo::kNumOscillators> oscBanks_;
        AhdsrEnvelope ampEnv_;
        AhdsrEnvelope filterEnv_;
        ZdfSvfFilter  filter_;
        ModOffsets    modOffsets_{};
    };
}
