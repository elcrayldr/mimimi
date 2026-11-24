#include "DSP/Effects/EffectsChain.h"
#include "Common/ParameterIDs.h"
#include "DSP/Utilities/DspMath.h"

namespace cray
{
    namespace
    {
        float loadParam(const juce::AudioProcessorValueTreeState& apvts, const char* id, float fallback = 0.0f) noexcept
        {
            if (auto* p = apvts.getRawParameterValue(id))
                return p->load(std::memory_order_relaxed);
            return fallback;
        }
    }

    void EffectsChain::prepare(double sampleRate) noexcept
    {
        eq_.prepare(sampleRate);
        satDist_.prepare(sampleRate);
        chorus_.prepare(sampleRate);
        delay_.prepare(sampleRate);
        reverb_.prepare(sampleRate);
    }

    void EffectsChain::reset() noexcept
    {
        eq_.reset();
        satDist_.reset();
        chorus_.reset();
        delay_.reset();
        reverb_.reset();
    }

    void EffectsChain::updateFromParameters(const juce::AudioProcessorValueTreeState& apvts,
                                            double hostBpm,
                                            const ModOffsets& modOffsets) noexcept
    {
        ParametricEq::Params eqP;
        eqP.enabled    = loadParam(apvts, ParamIDs::eqEnabled, 1.0f) >= 0.5f;
        eqP.lowGainDb  = loadParam(apvts, ParamIDs::eqLowGainDb, 0.0f);
        eqP.lowFreqHz  = loadParam(apvts, ParamIDs::eqLowFreq, 100.0f);
        eqP.mid1GainDb = loadParam(apvts, ParamIDs::eqMid1GainDb, 0.0f);
        eqP.mid1FreqHz = loadParam(apvts, ParamIDs::eqMid1Freq, 500.0f);
        eqP.mid1Q      = loadParam(apvts, ParamIDs::eqMid1Q, 1.0f);
        eqP.mid2GainDb = loadParam(apvts, ParamIDs::eqMid2GainDb, 0.0f);
        eqP.mid2FreqHz = loadParam(apvts, ParamIDs::eqMid2Freq, 2500.0f);
        eqP.mid2Q      = loadParam(apvts, ParamIDs::eqMid2Q, 1.0f);
        eqP.highGainDb = loadParam(apvts, ParamIDs::eqHighGainDb, 0.0f);
        eqP.highFreqHz = loadParam(apvts, ParamIDs::eqHighFreq, 8000.0f);
        eq_.setParameters(eqP);

        SaturationDistortion::SatParams satP;
        satP.enabled = loadParam(apvts, ParamIDs::satEnabled, 0.0f) >= 0.5f;
        satP.mode    = static_cast<SaturationMode>(static_cast<int>(loadParam(apvts, ParamIDs::satMode, 0.0f)));
        satP.drive   = DspMath::clamp(loadParam(apvts, ParamIDs::satDrive, 0.25f)
                                      + modOffsets[static_cast<size_t>(ModDest::SatDrive)], 0.0f, 1.0f);
        satP.tone    = loadParam(apvts, ParamIDs::satTone, 0.65f);
        satP.mix     = loadParam(apvts, ParamIDs::satMix, 0.5f);

        SaturationDistortion::DistParams distP;
        distP.enabled = loadParam(apvts, ParamIDs::distEnabled, 0.0f) >= 0.5f;
        distP.mode    = static_cast<DistortionMode>(static_cast<int>(loadParam(apvts, ParamIDs::distMode, 0.0f)));
        distP.drive   = loadParam(apvts, ParamIDs::distDrive, 0.30f);
        distP.tone    = loadParam(apvts, ParamIDs::distTone, 0.60f);
        distP.mix     = loadParam(apvts, ParamIDs::distMix, 0.40f);
        satDist_.setParameters(satP, distP);

        StereoChorus::Params chP;
        chP.enabled  = loadParam(apvts, ParamIDs::chorusEnabled, 0.0f) >= 0.5f;
        chP.rateHz   = loadParam(apvts, ParamIDs::chorusRate, 0.8f);
        chP.depth    = loadParam(apvts, ParamIDs::chorusDepth, 0.45f);
        chP.feedback = loadParam(apvts, ParamIDs::chorusFeedback, 0.2f);
        chP.mix      = loadParam(apvts, ParamIDs::chorusMix, 0.35f);
        chorus_.setParameters(chP);

        StereoDelay::Params delP;
        delP.enabled   = loadParam(apvts, ParamIDs::delayEnabled, 0.0f) >= 0.5f;
        delP.tempoSync = loadParam(apvts, ParamIDs::delaySync, 1.0f) >= 0.5f;
        delP.timeMs    = loadParam(apvts, ParamIDs::delayTimeMs, 375.0f);
        delP.syncDiv   = static_cast<TempoSyncDivision>(static_cast<int>(loadParam(apvts, ParamIDs::delaySyncDiv, 8.0f)));
        delP.hostBpm   = hostBpm;
        delP.feedback  = loadParam(apvts, ParamIDs::delayFeedback, 0.35f);
        delP.highCutHz = loadParam(apvts, ParamIDs::delayHighCut, 6500.0f);
        delP.pingPong  = loadParam(apvts, ParamIDs::delayPingPong, 1.0f) >= 0.5f;
        delP.mix       = DspMath::clamp(loadParam(apvts, ParamIDs::delayMix, 0.25f)
                                        + modOffsets[static_cast<size_t>(ModDest::DelayMix)], 0.0f, 1.0f);
        delay_.setParameters(delP);

        AlgorithmicReverb::Params revP;
        revP.enabled    = loadParam(apvts, ParamIDs::reverbEnabled, 0.0f) >= 0.5f;
        revP.preDelayMs = loadParam(apvts, ParamIDs::reverbPreDelayMs, 18.0f);
        revP.size       = loadParam(apvts, ParamIDs::reverbSize, 0.65f);
        revP.damping    = loadParam(apvts, ParamIDs::reverbDamping, 0.45f);
        revP.width      = loadParam(apvts, ParamIDs::reverbWidth, 0.85f);
        revP.mix        = DspMath::clamp(loadParam(apvts, ParamIDs::reverbMix, 0.25f)
                                         + modOffsets[static_cast<size_t>(ModDest::ReverbMix)], 0.0f, 1.0f);
        reverb_.setParameters(revP);
    }

    void EffectsChain::processBlock(float* left, float* right, int numSamples) noexcept
    {
        eq_.processBlock(left, right, numSamples);
        satDist_.processBlock(left, right, numSamples);
        chorus_.processBlock(left, right, numSamples);
        delay_.processBlock(left, right, numSamples);
        reverb_.processBlock(left, right, numSamples);

        // Transparent protective master safety limiter (prevents > +3 dBFS spikes)
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::abs(left[i]) > 0.98f)
                left[i] = 0.98f * std::tanh(left[i] / 0.98f);
            if (std::abs(right[i]) > 0.98f)
                right[i] = 0.98f * std::tanh(right[i] / 0.98f);
        }
    }
}
