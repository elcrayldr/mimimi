#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace cray
{
    namespace DspMath
    {
        inline constexpr float kPi      = 3.14159265358979323846f;
        inline constexpr float kTwoPi   = 6.28318530717958647692f;
        inline constexpr float kHalfPi  = 1.57079632679489661923f;

        inline float clamp(float x, float lo, float hi) noexcept
        {
            return (x < lo) ? lo : ((x > hi) ? hi : x);
        }

        inline double clamp(double x, double lo, double hi) noexcept
        {
            return (x < lo) ? lo : ((x > hi) ? hi : x);
        }

        inline float dbToGain(float db) noexcept
        {
            return (db <= -96.0f) ? 0.0f : std::pow(10.0f, db * 0.05f);
        }

        inline float gainToDb(float gain) noexcept
        {
            return (gain <= 1.0e-5f) ? -100.0f : 20.0f * std::log10(gain);
        }

        inline float midiNoteToHz(float midiNote) noexcept
        {
            return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
        }

        inline float semitonesToRatio(float semitones) noexcept
        {
            return std::pow(2.0f, semitones / 12.0f);
        }

        inline double semitonesToRatioDouble(double semitones) noexcept
        {
            return std::pow(2.0, semitones / 12.0);
        }

        /** Constant-power stereo pan law (-3 dB at center). */
        inline void equalPowerPan(float panMinus1To1, float& outLeft, float& outRight) noexcept
        {
            const float p = clamp(panMinus1To1, -1.0f, 1.0f);
            const float angle = (p + 1.0f) * 0.5f * kHalfPi;
            outLeft  = std::cos(angle);
            outRight = std::sin(angle);
        }

        /** Fast soft-clipper protecting internal busses from hard digital clipping. */
        inline float softLimit(float x) noexcept
        {
            if (x > 3.0f) return 1.0f;
            if (x < -3.0f) return -1.0f;
            return std::tanh(x);
        }

        /** Flush denormals to zero to avoid CPU spikes on tails. */
        inline float flushDenormal(float x) noexcept
        {
            return (std::abs(x) < 1.0e-20f) ? 0.0f : x;
        }

        /** 4-point Catmull-Rom / cubic Hermite interpolation for sample playback. */
        inline float cubicHermite(float ym1, float y0, float y1, float y2, float t) noexcept
        {
            const float c0 = y0;
            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * t + c2) * t + c1) * t + c0;
        }

        /** Deterministic fast xorshift32 PRNG suitable for per-voice audio-thread noise. */
        inline float nextWhiteNoise(uint32_t& state) noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            // Convert to [-1, 1]
            return static_cast<float>(static_cast<int32_t>(state)) * 4.656612873e-10f;
        }
    }
}
