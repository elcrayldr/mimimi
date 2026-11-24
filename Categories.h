#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace cray
{
    enum class SoundCategory : int
    {
        All = 0,
        PlucksAndBells,
        BassesAnd808,
        KeysAndPianos,
        PadsAndAtmospheres,
        Leads,
        StringsAndOrchestral,
        DrumsAndPercussion,
        VocalsAndFX,
        Count
    };

    inline constexpr std::array<const char*, 8> kCategoryNames = {
        "Plucks & Bells",
        "Basses & 808",
        "Keys & Pianos",
        "Pads & Atmospheres",
        "Leads",
        "Strings & Orchestral",
        "Drums & Percussion",
        "Vocals & FX"
    };

    inline juce::String categoryToString(SoundCategory cat)
    {
        const int idx = static_cast<int>(cat);
        if (idx >= 1 && idx <= static_cast<int>(kCategoryNames.size()))
            return kCategoryNames[static_cast<size_t>(idx - 1)];
        return "All";
    }

    inline SoundCategory categoryFromString(const juce::String& name)
    {
        for (size_t i = 0; i < kCategoryNames.size(); ++i)
        {
            if (name.equalsIgnoreCase(kCategoryNames[i]))
                return static_cast<SoundCategory>(i + 1);
        }
        return SoundCategory::PlucksAndBells;
    }
}
