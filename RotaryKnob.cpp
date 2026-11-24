#include "UI/Components/RotaryKnob.h"
#include "UI/Theme/ColorPalette.h"

namespace cray
{
    RotaryKnob::RotaryKnob(const juce::String& labelText, bool bipolar)
    {
        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider_.getProperties().set("bipolar", bipolar);
        slider_.onValueChange = [this]() { updateValueLabel(); };
        addAndMakeVisible(slider_);

        titleLabel_.setText(labelText, juce::dontSendNotification);
        titleLabel_.setJustificationType(juce::Justification::centred);
        titleLabel_.setColour(juce::Label::textColourId, Palette::textSecondary);
        titleLabel_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        addAndMakeVisible(titleLabel_);

        valueLabel_.setJustificationType(juce::Justification::centred);
        valueLabel_.setColour(juce::Label::textColourId, Palette::goldBright);
        valueLabel_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::plain)));
        addAndMakeVisible(valueLabel_);

        updateValueLabel();
    }

    void RotaryKnob::attachToParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
    {
        attachment_.reset();
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider_);
        if (auto* param = apvts.getParameter(paramId))
        {
            if (suffix_.isEmpty())
                suffix_ = param->getLabel();
        }
        updateValueLabel();
    }

    void RotaryKnob::setLabel(const juce::String& text)
    {
        titleLabel_.setText(text, juce::dontSendNotification);
    }

    void RotaryKnob::setValueSuffix(const juce::String& suffix)
    {
        suffix_ = suffix;
        updateValueLabel();
    }

    void RotaryKnob::updateValueLabel()
    {
        const double val = slider_.getValue();
        juce::String text;
        if (std::abs(val) >= 1000.0)
            text = juce::String(val / 1000.0, 2) + "k";
        else if (std::abs(val) >= 100.0 || slider_.getInterval() >= 1.0)
            text = juce::String(std::round(val));
        else
            text = juce::String(val, 2);

        if (suffix_.isNotEmpty())
            text += " " + suffix_;
        valueLabel_.setText(text, juce::dontSendNotification);
    }

    void RotaryKnob::resized()
    {
        auto area = getLocalBounds();
        titleLabel_.setBounds(area.removeFromTop(14));
        valueLabel_.setBounds(area.removeFromBottom(14));
        slider_.setBounds(area);
    }
}
