#include "DSP/Oscillators/PolyBlepOscillator.h"
#include "DSP/Utilities/DspMath.h"
#include <cmath>

namespace cray
{
    void PolyBlepOscillator::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
    }

    void PolyBlepOscillator::reset(float initialPhase, uint32_t noiseSeed) noexcept
    {
        phase_ = initialPhase - std::floor(initialPhase);
        triIntegrator_ = 0.0f;
        noiseState_ = (noiseSeed != 0u) ? noiseSeed : 0x9E3779B9u;
    }

    void PolyBlepOscillator::setFrequency(float freqHz) noexcept
    {
        const float nyquistGuard = static_cast<float>(sampleRate_ * 0.48);
        const float clampedHz = DspMath::clamp(freqHz, 0.0f, nyquistGuard);
        phaseInc_ = clampedHz / static_cast<float>(sampleRate_);
    }

    float PolyBlepOscillator::polyBlep(float t, float dt) noexcept
    {
        if (dt <= 0.0f)
            return 0.0f;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float PolyBlepOscillator::processSample() noexcept
    {
        if (waveform_ == OscWaveform::Noise)
            return DspMath::nextWhiteNoise(noiseState_);

        float out = 0.0f;
        const float t  = phase_;
        const float dt = phaseInc_;

        switch (waveform_)
        {
            case OscWaveform::Sine:
                out = std::sin(DspMath::kTwoPi * t);
                break;

            case OscWaveform::Saw:
            {
                out = 2.0f * t - 1.0f;
                out -= polyBlep(t, dt);
                break;
            }

            case OscWaveform::Square:
            {
                out = (t < 0.5f) ? 1.0f : -1.0f;
                out += polyBlep(t, dt);
                float t2 = t + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                out -= polyBlep(t2, dt);
                break;
            }

            case OscWaveform::Triangle:
            {
                // Integrate PolyBLEP square wave for alias-reduced triangle
                float sq = (t < 0.5f) ? 1.0f : -1.0f;
                sq += polyBlep(t, dt);
                float t2 = t + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                sq -= polyBlep(t2, dt);

                triIntegrator_ = dt * sq + (1.0f - dt) * triIntegrator_;
                // Scale and clamp gently
                out = DspMath::clamp(triIntegrator_ * 4.0f, -1.05f, 1.05f);
                break;
            }

            case OscWaveform::Noise:
                break;
        }

        phase_ += dt;
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;

        return out;
    }
}
