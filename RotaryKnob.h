#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    /**
     * Self-contained labeled rotary knob with live value readout and optional APVTS attachment.
     */
    class RotaryKnob : public juce::Component
    {
    public:
        explicit RotaryKnob(const juce::String& labelText = {}, bool bipolar = false);

        void attachToParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
        juce::Slider& getSlider() noexcept { return slider_; }
        void setLabel(const juce::String& text);
        void setValueSuffix(const juce::String& suffix);

        void resized() override;

    private:
        juce::Slider slider_;
        juce::Label  titleLabel_;
        juce::Label  valueLabel_;
        juce::String suffix_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;

        void updateValueLabel();
    };
}
