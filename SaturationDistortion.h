#pragma once

#include "DSP/Utilities/DcBlocker.h"
#include "DSP/Filters/ZdfSvfFilter.h"

namespace cray
{
    enum class SaturationMode : int
    {
        Tape = 0,
        Tube,
        Warm
    };

    enum class DistortionMode : int
    {
        HardClip = 0,
        Foldback,
        Crunch
    };

    /**
     * Dual-stage Saturation + Distortion processor with DC blocking and tone shaping.
     */
    class SaturationDistortion
    {
    public:
        struct SatParams
        {
            bool           enabled = false;
            SaturationMode mode    = SaturationMode::Tape;
            float          drive   = 0.25f; // [0..1]
            float          tone    = 0.65f; // [0..1]
            float          mix     = 0.5f;  // [0..1]
        };

        struct DistParams
        {
            bool           enabled = false;
            DistortionMode mode    = DistortionMode::HardClip;
            float          drive   = 0.30f; // [0..1]
            float          tone    = 0.60f; // [0..1]
            float          mix     = 0.4f;  // [0..1]
        };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setParameters(const SatParams& sat, const DistParams& dist) noexcept;
        void processBlock(float* left, float* right, int numSamples) noexcept;

    private:
        float shapeSat(float x, SaturationMode mode, float preGain) noexcept;
        float shapeDist(float x, DistortionMode mode, float preGain) noexcept;

        double       sampleRate_ = 44100.0;
        SatParams    sat_{};
        DistParams   dist_{};

        DcBlocker    dcSatL_, dcSatR_;
        DcBlocker    dcDistL_, dcDistR_;
        ZdfSvfFilter satToneFilter_;
        ZdfSvfFilter distToneFilter_;
    };
}
