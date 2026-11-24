#pragma once

#include <cmath>

namespace cray
{
    /**
     * Real-time linear/exponential parameter smoother with zero allocations.
     */
    class OnePoleSmoother
    {
    public:
        void prepare(double sampleRate, double smoothingSeconds = 0.015) noexcept
        {
            sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
            const double samples = std::max(1.0, smoothingSeconds * sampleRate_);
            coeff_ = static_cast<float>(1.0 - std::exp(-1.0 / samples));
        }

        void reset(float value) noexcept
        {
            current_ = value;
            target_  = value;
        }

        void setTarget(float target) noexcept
        {
            target_ = target;
        }

        float getNext() noexcept
        {
            current_ += coeff_ * (target_ - current_);
            if (std::abs(target_ - current_) < 1.0e-7f)
                current_ = target_;
            return current_;
        }

        float getCurrent() const noexcept { return current_; }
        bool  isSmoothing() const noexcept { return current_ != target_; }

    private:
        double sampleRate_ = 44100.0;
        float  coeff_      = 0.01f;
        float  current_    = 0.0f;
        float  target_     = 0.0f;
    };
}
