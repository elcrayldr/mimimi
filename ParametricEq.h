#pragma once

#include <array>

namespace cray
{
    /**
     * 4-Band Stereo Parametric Equalizer (Low Shelf, Peak 1, Peak 2, High Shelf)
     * implemented with RBJ Audio EQ Cookbook biquads and magnitude response query for UI display.
     */
    class ParametricEq
    {
    public:
        struct Params
        {
            bool  enabled    = true;
            float lowGainDb  = 0.0f;
            float lowFreqHz  = 100.0f;
            float mid1GainDb = 0.0f;
            float mid1FreqHz = 500.0f;
            float mid1Q      = 1.0f;
            float mid2GainDb = 0.0f;
            float mid2FreqHz = 2500.0f;
            float mid2Q      = 1.0f;
            float highGainDb = 0.0f;
            float highFreqHz = 8000.0f;
        };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setParameters(const Params& params) noexcept;
        void processBlock(float* left, float* right, int numSamples) noexcept;

        /** Compute combined magnitude response in dB at freqHz for the UI EQ curve. */
        float getMagnitudeDbAt(float freqHz) const noexcept;

    private:
        struct Biquad
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            float a1 = 0.0f, a2 = 0.0f;
            float z1L = 0.0f, z2L = 0.0f;
            float z1R = 0.0f, z2R = 0.0f;

            void reset() noexcept { z1L = z2L = z1R = z2R = 0.0f; }

            void processStereo(float& l, float& r) noexcept
            {
                const float outL = b0 * l + z1L;
                z1L = b1 * l - a1 * outL + z2L;
                z2L = b2 * l - a2 * outL;
                l = outL;

                const float outR = b0 * r + z1R;
                z1R = b1 * r - a1 * outR + z2R;
                z2R = b2 * r - a2 * outR;
                r = outR;
            }

            float magnitudeAt(float omega) const noexcept;
        };

        static void makeLowShelf(Biquad& bq, double sr, float freqHz, float gainDb, float slope = 0.8f) noexcept;
        static void makePeak(Biquad& bq, double sr, float freqHz, float q, float gainDb) noexcept;
        static void makeHighShelf(Biquad& bq, double sr, float freqHz, float gainDb, float slope = 0.8f) noexcept;

        double sampleRate_ = 44100.0;
        Params params_{};
        std::array<Biquad, 4> bands_{};
    };
}
