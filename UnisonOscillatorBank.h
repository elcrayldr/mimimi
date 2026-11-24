#pragma once

#include "Common/ProductInfo.h"
#include "DSP/Oscillators/PolyBlepOscillator.h"
#include <array>
#include <cstdint>

namespace cray
{
    /**
     * Multi-voice unison oscillator bank (1..8 voices) with symmetric detune,
     * stereo spread, per-oscillator pan, and gain compensation.
     */
    class UnisonOscillatorBank
    {
    public:
        void prepare(double sampleRate) noexcept;
        void reset(uint32_t seedBase, bool randomizePhases = true) noexcept;

        void setParameters(OscWaveform waveform,
                           float baseFreqHz,
                           int unisonVoices,
                           float detuneAmount01,
                           float stereoSpread01,
                           float oscPanMinus1To1) noexcept;

        void processSample(float& outLeft, float& outRight) noexcept;

    private:
        struct VoiceLayout
        {
            float detuneRatio = 1.0f;
            float panLeft     = 0.7071f;
            float panRight    = 0.7071f;
        };

        std::array<PolyBlepOscillator, ProductInfo::kMaxUnisonVoices> voices_;
        std::array<VoiceLayout, ProductInfo::kMaxUnisonVoices>        layout_;

        double sampleRate_   = 44100.0;
        int    activeCount_  = 1;
        float  normGain_     = 1.0f;
    };
}
