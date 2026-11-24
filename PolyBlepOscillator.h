#pragma once

#include <cstdint>

namespace cray
{
    enum class OscWaveform : int
    {
        Sine = 0,
        Saw,
        Square,
        Triangle,
        Noise
    };

    /**
     * Anti-aliased PolyBLEP oscillator supporting Sine, Saw, Square, Triangle, and Noise.
     * Real-time safe with zero heap allocations.
     */
    class PolyBlepOscillator
    {
    public:
        void prepare(double sampleRate) noexcept;
        void reset(float initialPhase = 0.0f, uint32_t noiseSeed = 0x12345678u) noexcept;

        void setWaveform(OscWaveform waveform) noexcept { waveform_ = waveform; }
        void setFrequency(float freqHz) noexcept;

        float processSample() noexcept;

    private:
        static float polyBlep(float t, float dt) noexcept;

        double      sampleRate_    = 44100.0;
        OscWaveform waveform_      = OscWaveform::Saw;
        float       phase_         = 0.0f;
        float       phaseInc_      = 0.01f;
        float       triIntegrator_ = 0.0f;
        uint32_t    noiseState_    = 0x9E3779B9u;
    };
}
