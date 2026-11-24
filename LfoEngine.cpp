#include "DSP/Modulation/LfoEngine.h"
#include "DSP/Utilities/DspMath.h"
#include <cmath>

namespace cray
{
    void LfoEngine::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
    }

    void LfoEngine::reset(float initialPhase) noexcept
    {
        phase_ = initialPhase - std::floor(initialPhase);
        currentValue_ = 0.0f;
        shHeldValue_ = DspMath::nextWhiteNoise(rngState_);
    }

    void LfoEngine::retrigger() noexcept
    {
        if (retriggerEnabled_)
        {
            phase_ = 0.0f;
            shHeldValue_ = DspMath::nextWhiteNoise(rngState_);
        }
    }

    float LfoEngine::divisionToBeats(TempoSyncDivision div) noexcept
    {
        switch (div)
        {
            case TempoSyncDivision::Whole_1_1:        return 4.0f;
            case TempoSyncDivision::Half_1_2:         return 2.0f;
            case TempoSyncDivision::HalfDot_1_2D:     return 3.0f;
            case TempoSyncDivision::HalfTrip_1_2T:    return 4.0f / 3.0f;
            case TempoSyncDivision::Quarter_1_4:      return 1.0f;
            case TempoSyncDivision::QuarterDot_1_4D:  return 1.5f;
            case TempoSyncDivision::QuarterTrip_1_4T: return 2.0f / 3.0f;
            case TempoSyncDivision::Eighth_1_8:       return 0.5f;
            case TempoSyncDivision::EighthDot_1_8D:   return 0.75f;
            case TempoSyncDivision::EighthTrip_1_8T:  return 1.0f / 3.0f;
            case TempoSyncDivision::Sixteenth_1_16:   return 0.25f;
            case TempoSyncDivision::ThirtySecond_1_32:return 0.125f;
        }
        return 1.0f;
    }

    void LfoEngine::setParameters(LfoWaveform waveform,
                                  float rateHz,
                                  bool tempoSync,
                                  TempoSyncDivision division,
                                  double hostBpm,
                                  float depth01,
                                  bool retriggerEnabled) noexcept
    {
        waveform_         = waveform;
        depth_            = DspMath::clamp(depth01, 0.0f, 1.0f);
        retriggerEnabled_ = retriggerEnabled;

        float effectiveHz = DspMath::clamp(rateHz, 0.01f, 40.0f);
        if (tempoSync)
        {
            const double bpm   = DspMath::clamp(hostBpm, 20.0, 300.0);
            const float  beats = divisionToBeats(division);
            effectiveHz = static_cast<float>((bpm / 60.0) / static_cast<double>(beats));
        }

        phaseInc_ = effectiveHz / static_cast<float>(sampleRate_);
    }

    float LfoEngine::processSample() noexcept
    {
        float raw = 0.0f;
        switch (waveform_)
        {
            case LfoWaveform::Sine:
                raw = std::sin(DspMath::kTwoPi * phase_);
                break;

            case LfoWaveform::Triangle:
                raw = 1.0f - 4.0f * std::abs(phase_ - 0.5f);
                break;

            case LfoWaveform::SawUp:
                raw = 2.0f * phase_ - 1.0f;
                break;

            case LfoWaveform::SawDown:
                raw = 1.0f - 2.0f * phase_;
                break;

            case LfoWaveform::Square:
                raw = (phase_ < 0.5f) ? 1.0f : -1.0f;
                break;

            case LfoWaveform::SampleAndHold:
                raw = shHeldValue_;
                break;
        }

        phase_ += phaseInc_;
        if (phase_ >= 1.0f)
        {
            phase_ -= std::floor(phase_);
            if (waveform_ == LfoWaveform::SampleAndHold)
                shHeldValue_ = DspMath::nextWhiteNoise(rngState_);
        }

        currentValue_ = raw * depth_;
        return currentValue_;
    }
}
