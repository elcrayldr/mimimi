#include "DSP/Oscillators/UnisonOscillatorBank.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void UnisonOscillatorBank::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        for (auto& v : voices_)
            v.prepare(sampleRate_);
    }

    void UnisonOscillatorBank::reset(uint32_t seedBase, bool randomizePhases) noexcept
    {
        uint32_t rng = (seedBase != 0u) ? seedBase : 0xA5A55A5Au;
        for (int i = 0; i < ProductInfo::kMaxUnisonVoices; ++i)
        {
            float phase = 0.0f;
            if (randomizePhases && i > 0)
            {
                // Deterministic golden-ratio style spread + PRNG jitter for fat non-phasing unison
                const float noise = DspMath::nextWhiteNoise(rng);
                phase = 0.5f * (noise + 1.0f);
            }
            voices_[static_cast<size_t>(i)].reset(phase, rng ^ static_cast<uint32_t>(i * 7919 + 17));
        }
    }

    void UnisonOscillatorBank::setParameters(OscWaveform waveform,
                                             float baseFreqHz,
                                             int unisonVoices,
                                             float detuneAmount01,
                                             float stereoSpread01,
                                             float oscPanMinus1To1) noexcept
    {
        activeCount_ = std::clamp(unisonVoices, 1, ProductInfo::kMaxUnisonVoices);
        const float detuneMaxCents = DspMath::clamp(detuneAmount01, 0.0f, 1.0f) * 50.0f; // up to +/- 50 cents
        const float spread = DspMath::clamp(stereoSpread01, 0.0f, 1.0f);

        // Energy-preserving normalization (~1/sqrt(N)) so stacking unison doesn't clip
        normGain_ = 1.0f / std::sqrt(static_cast<float>(activeCount_));

        if (activeCount_ == 1)
        {
            auto& v = voices_[0];
            v.setWaveform(waveform);
            v.setFrequency(baseFreqHz);
            DspMath::equalPowerPan(oscPanMinus1To1, layout_[0].panLeft, layout_[0].panRight);
            return;
        }

        for (int i = 0; i < activeCount_; ++i)
        {
            const float normPos = (2.0f * static_cast<float>(i) / static_cast<float>(activeCount_ - 1)) - 1.0f; // [-1, 1]
            const float cents   = normPos * detuneMaxCents;
            const float ratio   = std::pow(2.0f, cents / 1200.0f);

            // Alternate pan polarity for symmetric stereo image
            const float panOffset = (i % 2 == 0 ? -1.0f : 1.0f) * std::abs(normPos) * spread;
            const float totalPan  = DspMath::clamp(oscPanMinus1To1 + panOffset, -1.0f, 1.0f);

            auto& v = voices_[static_cast<size_t>(i)];
            v.setWaveform(waveform);
            v.setFrequency(baseFreqHz * ratio);

            auto& lay = layout_[static_cast<size_t>(i)];
            lay.detuneRatio = ratio;
            DspMath::equalPowerPan(totalPan, lay.panLeft, lay.panRight);
        }
    }

    void UnisonOscillatorBank::processSample(float& outLeft, float& outRight) noexcept
    {
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (int i = 0; i < activeCount_; ++i)
        {
            const float s = voices_[static_cast<size_t>(i)].processSample();
            const auto& lay = layout_[static_cast<size_t>(i)];
            sumL += s * lay.panLeft;
            sumR += s * lay.panRight;
        }

        outLeft  = sumL * normGain_;
        outRight = sumR * normGain_;
    }
}
