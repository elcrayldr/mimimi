#pragma once

#include "DSP/Modulation/LfoEngine.h"
#include "DSP/Filters/ZdfSvfFilter.h"
#include "DSP/Utilities/ParameterSmoother.h"
#include <vector>

namespace cray
{
    /**
     * Stereo / Ping-Pong Delay with DAW Tempo Sync, High-Cut feedback damping, and smooth delay time slew.
     */
    class StereoDelay
    {
    public:
        struct Params
        {
            bool              enabled    = false;
            bool              tempoSync  = true;
            float             timeMs     = 375.0f;
            TempoSyncDivision syncDiv    = TempoSyncDivision::EighthDot_1_8D;
            double            hostBpm    = 120.0;
            float             feedback   = 0.35f;
            float             highCutHz  = 6500.0f;
            bool              pingPong   = true;
            float             mix        = 0.25f;
        };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setParameters(const Params& params) noexcept;
        void processBlock(float* left, float* right, int numSamples) noexcept;

    private:
        float readInterpolated(const std::vector<float>& buf, float delaySamples) const noexcept;

        double             sampleRate_ = 44100.0;
        Params             params_{};
        std::vector<float> bufferL_;
        std::vector<float> bufferR_;
        int                writeIdx_   = 0;

        OnePoleSmoother    delaySamplesSmoother_;
        ZdfSvfFilter       fbLowPass_;
    };
}
