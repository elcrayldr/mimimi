#pragma once

#include "Common/ProductInfo.h"
#include "UI/Components/RotaryKnob.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace cray
{
    /**
     * Full 4-Oscillator Synth Engine + Dual LFO + 8-Slot Modulation Matrix view.
     */
    class EditView : public juce::Component
    {
    public:
        explicit EditView(juce::AudioProcessorValueTreeState& apvts);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        struct OscStrip
        {
            juce::ToggleButton enabledBtn;
            juce::ComboBox     waveBox;
            RotaryKnob         levelKnob   { "LEVEL" };
            RotaryKnob         panKnob     { "PAN", true };
            RotaryKnob         pitchKnob   { "PITCH", true };
            RotaryKnob         fineKnob    { "FINE", true };
            RotaryKnob         unisonKnob  { "UNISON" };
            RotaryKnob         detuneKnob  { "DETUNE" };
            RotaryKnob         spreadKnob  { "SPREAD" };

            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   enableAttach;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
        };

        struct LfoStrip
        {
            juce::Label        title;
            juce::ComboBox     waveBox;
            juce::ToggleButton syncBtn      { "SYNC" };
            juce::ToggleButton retrigBtn    { "RETRIG" };
            juce::ComboBox     divBox;
            RotaryKnob         rateKnob     { "RATE" };
            RotaryKnob         depthKnob    { "DEPTH" };

            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   syncAttach;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   retrigAttach;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttach;
        };

        struct ModSlotRow
        {
            juce::Label    idxLabel;
            juce::ComboBox srcBox;
            juce::ComboBox dstBox;
            juce::Slider   amtSlider;

            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dstAttach;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amtAttach;
        };

        std::array<OscStrip, ProductInfo::kNumOscillators> oscStrips_;
        std::array<LfoStrip, ProductInfo::kNumLfos>        lfoStrips_;
        std::array<ModSlotRow, ProductInfo::kNumModSlots>  modRows_;
    };
}
