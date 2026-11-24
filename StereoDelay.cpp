#include "DSP/Effects/StereoDelay.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void StereoDelay::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        // Max 2.5 seconds delay buffer
        const size_t maxSamples = static_cast<size_t>(sampleRate_ * 2.5) + 16;
        bufferL_.assign(maxSamples, 0.0f);
        bufferR_.assign(maxSamples, 0.0f);
        writeIdx_ = 0;

        delaySamplesSmoother_.prepare(sampleRate_, 0.06);
        delaySamplesSmoother_.reset(static_cast<float>(sampleRate_ * 0.375));
        fbLowPass_.prepare(sampleRate_);
        reset();
    }

    void StereoDelay::reset() noexcept
    {
        std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
        std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
        writeIdx_ = 0;
        fbLowPass_.reset();
    }

    void StereoDelay::setParameters(const Params& params) noexcept
    {
        params_ = params;

        float targetMs = DspMath::clamp(params_.timeMs, 10.0f, 2200.0f);
        if (params_.tempoSync)
        {
            const double bpm   = DspMath::clamp(params_.hostBpm, 20.0, 300.0);
            const float  beats = LfoEngine::divisionToBeats(params_.syncDiv);
            targetMs = static_cast<float>((60000.0 / bpm) * static_cast<double>(beats));
            targetMs = DspMath::clamp(targetMs, 10.0f, 2200.0f);
        }

        delaySamplesSmoother_.setTarget(targetMs * 0.001f * static_cast<float>(sampleRate_));
        fbLowPass_.setParameters(SvfFilterType::LowPass12, DspMath::clamp(params_.highCutHz, 400.0f, 18000.0f), 0.05f);
    }

    float StereoDelay::readInterpolated(const std::vector<float>& buf, float delaySamples) const noexcept
    {
        const int size = static_cast<int>(buf.size());
        if (size <= 4)
            return 0.0f;

        const float clampedDelay = DspMath::clamp(delaySamples, 1.0f, static_cast<float>(size - 4));
        float readPos = static_cast<float>(writeIdx_) - clampedDelay;
        while (readPos < 0.0f)
            readPos += static_cast<float>(size);

        const int i0 = static_cast<int>(readPos) % size;
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - std::floor(readPos);
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    }

    void StereoDelay::processBlock(float* left, float* right, int numSamples) noexcept
    {
        if (!params_.enabled || bufferL_.empty())
            return;

        const float fb     = DspMath::clamp(params_.feedback, 0.0f, 0.92f);
        const float wetMix = DspMath::clamp(params_.mix, 0.0f, 1.0f);
        const int   size   = static_cast<int>(bufferL_.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const float delaySamples = delaySamplesSmoother_.getNext();
            const float tapL = readInterpolated(bufferL_, delaySamples);
            const float tapR = readInterpolated(bufferR_, delaySamples);

            float fbL = tapL;
            float fbR = tapR;
            fbLowPass_.processStereo(fbL, fbR);

            const float inL = left[i];
            const float inR = right[i];

            if (params_.pingPong)
            {
                const float monoIn = 0.5f * (inL + inR);
                bufferL_[static_cast<size_t>(writeIdx_)] = DspMath::flushDenormal(std::tanh(monoIn + fbR * fb));
                bufferR_[static_cast<size_t>(writeIdx_)] = DspMath::flushDenormal(std::tanh(fbL * fb));
            }
            else
            {
                bufferL_[static_cast<size_t>(writeIdx_)] = DspMath::flushDenormal(std::tanh(inL + fbL * fb));
                bufferR_[static_cast<size_t>(writeIdx_)] = DspMath::flushDenormal(std::tanh(inR + fbR * fb));
            }

            left[i]  = inL + tapL * wetMix;
            right[i] = inR + tapR * wetMix;

            writeIdx_ = (writeIdx_ + 1) % size;
        }
    }
}
