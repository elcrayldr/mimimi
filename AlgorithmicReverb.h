#pragma once

#include <array>
#include <vector>

namespace cray
{
    /**
     * Lush Stereo Algorithmic Reverb (Pre-delay + 8 parallel damped comb filters + 4 series allpass diffusers).
     * Scaled dynamically for any sample rate, pre-allocated in prepare(), zero real-time allocations.
     */
    class AlgorithmicReverb
    {
    public:
        struct Params
        {
            bool  enabled    = false;
            float preDelayMs = 18.0f;
            float size       = 0.65f; // [0..1]
            float damping    = 0.45f; // [0..1]
            float width      = 0.85f; // [0..1]
            float mix        = 0.25f; // [0..1]
        };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setParameters(const Params& params) noexcept;
        void processBlock(float* left, float* right, int numSamples) noexcept;

    private:
        struct Comb
        {
            std::vector<float> buffer;
            int   index       = 0;
            float filterStore = 0.0f;

            void resize(int samples) { buffer.assign(static_cast<size_t>(samples), 0.0f); index = 0; filterStore = 0.0f; }
            void clear() noexcept { std::fill(buffer.begin(), buffer.end(), 0.0f); filterStore = 0.0f; }

            float process(float input, float feedback, float damp1, float damp2) noexcept
            {
                const float output = buffer[static_cast<size_t>(index)];
                filterStore = (output * damp2) + (filterStore * damp1);
                if (filterStore > -1.0e-20f && filterStore < 1.0e-20f)
                    filterStore = 0.0f;
                buffer[static_cast<size_t>(index)] = input + filterStore * feedback;
                if (++index >= static_cast<int>(buffer.size()))
                    index = 0;
                return output;
            }
        };

        struct Allpass
        {
            std::vector<float> buffer;
            int index = 0;

            void resize(int samples) { buffer.assign(static_cast<size_t>(samples), 0.0f); index = 0; }
            void clear() noexcept { std::fill(buffer.begin(), buffer.end(), 0.0f); }

            float process(float input) noexcept
            {
                const float bufOut = buffer[static_cast<size_t>(index)];
                const float output = -input + bufOut;
                buffer[static_cast<size_t>(index)] = input + bufOut * 0.5f;
                if (++index >= static_cast<int>(buffer.size()))
                    index = 0;
                return output;
            }
        };

        static constexpr int kNumCombs    = 8;
        static constexpr int kNumAllpasses = 4;

        double sampleRate_ = 44100.0;
        Params params_{};

        std::vector<float> preDelayL_;
        std::vector<float> preDelayR_;
        int                preDelayWriteIdx_ = 0;
        int                preDelaySamples_  = 1;

        std::array<Comb, kNumCombs>       combL_;
        std::array<Comb, kNumCombs>       combR_;
        std::array<Allpass, kNumAllpasses> allpassL_;
        std::array<Allpass, kNumAllpasses> allpassR_;
    };
}
