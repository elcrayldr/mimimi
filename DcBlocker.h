#pragma once

#include <cmath>

namespace cray
{
    /**
     * 1st-order DC blocking high-pass filter (~5-10 Hz) to remove sub-audible DC offsets
     * after asymmetric saturation or wave-shaping.
     */
    class DcBlocker
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            const double fc = 7.0;
            const double w = 2.0 * 3.14159265358979323846 * fc / ((sampleRate > 1.0) ? sampleRate : 44100.0);
            r_ = static_cast<float>(1.0 - w);
            if (r_ < 0.9f) r_ = 0.9f;
            if (r_ > 0.9999f) r_ = 0.9999f;
            reset();
        }

        void reset() noexcept
        {
            x1_ = 0.0f;
            y1_ = 0.0f;
        }

        float process(float x) noexcept
        {
            const float y = x - x1_ + r_ * y1_;
            x1_ = x;
            y1_ = (std::abs(y) < 1.0e-20f) ? 0.0f : y;
            return y1_;
        }

    private:
        float r_  = 0.9985f;
        float x1_ = 0.0f;
        float y1_ = 0.0f;
    };
}
