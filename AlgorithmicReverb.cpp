#include "DSP/Effects/AlgorithmicReverb.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>

namespace cray
{
    namespace
    {
        constexpr int kBaseCombLengths[8]    = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        constexpr int kBaseAllpassLengths[4] = { 556, 441, 341, 225 };
        constexpr int kStereoSpread          = 23;
    }

    void AlgorithmicReverb::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        const double ratio = sampleRate_ / 44100.0;

        const size_t maxPreDelay = static_cast<size_t>(sampleRate_ * 0.25) + 8; // up to 250 ms
        preDelayL_.assign(maxPreDelay, 0.0f);
        preDelayR_.assign(maxPreDelay, 0.0f);
        preDelayWriteIdx_ = 0;

        for (int i = 0; i < kNumCombs; ++i)
        {
            const int lenL = std::max(16, static_cast<int>(kBaseCombLengths[i] * ratio));
            const int lenR = std::max(16, static_cast<int>((kBaseCombLengths[i] + kStereoSpread) * ratio));
            combL_[static_cast<size_t>(i)].resize(lenL);
            combR_[static_cast<size_t>(i)].resize(lenR);
        }

        for (int i = 0; i < kNumAllpasses; ++i)
        {
            const int lenL = std::max(8, static_cast<int>(kBaseAllpassLengths[i] * ratio));
            const int lenR = std::max(8, static_cast<int>((kBaseAllpassLengths[i] + kStereoSpread) * ratio));
            allpassL_[static_cast<size_t>(i)].resize(lenL);
            allpassR_[static_cast<size_t>(i)].resize(lenR);
        }

        setParameters(params_);
    }

    void AlgorithmicReverb::reset() noexcept
    {
        std::fill(preDelayL_.begin(), preDelayL_.end(), 0.0f);
        std::fill(preDelayR_.begin(), preDelayR_.end(), 0.0f);
        preDelayWriteIdx_ = 0;

        for (auto& c : combL_) c.clear();
        for (auto& c : combR_) c.clear();
        for (auto& a : allpassL_) a.clear();
        for (auto& a : allpassR_) a.clear();
    }

    void AlgorithmicReverb::setParameters(const Params& params) noexcept
    {
        params_ = params;
        const float pdMs = DspMath::clamp(params_.preDelayMs, 0.0f, 200.0f);
        const int maxPd = static_cast<int>(preDelayL_.size()) - 2;
        preDelaySamples_ = std::clamp(static_cast<int>(pdMs * 0.001f * static_cast<float>(sampleRate_)), 1, std::max(1, maxPd));
    }

    void AlgorithmicReverb::processBlock(float* left, float* right, int numSamples) noexcept
    {
        if (!params_.enabled || params_.mix <= 0.001f || preDelayL_.empty())
            return;

        const float roomFeedback = 0.70f + DspMath::clamp(params_.size, 0.0f, 1.0f) * 0.27f;
        const float damp1        = DspMath::clamp(params_.damping, 0.0f, 1.0f) * 0.55f;
        const float damp2        = 1.0f - damp1;
        const float width        = DspMath::clamp(params_.width, 0.0f, 1.0f);
        const float wetMix       = DspMath::clamp(params_.mix, 0.0f, 1.0f);
        const float dryMix       = 1.0f - wetMix * 0.4f;

        const float wet1 = wetMix * (width * 0.5f + 0.5f);
        const float wet2 = wetMix * ((1.0f - width) * 0.5f);
        const int pdSize = static_cast<int>(preDelayL_.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = right[i];

            preDelayL_[static_cast<size_t>(preDelayWriteIdx_)] = inL;
            preDelayR_[static_cast<size_t>(preDelayWriteIdx_)] = inR;

            int readIdx = preDelayWriteIdx_ - preDelaySamples_;
            if (readIdx < 0) readIdx += pdSize;

            const float monoInput = (preDelayL_[static_cast<size_t>(readIdx)] +
                                     preDelayR_[static_cast<size_t>(readIdx)]) * 0.015f;

            float outL = 0.0f;
            float outR = 0.0f;

            for (int c = 0; c < kNumCombs; ++c)
            {
                outL += combL_[static_cast<size_t>(c)].process(monoInput, roomFeedback, damp1, damp2);
                outR += combR_[static_cast<size_t>(c)].process(monoInput, roomFeedback, damp1, damp2);
            }

            for (int a = 0; a < kNumAllpasses; ++a)
            {
                outL = allpassL_[static_cast<size_t>(a)].process(outL);
                outR = allpassR_[static_cast<size_t>(a)].process(outR);
            }

            left[i]  = inL * dryMix + outL * wet1 + outR * wet2;
            right[i] = inR * dryMix + outR * wet1 + outL * wet2;

            if (++preDelayWriteIdx_ >= pdSize)
                preDelayWriteIdx_ = 0;
        }
    }
}
