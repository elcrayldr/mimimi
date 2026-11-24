#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    /**
     * Custom commercial JUCE LookAndFeel for CRAY LDR URBAN INSTRUMENTS:
     * Obsidian/Graphite hardware aesthetic with warm gold machined dials, crisp borders, and legible typography.
     */
    class CrayLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        CrayLookAndFeel();

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider& slider) override;

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              juce::Slider::SliderStyle style, juce::Slider& slider) override;

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;

        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox& box) override;

        juce::Font getComboBoxFont(juce::ComboBox& box) override;
        juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    };
}
