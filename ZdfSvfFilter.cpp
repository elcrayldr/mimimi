#include "DSP/Filters/ZdfSvfFilter.h"
#include "DSP/Utilities/DspMath.h"
#include <cmath>

namespace cray
{
    void ZdfSvfFilter::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        reset();
    }

    void ZdfSvfFilter::reset() noexcept
    {
        leftState_  = {};
        rightState_ = {};
    }

    void ZdfSvfFilter::setParameters(SvfFilterType type, float cutoffHz, float resonance01) noexcept
    {
        type_ = type;
        const float maxHz = static_cast<float>(sampleRate_ * 0.45);
        const float fc    = DspMath::clamp(cutoffHz, 20.0f, maxHz);
        const float res   = DspMath::clamp(resonance01, 0.0f, 0.96f);

        g_  = std::tan(DspMath::kPi * fc / static_cast<float>(sampleRate_));
        // Q goes from ~0.707 (flat) up to ~16 (musical self-oscillation edge without instability)
        k_  = 2.0f * (1.0f - res * 0.95f);
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }

    float ZdfSvfFilter::processSingleStage(float v0, ChannelState& st, bool secondStage) noexcept
    {
        float& ic1 = secondStage ? st.ic1eq2 : st.ic1eq;
        float& ic2 = secondStage ? st.ic2eq2 : st.ic2eq;

        const float v3 = v0 - ic2;
        const float v1 = a1_ * ic1 + a2_ * v3;
        const float v2 = ic2 + a2_ * ic1 + a3_ * v3;

        // Soft-saturate feedback state gently to keep resonance musical and bounded
        ic1 = DspMath::flushDenormal(2.0f * v1 - ic1);
        ic2 = DspMath::flushDenormal(2.0f * v2 - ic2);

        switch (type_)
        {
            case SvfFilterType::LowPass12:
            case SvfFilterType::LowPass24:
                return v2;
            case SvfFilterType::BandPass12:
                return v1;
            case SvfFilterType::HighPass12:
                return v0 - k_ * v1 - v2;
        }
        return v2;
    }

    void ZdfSvfFilter::processStereo(float& left, float& right) noexcept
    {
        left  = processSingleStage(left, leftState_, false);
        right = processSingleStage(right, rightState_, false);

        if (type_ == SvfFilterType::LowPass24)
        {
            left  = processSingleStage(left, leftState_, true);
            right = processSingleStage(right, rightState_, true);
        }
    }
}
