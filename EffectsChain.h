#pragma once

#include "DSP/Effects/ParametricEq.h"
#include "DSP/Effects/SaturationDistortion.h"
#include "DSP/Effects/StereoChorus.h"
#include "DSP/Effects/StereoDelay.h"
#include "DSP/Effects/AlgorithmicReverb.h"
#include "DSP/Modulation/ModMatrix.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace cray
{
    /**
     * Master stereo effects rack:
     *   EQ -> Saturation & Distortion -> Stereo Chorus -> Stereo Delay -> Algorithmic Reverb -> Soft Limiter
     */
    class EffectsChain
    {
    public:
        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void updateFromParameters(const juce::AudioProcessorValueTreeState& apvts,
                                  double hostBpm,
                                  const ModOffsets& modOffsets) noexcept;

        void processBlock(float* left, float* right, int numSamples) noexcept;

        const ParametricEq& getEq() const noexcept { return eq_; }

    private:
        ParametricEq         eq_;
        SaturationDistortion satDist_;
        StereoChorus         chorus_;
        StereoDelay          delay_;
        AlgorithmicReverb    reverb_;
    };
}
