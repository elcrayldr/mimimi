#include "Engine/SynthEngine.h"
#include "DSP/Utilities/DspMath.h"
#include <cmath>

namespace cray
{
    void SynthVoice::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        for (auto& osc : oscBanks_)
            osc.prepare(sampleRate_);
        ampEnv_.prepare(sampleRate_);
        filterEnv_.prepare(sampleRate_);
        filter_.prepare(sampleRate_);
        reset();
    }

    void SynthVoice::reset() noexcept
    {
        ampEnv_.reset();
        filterEnv_.reset();
        filter_.reset();
        modOffsets_.fill(0.0f);
    }

    void SynthVoice::startNote(int midiNote, float velocity01, float /*currentNoteFloat*/, bool isLegato, uint32_t seed) noexcept
    {
        midiNote_ = midiNote;
        velocity_ = DspMath::clamp(velocity01, 0.01f, 1.0f);

        if (!isLegato)
        {
            for (size_t i = 0; i < oscBanks_.size(); ++i)
                oscBanks_[i].reset(seed ^ static_cast<uint32_t>(i * 31337u + 19u), true);

            filter_.reset();
            ampEnv_.noteOn();
            filterEnv_.noteOn();
        }
    }

    void SynthVoice::releaseNote() noexcept
    {
        ampEnv_.noteOff();
        filterEnv_.noteOff();
    }

    void SynthVoice::fastKill() noexcept
    {
        ampEnv_.fastKill(0.004f);
        filterEnv_.fastKill(0.004f);
    }

    void SynthVoice::updateEnvelopes(const SynthGlobalParams& params) noexcept
    {
        ampEnv_.setParameters(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
        filterEnv_.setParameters(params.filterAttack, params.filterDecay, params.filterSustain, params.filterRelease);
    }

    void SynthVoice::renderSample(const SynthGlobalParams& params,
                                  const ModMatrix& modMatrix,
                                  float currentNoteFloat,
                                  float pitchBendSemis,
                                  float lfo1Val,
                                  float lfo2Val,
                                  float modWheelVal,
                                  float aftertouchVal,
                                  float& outLeft,
                                  float& outRight) noexcept
    {
        if (!ampEnv_.isActive())
        {
            outLeft = 0.0f;
            outRight = 0.0f;
            return;
        }

        const float ampEnvVal    = ampEnv_.getNextSample();
        const float filterEnvVal = filterEnv_.getNextSample();

        ModInputContext modCtx;
        modCtx.lfo1       = lfo1Val;
        modCtx.lfo2       = lfo2Val;
        modCtx.ampEnv     = ampEnvVal;
        modCtx.filterEnv  = filterEnvVal;
        modCtx.modWheel   = modWheelVal;
        modCtx.velocity   = velocity_;
        modCtx.keyTrack   = (currentNoteFloat - 60.0f) / 60.0f;
        modCtx.pitchBend  = pitchBendSemis / 24.0f;
        modCtx.aftertouch = aftertouchVal;

        modMatrix.evaluate(modCtx, modOffsets_);

        const float matrixMasterPitchSemis = modOffsets_[static_cast<size_t>(ModDest::MasterPitch)] * 12.0f;
        const float lfoVibratoSemis        = lfo1Val * params.lfo1ToPitch * 2.0f;
        const float totalBaseNote          = currentNoteFloat + pitchBendSemis + matrixMasterPitchSemis + lfoVibratoSemis;

        float mixL = 0.0f;
        float mixR = 0.0f;

        for (int i = 0; i < ProductInfo::kNumOscillators; ++i)
        {
            const auto& op = params.oscs[static_cast<size_t>(i)];
            if (!op.enabled || op.level <= 0.0005f)
                continue;

            const size_t pitchModIdx = static_cast<size_t>(ModDest::Osc1Pitch) + static_cast<size_t>(i);
            const size_t levelModIdx = static_cast<size_t>(ModDest::Osc1Level) + static_cast<size_t>(i);

            const float oscPitchModSemis = modOffsets_[pitchModIdx] * 12.0f;
            const float noteForOsc       = totalBaseNote + static_cast<float>(op.pitchSemi)
                                           + (op.fineCents * 0.01f) + oscPitchModSemis;
            const float freqHz           = DspMath::midiNoteToHz(noteForOsc);

            auto& bank = oscBanks_[static_cast<size_t>(i)];
            bank.setParameters(op.waveform, freqHz, op.unison, op.detune, op.spread, op.pan);

            float oscL = 0.0f;
            float oscR = 0.0f;
            bank.processSample(oscL, oscR);

            const float oscLevel = DspMath::clamp(op.level + modOffsets_[levelModIdx], 0.0f, 1.5f);
            mixL += oscL * oscLevel;
            mixR += oscR * oscLevel;
        }

        // Normalize headroom gently across 4 stacked oscillators
        mixL *= 0.45f;
        mixR *= 0.45f;

        if (params.filterEnabled)
        {
            const float keyTrackOctaves   = ((currentNoteFloat - 60.0f) / 12.0f) * params.filterKeyTrack;
            const float envOctaves        = filterEnvVal * params.filterEnvAmount * 5.0f;
            const float lfoOctaves        = lfo1Val * params.lfo1ToCutoff * 3.0f;
            const float matrixOctaves     = modOffsets_[static_cast<size_t>(ModDest::FilterCutoff)] * 5.0f;
            const float totalOctaveOffset = keyTrackOctaves + envOctaves + lfoOctaves + matrixOctaves;

            const float modCutoff = params.filterCutoffHz * std::pow(2.0f, totalOctaveOffset);
            const float modRes    = DspMath::clamp(params.filterResonance
                                                   + modOffsets_[static_cast<size_t>(ModDest::FilterResonance)] * 0.5f,
                                                   0.0f, 0.96f);

            filter_.setParameters(params.filterType, modCutoff, modRes);
            filter_.processStereo(mixL, mixR);
        }

        const float velGain = (1.0f - params.velSensitivity) + params.velSensitivity * velocity_;
        const float ampMod  = DspMath::clamp(1.0f + lfo1Val * params.lfo1ToAmp * 0.5f
                                             + modOffsets_[static_cast<size_t>(ModDest::AmpLevel)],
                                             0.0f, 1.5f);
        const float gain = ampEnvVal * velGain * ampMod;

        const float panMod = DspMath::clamp(modOffsets_[static_cast<size_t>(ModDest::Pan)], -1.0f, 1.0f);
        if (std::abs(panMod) > 0.001f)
        {
            float pL = 1.0f, pR = 1.0f;
            DspMath::equalPowerPan(panMod, pL, pR);
            mixL *= pL * 1.414f;
            mixR *= pR * 1.414f;
        }

        outLeft  = mixL * gain;
        outRight = mixR * gain;
    }
}
