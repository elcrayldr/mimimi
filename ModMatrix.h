#pragma once

#include "Common/ProductInfo.h"
#include <array>

namespace cray
{
    enum class ModSource : int
    {
        None = 0,
        Lfo1,
        Lfo2,
        AmpEnv,
        FilterEnv,
        ModWheel,
        Velocity,
        KeyTrack,
        PitchBend,
        Aftertouch,
        Count
    };

    enum class ModDest : int
    {
        None = 0,
        MasterPitch,
        Osc1Pitch,
        Osc2Pitch,
        Osc3Pitch,
        Osc4Pitch,
        Osc1Level,
        Osc2Level,
        Osc3Level,
        Osc4Level,
        FilterCutoff,
        FilterResonance,
        AmpLevel,
        Pan,
        Lfo1Rate,
        Lfo2Rate,
        SatDrive,
        DelayMix,
        ReverbMix,
        Count
    };

    struct ModSlot
    {
        ModSource source = ModSource::None;
        ModDest   dest   = ModDest::None;
        float     amount = 0.0f; // [-1, +1]
    };

    struct ModInputContext
    {
        float lfo1       = 0.0f; // [-1, +1]
        float lfo2       = 0.0f; // [-1, +1]
        float ampEnv     = 0.0f; // [0, 1]
        float filterEnv  = 0.0f; // [0, 1]
        float modWheel   = 0.0f; // [0, 1]
        float velocity   = 0.0f; // [0, 1]
        float keyTrack   = 0.0f; // [-1, +1] centered at MIDI 60
        float pitchBend  = 0.0f; // [-1, +1]
        float aftertouch = 0.0f; // [0, 1]
    };

    using ModOffsets = std::array<float, static_cast<size_t>(ModDest::Count)>;

    /**
     * Real-time 8-slot Modulation Matrix evaluator.
     */
    class ModMatrix
    {
    public:
        void setSlot(int index, ModSource src, ModDest dst, float amountMinus1To1) noexcept;
        const ModSlot& getSlot(int index) const noexcept { return slots_[static_cast<size_t>(index)]; }

        void evaluate(const ModInputContext& ctx, ModOffsets& outOffsets) const noexcept;

        static const char* sourceName(ModSource src) noexcept;
        static const char* destName(ModDest dst) noexcept;

    private:
        static float readSource(ModSource src, const ModInputContext& ctx) noexcept;

        std::array<ModSlot, ProductInfo::kNumModSlots> slots_{};
    };
}
