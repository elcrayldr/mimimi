#include "DSP/Effects/SaturationDistortion.h"
#include "DSP/Utilities/DspMath.h"
#include <cmath>

namespace cray
{
    void SaturationDistortion::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        dcSatL_.prepare(sampleRate_);
        dcSatR_.prepare(sampleRate_);
        dcDistL_.prepare(sampleRate_);
        dcDistR_.prepare(sampleRate_);
        satToneFilter_.prepare(sampleRate_);
        distToneFilter_.prepare(sampleRate_);
        reset();
    }

    void SaturationDistortion::reset() noexcept
    {
        dcSatL_.reset();
        dcSatR_.reset();
        dcDistL_.reset();
        dcDistR_.reset();
        satToneFilter_.reset();
        distToneFilter_.reset();
    }

    void SaturationDistortion::setParameters(const SatParams& sat, const DistParams& dist) noexcept
    {
        sat_  = sat;
        dist_ = dist;

        const float satCut = 1200.0f * std::pow(16.0f, DspMath::clamp(sat_.tone, 0.0f, 1.0f));
        satToneFilter_.setParameters(SvfFilterType::LowPass12, satCut, 0.1f);

        const float distCut = 900.0f * std::pow(18.0f, DspMath::clamp(dist_.tone, 0.0f, 1.0f));
        distToneFilter_.setParameters(SvfFilterType::LowPass12, distCut, 0.15f);
    }

    float SaturationDistortion::shapeSat(float x, SaturationMode mode, float preGain) noexcept
    {
        const float driven = x * preGain;
        switch (mode)
        {
            case SaturationMode::Tape:
                return std::tanh(driven) / std::max(1.0f, std::tanh(preGain));

            case SaturationMode::Tube:
            {
                // Asymmetric soft triode curve adding warm 2nd harmonics
                const float biased = driven + 0.15f;
                const float shaped = std::tanh(biased) - std::tanh(0.15f);
                return shaped * (1.0f / (1.0f + 0.25f * preGain));
            }

            case SaturationMode::Warm:
            {
                const float absX = std::abs(driven);
                return (driven / (1.0f + absX)) * (1.0f + 0.4f / preGain);
            }
        }
        return x;
    }

    float SaturationDistortion::shapeDist(float x, DistortionMode mode, float preGain) noexcept
    {
        const float driven = x * preGain;
        switch (mode)
        {
            case DistortionMode::HardClip:
                return DspMath::clamp(driven, -0.85f, 0.85f) * 0.95f;

            case DistortionMode::Foldback:
            {
                float v = driven;
                if (v > 1.0f || v < -1.0f)
                {
                    v = std::fmod(v - 1.0f, 4.0f);
                    if (v < 0.0f) v += 4.0f;
                    v = std::abs(v - 2.0f) - 1.0f;
                }
                return v * 0.8f;
            }

            case DistortionMode::Crunch:
            {
                // Quantized soft-clip for gritty urban lo-fi punch
                const float steps = 24.0f;
                const float q = std::round(driven * steps) / steps;
                return std::tanh(q * 1.4f);
            }
        }
        return x;
    }

    void SaturationDistortion::processBlock(float* left, float* right, int numSamples) noexcept
    {
        if (sat_.enabled && sat_.mix > 0.001f)
        {
            const float preGain = 1.0f + DspMath::clamp(sat_.drive, 0.0f, 1.0f) * 7.0f;
            const float wetMix  = DspMath::clamp(sat_.mix, 0.0f, 1.0f);
            const float dryMix  = 1.0f - wetMix;

            for (int i = 0; i < numSamples; ++i)
            {
                const float dryL = left[i];
                const float dryR = right[i];

                float wetL = dcSatL_.process(shapeSat(dryL, sat_.mode, preGain));
                float wetR = dcSatR_.process(shapeSat(dryR, sat_.mode, preGain));
                satToneFilter_.processStereo(wetL, wetR);

                left[i]  = dryL * dryMix + wetL * wetMix;
                right[i] = dryR * dryMix + wetR * wetMix;
            }
        }

        if (dist_.enabled && dist_.mix > 0.001f)
        {
            const float preGain = 1.0f + DspMath::clamp(dist_.drive, 0.0f, 1.0f) * 14.0f;
            const float wetMix  = DspMath::clamp(dist_.mix, 0.0f, 1.0f);
            const float dryMix  = 1.0f - wetMix;

            for (int i = 0; i < numSamples; ++i)
            {
                const float dryL = left[i];
                const float dryR = right[i];

                float wetL = dcDistL_.process(shapeDist(dryL, dist_.mode, preGain));
                float wetR = dcDistR_.process(shapeDist(dryR, dist_.mode, preGain));
                distToneFilter_.processStereo(wetL, wetR);

                left[i]  = dryL * dryMix + wetL * wetMix;
                right[i] = dryR * dryMix + wetR * wetMix;
            }
        }
    }
}
