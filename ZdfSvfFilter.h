#pragma once

namespace cray
{
    enum class SvfFilterType : int
    {
        LowPass12 = 0,
        LowPass24,
        HighPass12,
        BandPass12
    };

    /**
     * Zero-Delay Feedback (TPT) State Variable Filter.
     * Unconditionally stable under rapid cutoff & resonance modulation.
     */
    class ZdfSvfFilter
    {
    public:
        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setParameters(SvfFilterType type, float cutoffHz, float resonance01) noexcept;
        void processStereo(float& left, float& right) noexcept;

    private:
        struct ChannelState
        {
            float ic1eq = 0.0f;
            float ic2eq = 0.0f;
            float ic1eq2 = 0.0f;
            float ic2eq2 = 0.0f;
        };

        float processSingleStage(float v0, ChannelState& st, bool secondStage) noexcept;

        double        sampleRate_ = 44100.0;
        SvfFilterType type_       = SvfFilterType::LowPass12;
        float         g_          = 0.5f;
        float         k_          = 1.0f;
        float         a1_         = 0.5f;
        float         a2_         = 0.25f;
        float         a3_         = 0.125f;

        ChannelState  leftState_{};
        ChannelState  rightState_{};
    };
}
