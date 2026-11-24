#include "DSP/Effects/StereoChorus.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void StereoChorus::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        const size_t bufSize = static_cast<size_t>(sampleRate_ * 0.08) + 16; // up to 80 ms
        bufferL_.assign(bufSize, 0.0f);
        bufferR_.assign(bufSize, 0.0f);
        writeIdx_ = 0;
        lfoPhase_ = 0.0f;
    }

    void StereoChorus::reset() noexcept
    {
        std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
        std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
        writeIdx_ = 0;
        lfoPhase_ = 0.0f;
    }

    void StereoChorus::setParameters(const Params& params) noexcept
    {
        params_ = params;
    }

    float StereoChorus::readInterpolated(const std::vector<float>& buf, float delaySamples) const noexcept
    {
        const int size = static_cast<int>(buf.size());
        if (size <= 4)
            return 0.0f;

        float readPos = static_cast<float>(writeIdx_) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(size);

        const int i0 = static_cast<int>(readPos) % size;
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - std::floor(readPos);
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    }

    void StereoChorus::processBlock(float* left, float* right, int numSamples) noexcept
    {
        if (!params_.enabled || params_.mix <= 0.001f || bufferL_.empty())
            return;

        const float rateHz   = DspMath::clamp(params_.rateHz, 0.05f, 8.0f);
        const float phaseInc = rateHz / static_cast<float>(sampleRate_);
        const float depthMs  = DspMath::clamp(params_.depth, 0.0f, 1.0f) * 8.0f;
        const float fb       = DspMath::clamp(params_.feedback, -0.75f, 0.75f);
        const float wetMix   = DspMath::clamp(params_.mix, 0.0f, 1.0f);
        const float dryMix   = 1.0f - wetMix * 0.5f;

        const float baseDelaySamples  = static_cast<float>(sampleRate_) * 0.014f; // 14 ms center
        const float depthSamples      = static_cast<float>(sampleRate_) * (depthMs * 0.001f);
        const int   bufSize           = static_cast<int>(bufferL_.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const float lfoL = std::sin(DspMath::kTwoPi * lfoPhase_);
            const float lfoR = std::cos(DspMath::kTwoPi * lfoPhase_);

            const float dL = baseDelaySamples + lfoL * depthSamples;
            const float dR = baseDelaySamples + lfoR * depthSamples;

            const float delayedL = readInterpolated(bufferL_, dL);
            const float delayedR = readInterpolated(bufferR_, dR);

            const float inL = left[i];
            const float inR = right[i];

            bufferL_[static_cast<size_t>(writeIdx_)] = DspMath::flushDenormal(inL + delayedR * fb * 0.6f);
            bufferR_[static_cast<size_t>(writeIdx_)] = DspMath::flushDenormal(inR + delayedL * fb * 0.6f);

            left[i]  = inL * dryMix + delayedL * wetMix;
            right[i] = inR * dryMix + delayedR * wetMix;

            writeIdx_ = (writeIdx_ + 1) % bufSize;
            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0f)
                lfoPhase_ -= 1.0f;
        }
    }
}
