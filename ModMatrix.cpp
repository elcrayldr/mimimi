#include "DSP/Modulation/ModMatrix.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>

namespace cray
{
    void ModMatrix::setSlot(int index, ModSource src, ModDest dst, float amountMinus1To1) noexcept
    {
        if (index < 0 || index >= ProductInfo::kNumModSlots)
            return;

        auto& s = slots_[static_cast<size_t>(index)];
        s.source = src;
        s.dest   = dst;
        s.amount = DspMath::clamp(amountMinus1To1, -1.0f, 1.0f);
    }

    float ModMatrix::readSource(ModSource src, const ModInputContext& ctx) noexcept
    {
        switch (src)
        {
            case ModSource::Lfo1:       return ctx.lfo1;
            case ModSource::Lfo2:       return ctx.lfo2;
            case ModSource::AmpEnv:     return ctx.ampEnv;
            case ModSource::FilterEnv:  return ctx.filterEnv;
            case ModSource::ModWheel:   return ctx.modWheel;
            case ModSource::Velocity:   return ctx.velocity;
            case ModSource::KeyTrack:   return ctx.keyTrack;
            case ModSource::PitchBend:  return ctx.pitchBend;
            case ModSource::Aftertouch: return ctx.aftertouch;
            case ModSource::None:
            case ModSource::Count:
                break;
        }
        return 0.0f;
    }

    void ModMatrix::evaluate(const ModInputContext& ctx, ModOffsets& outOffsets) const noexcept
    {
        outOffsets.fill(0.0f);

        for (const auto& slot : slots_)
        {
            if (slot.source == ModSource::None || slot.dest == ModDest::None || slot.amount == 0.0f)
                continue;

            const size_t dstIdx = static_cast<size_t>(slot.dest);
            if (dstIdx < outOffsets.size())
                outOffsets[dstIdx] += readSource(slot.source, ctx) * slot.amount;
        }
    }

    const char* ModMatrix::sourceName(ModSource src) noexcept
    {
        switch (src)
        {
            case ModSource::None:       return "Off";
            case ModSource::Lfo1:       return "LFO 1";
            case ModSource::Lfo2:       return "LFO 2";
            case ModSource::AmpEnv:     return "Amp Env";
            case ModSource::FilterEnv:  return "Filter Env";
            case ModSource::ModWheel:   return "Mod Wheel";
            case ModSource::Velocity:   return "Velocity";
            case ModSource::KeyTrack:   return "Key Track";
            case ModSource::PitchBend:  return "Pitch Bend";
            case ModSource::Aftertouch: return "Aftertouch";
            case ModSource::Count:      return "Off";
        }
        return "Off";
    }

    const char* ModMatrix::destName(ModDest dst) noexcept
    {
        switch (dst)
        {
            case ModDest::None:            return "Off";
            case ModDest::MasterPitch:     return "Master Pitch";
            case ModDest::Osc1Pitch:       return "Osc 1 Pitch";
            case ModDest::Osc2Pitch:       return "Osc 2 Pitch";
            case ModDest::Osc3Pitch:       return "Osc 3 Pitch";
            case ModDest::Osc4Pitch:       return "Osc 4 Pitch";
            case ModDest::Osc1Level:       return "Osc 1 Level";
            case ModDest::Osc2Level:       return "Osc 2 Level";
            case ModDest::Osc3Level:       return "Osc 3 Level";
            case ModDest::Osc4Level:       return "Osc 4 Level";
            case ModDest::FilterCutoff:    return "Filter Cutoff";
            case ModDest::FilterResonance: return "Filter Reso";
            case ModDest::AmpLevel:        return "Amp Level";
            case ModDest::Pan:             return "Stereo Pan";
            case ModDest::Lfo1Rate:        return "LFO 1 Rate";
            case ModDest::Lfo2Rate:        return "LFO 2 Rate";
            case ModDest::SatDrive:        return "Sat Drive";
            case ModDest::DelayMix:        return "Delay Mix";
            case ModDest::ReverbMix:       return "Reverb Mix";
            case ModDest::Count:           return "Off";
        }
        return "Off";
    }
}
