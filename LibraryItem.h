#pragma once

#include "Common/Categories.h"
#include <juce_core/juce_core.h>

namespace cray
{
    enum class LibraryItemType : int
    {
        Preset = 0,
        Instrument,
        Sample
    };

    enum class LibraryScope : int
    {
        All = 0,
        Factory,
        User
    };

    struct LibraryItem
    {
        juce::String    id;
        juce::String    name;
        juce::String    filePath;
        juce::String    virtualFolder; // e.g., "808s/Hard"
        SoundCategory   category      = SoundCategory::PlucksAndBells;
        juce::String    bank          = "Factory";
        juce::String    author        = "CRAY LDR";
        juce::String    tags;
        LibraryItemType type          = LibraryItemType::Preset;
        bool            isFactory     = false;
        bool            isFavorite    = false;
        bool            isMissing     = false;

        juce::String    contentHash;
        juce::int64     fileSizeBytes = 0;
        double          sampleRate    = 0.0;
        int             channels      = 0;
        double          durationSec   = 0.0;
    };

    struct LibraryQuery
    {
        LibraryScope    scope         = LibraryScope::All;
        LibraryItemType type          = LibraryItemType::Preset;
        bool            filterByType  = true;
        SoundCategory   category      = SoundCategory::All;
        juce::String    searchText;
        juce::String    folderFilter;
        bool            favoritesOnly = false;
    };
}
