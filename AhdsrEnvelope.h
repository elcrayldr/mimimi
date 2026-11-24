#pragma once

namespace cray
{
    /**
     * Musical ADSR envelope with smooth exponential decay/release and click-free retrigger.
     * Zero allocations, real-time safe.
     */
    class AhdsrEnvelope
    {
    public:
        enum class Stage
        {
            Idle = 0,
            Attack,
            Decay,
            Sustain,
            Release,
            FastKill
        };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setParameters(float attackSec, float decaySec, float sustainLevel, float releaseSec) noexcept;

        void noteOn() noexcept;
        void noteOff() noexcept;
        void fastKill(float killTimeSec = 0.004f) noexcept;

        float getNextSample() noexcept;

        bool  isActive() const noexcept { return stage_ != Stage::Idle; }
        bool  isReleasing() const noexcept { return stage_ == Stage::Release || stage_ == Stage::FastKill; }
        float getCurrentLevel() const noexcept { return level_; }
        Stage getStage() const noexcept { return stage_; }

    private:
        static float calcTimeCoeff(float timeSec, double sampleRate, float targetRatio) noexcept;

        double sampleRate_    = 44100.0;
        Stage  stage_         = Stage::Idle;
        float  level_         = 0.0f;

        float  attackSec_     = 0.005f;
        float  decaySec_      = 0.25f;
        float  sustainLevel_  = 0.75f;
        float  releaseSec_    = 0.30f;

        float  attackCoeff_   = 0.0f;
        float  attackBase_    = 0.0f;
        float  decayCoeff_    = 0.0f;
        float  decayBase_     = 0.0f;
        float  releaseCoeff_  = 0.0f;
        float  releaseBase_   = 0.0f;
        float  fastKillStep_  = 0.005f;

        static constexpr float kTargetRatioA = 0.3f;
        static constexpr float kTargetRatioDR = 0.0001f;
    };
}
