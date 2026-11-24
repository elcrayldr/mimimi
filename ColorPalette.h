#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    namespace Palette
    {
        // Obsidian & Graphite surfaces
        inline const juce::Colour bgObsidian       { 0xFF0B0C10 };
        inline const juce::Colour bgGraphite       { 0xFF13151B };
        inline const juce::Colour bgPanel          { 0xFF1A1D26 };
        inline const juce::Colour bgCard           { 0xFF212531 };
        inline const juce::Colour bgElevated       { 0xFF2A2F3E };
        inline const juce::Colour borderSubtle     { 0xFF2E3446 };
        inline const juce::Colour borderBright     { 0xFF454E68 };

        // Warm Gold & Amber accents
        inline const juce::Colour goldPrimary      { 0xFFE6B34B };
        inline const juce::Colour goldBright       { 0xFFF7CE6D };
        inline const juce::Colour goldDim          { 0xFF8C6924 };
        inline const juce::Colour goldGlow         { 0x44E6B34B };

        // Typography & status indicators
        inline const juce::Colour textPrimary      { 0xFFF1F3F8 };
        inline const juce::Colour textSecondary    { 0xFF9EA6B8 };
        inline const juce::Colour textMuted        { 0xFF666E82 };

        inline const juce::Colour ledGreen         { 0xFF39D98A };
        inline const juce::Colour ledCyan          { 0xFF4CC9F0 };
        inline const juce::Colour ledRed           { 0xFFFF5A5F };
        inline const juce::Colour ledAmber         { 0xFFF4A261 };
    }
}
