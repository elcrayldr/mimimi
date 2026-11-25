#pragma once

#include "Plugin/PluginProcessor.h"
#include "UI/Components/WaveformView.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    /**
     * Non-destructive Visual Sample Editor View:
     * Waveform + Zoom + Scroll + Region Selection + Start/End/Loop/Fade +
     * Reverse + Gain + Normalize + Pitch/Fine/Pan + Zone ADSR + Zone Filter + Live Audition.
     */
    class SampleEditorView : public juce::Component
    {
    public:
        explicit SampleEditorView(CrayLdrAudioProcessor& processor);

        void syncFromModel();
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void configureSlider(juce::Slider& s, double minV, double maxV, double step, const juce::String& suffix);

        CrayLdrAudioProcessor& processor_;

        juce::ComboBox     zoneSelector_;
        juce::TextButton   auditionBtn_  { "AUDITION" };
        juce::TextButton   normalizeBtn_ { "NORMALIZE" };
        juce::TextButton   autoRootBtn_  { "AUTO ROOT" };
        juce::TextButton   relinkBtn_    { "LOCATE MISSING" };
        juce::Label        infoLabel_;

        WaveformView       waveformView_;
        juce::Slider       rootNoteSlider_;
        juce::Slider       zoomSlider_;
        juce::Slider       scrollSlider_;

        juce::ToggleButton reverseBtn_   { "REVERSE" };
        juce::ToggleButton loopBtn_      { "LOOP" };

        juce::Slider startSlider_, endSlider_;
        juce::Slider loopStartSlider_, loopEndSlider_;
        juce::Slider gainSlider_, pitchSlider_, fineSlider_, panSlider_;
        juce::Slider fadeInMsSlider_, fadeOutMsSlider_;
        juce::Slider attackSlider_, decaySlider_, sustainSlider_, releaseSlider_;

        juce::ToggleButton filterEnableBtn_ { "ZONE FILTER" };
        juce::ComboBox     filterTypeBox_;
        juce::Slider       cutoffSlider_, resSlider_;

        bool updatingUi_ = false;
        std::unique_ptr<juce::FileChooser> relinkChooser_;
    };
}
