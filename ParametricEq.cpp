#include "DSP/Effects/ParametricEq.h"
#include "DSP/Utilities/DspMath.h"
#include <cmath>
#include <complex>

namespace cray
{
    void ParametricEq::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        reset();
        setParameters(params_);
    }

    void ParametricEq::reset() noexcept
    {
        for (auto& b : bands_)
            b.reset();
    }

    float ParametricEq::Biquad::magnitudeAt(float omega) const noexcept
    {
        const std::complex<float> z1(std::cos(-omega), std::sin(-omega));
        const std::complex<float> z2(std::cos(-2.0f * omega), std::sin(-2.0f * omega));
        const std::complex<float> num = b0 + b1 * z1 + b2 * z2;
        const std::complex<float> den = 1.0f + a1 * z1 + a2 * z2;
        const float mag = std::abs(num) / std::max(1.0e-6f, std::abs(den));
        return mag;
    }

    void ParametricEq::makeLowShelf(Biquad& bq, double sr, float freqHz, float gainDb, float slope) noexcept
    {
        const float fc = DspMath::clamp(freqHz, 20.0f, static_cast<float>(sr * 0.45));
        const float A  = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = DspMath::kTwoPi * fc / static_cast<float>(sr);
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 * 0.5f * std::sqrt((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;

        const float a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha;
        bq.b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha)) / a0;
        bq.b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0)) / a0;
        bq.b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha)) / a0;
        bq.a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0)) / a0;
        bq.a2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha) / a0;
    }

    void ParametricEq::makeHighShelf(Biquad& bq, double sr, float freqHz, float gainDb, float slope) noexcept
    {
        const float fc = DspMath::clamp(freqHz, 20.0f, static_cast<float>(sr * 0.45));
        const float A  = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = DspMath::kTwoPi * fc / static_cast<float>(sr);
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 * 0.5f * std::sqrt((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;

        const float a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha;
        bq.b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha)) / a0;
        bq.b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0)) / a0;
        bq.b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha)) / a0;
        bq.a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0)) / a0;
        bq.a2 = ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha) / a0;
    }

    void ParametricEq::makePeak(Biquad& bq, double sr, float freqHz, float q, float gainDb) noexcept
    {
        const float fc = DspMath::clamp(freqHz, 20.0f, static_cast<float>(sr * 0.45));
        const float A  = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = DspMath::kTwoPi * fc / static_cast<float>(sr);
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 / (2.0f * DspMath::clamp(q, 0.2f, 12.0f));

        const float a0 = 1.0f + alpha / A;
        bq.b0 = (1.0f + alpha * A) / a0;
        bq.b1 = (-2.0f * cosW0) / a0;
        bq.b2 = (1.0f - alpha * A) / a0;
        bq.a1 = (-2.0f * cosW0) / a0;
        bq.a2 = (1.0f - alpha / A) / a0;
    }

    void ParametricEq::setParameters(const Params& params) noexcept
    {
        params_ = params;
        makeLowShelf(bands_[0], sampleRate_, params_.lowFreqHz, params_.lowGainDb);
        makePeak(bands_[1], sampleRate_, params_.mid1FreqHz, params_.mid1Q, params_.mid1GainDb);
        makePeak(bands_[2], sampleRate_, params_.mid2FreqHz, params_.mid2Q, params_.mid2GainDb);
        makeHighShelf(bands_[3], sampleRate_, params_.highFreqHz, params_.highGainDb);
    }

    void ParametricEq::processBlock(float* left, float* right, int numSamples) noexcept
    {
        if (!params_.enabled)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right[i];

            bands_[0].processStereo(l, r);
            bands_[1].processStereo(l, r);
            bands_[2].processStereo(l, r);
            bands_[3].processStereo(l, r);

            left[i]  = DspMath::flushDenormal(l);
            right[i] = DspMath::flushDenormal(r);
        }
    }

    float ParametricEq::getMagnitudeDbAt(float freqHz) const noexcept
    {
        if (!params_.enabled)
            return 0.0f;

        const float omega = DspMath::kTwoPi * freqHz / static_cast<float>(sampleRate_);
        float totalMag = 1.0f;
        for (const auto& b : bands_)
            totalMag *= b.magnitudeAt(omega);

        return DspMath::gainToDb(totalMag);
    }
}
