#include "DSP/Envelopes/AhdsrEnvelope.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void AhdsrEnvelope::prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1.0) ? sampleRate : 44100.0;
        setParameters(attackSec_, decaySec_, sustainLevel_, releaseSec_);
    }

    void AhdsrEnvelope::reset() noexcept
    {
        stage_ = Stage::Idle;
        level_ = 0.0f;
    }

    float AhdsrEnvelope::calcTimeCoeff(float timeSec, double sampleRate, float targetRatio) noexcept
    {
        const float samples = std::max(1.0f, timeSec * static_cast<float>(sampleRate));
        return std::exp(-std::log((1.0f + targetRatio) / targetRatio) / samples);
    }

    void AhdsrEnvelope::setParameters(float attackSec, float decaySec, float sustainLevel, float releaseSec) noexcept
    {
        attackSec_    = std::max(0.0005f, attackSec);
        decaySec_     = std::max(0.001f, decaySec);
        sustainLevel_ = DspMath::clamp(sustainLevel, 0.0f, 1.0f);
        releaseSec_   = std::max(0.001f, releaseSec);

        attackCoeff_  = calcTimeCoeff(attackSec_, sampleRate_, kTargetRatioA);
        attackBase_   = (1.0f + kTargetRatioA) * (1.0f - attackCoeff_);

        decayCoeff_   = calcTimeCoeff(decaySec_, sampleRate_, kTargetRatioDR);
        decayBase_    = (sustainLevel_ - kTargetRatioDR) * (1.0f - decayCoeff_);

        releaseCoeff_ = calcTimeCoeff(releaseSec_, sampleRate_, kTargetRatioDR);
        releaseBase_  = (-kTargetRatioDR) * (1.0f - releaseCoeff_);
    }

    void AhdsrEnvelope::noteOn() noexcept
    {
        stage_ = Stage::Attack;
    }

    void AhdsrEnvelope::noteOff() noexcept
    {
        if (stage_ != Stage::Idle && stage_ != Stage::FastKill)
            stage_ = Stage::Release;
    }

    void AhdsrEnvelope::fastKill(float killTimeSec) noexcept
    {
        if (stage_ == Stage::Idle)
            return;

        const float samples = std::max(1.0f, killTimeSec * static_cast<float>(sampleRate_));
        fastKillStep_ = std::max(level_, 0.0001f) / samples;
        stage_ = Stage::FastKill;
    }

    float AhdsrEnvelope::getNextSample() noexcept
    {
        switch (stage_)
        {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                level_ = attackBase_ + level_ * attackCoeff_;
                if (level_ >= 1.0f)
                {
                    level_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;

            case Stage::Decay:
                level_ = decayBase_ + level_ * decayCoeff_;
                if (level_ <= sustainLevel_ + 1.0e-5f)
                {
                    level_ = sustainLevel_;
                    stage_ = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                level_ = sustainLevel_;
                break;

            case Stage::Release:
                level_ = releaseBase_ + level_ * releaseCoeff_;
                if (level_ <= 1.0e-5f)
                {
                    level_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;

            case Stage::FastKill:
                level_ -= fastKillStep_;
                if (level_ <= 0.0f)
                {
                    level_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
        }

        return level_;
    }
}
