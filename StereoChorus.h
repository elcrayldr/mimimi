#pragma once

#include <vector>

namespace cray
{
    /**
     * Stereo modulated chorus with dual quadrature LFOs and feedback.
     * All buffers pre-allocated in prepare().
     */
    class StereoChorus
    {
    public:
        struct Params
        {
            bool  enabled  = false;
            float rateHz   = 0.8f;
            float depth    = 0.45f;
            float feedback = 0.20f;
            float mix      = 0.35f;
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
        float              lfoPhase_   = 0.0f;
    };
}
