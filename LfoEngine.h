#pragma once

#include <cstdint>

namespace cray
{
    enum class LfoWaveform : int
    {
        Sine = 0,
        Triangle,
        SawUp,
        SawDown,
        Square,
        SampleAndHold
    };

    enum class TempoSyncDivision : int
    {
        Whole_1_1 = 0,
        Half_1_2,
        HalfDot_1_2D,
        HalfTrip_1_2T,
        Quarter_1_4,
        QuarterDot_1_4D,
        QuarterTrip_1_4T,
        Eighth_1_8,
        EighthDot_1_8D,
        EighthTrip_1_8T,
        Sixteenth_1_16,
        ThirtySecond_1_32
    };

    /**
     * Tempo-syncable LFO supporting Sine, Triangle, Saw Up/Down, Square, and S&H.
     * Outputs bipolar [-1, +1] scaled by depth.
     */
    class LfoEngine
    {
    public:
        void prepare(double sampleRate) noexcept;
        void reset(float initialPhase = 0.0f) noexcept;
        void retrigger() noexcept;

        void setParameters(LfoWaveform waveform,
                           float rateHz,
                           bool tempoSync,
                           TempoSyncDivision division,
                           double hostBpm,
                           float depth01,
                           bool retriggerEnabled) noexcept;

        float processSample() noexcept;
        float getCurrentValue() const noexcept { return currentValue_; }

        static float divisionToBeats(TempoSyncDivision div) noexcept;

    private:
        double            sampleRate_       = 44100.0;
        LfoWaveform       waveform_         = LfoWaveform::Sine;
        float             phase_            = 0.0f;
        float             phaseInc_         = 0.0001f;
        float             depth_            = 1.0f;
        bool              retriggerEnabled_ = false;
        float             currentValue_     = 0.0f;
        float             shHeldValue_      = 0.0f;
        uint32_t          rngState_         = 0x67452301u;
    };
}
