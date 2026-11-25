#pragma once

#include "UI/Components/LevelMeter.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

namespace cray
{
    enum class ActiveViewTab : int
    {
        Main = 0,
        Edit,
        SampleEditor,
        InstrumentBuilder,
        Effects
    };

    class HeaderBar : public juce::Component
    {
    public:
        explicit HeaderBar(juce::AudioProcessorValueTreeState& apvts);

        std::function<void(ActiveViewTab)> onTabSelected;
        std::function<void()>              onPrevPreset;
        std::function<void()>              onNextPreset;
        std::function<void()>              onSavePreset;
        std::function<void()>              onInitPreset;
        std::function<void()>              onExportPack;
        std::function<void()>              onImportPack;
        std::function<void()>              onToggleBrowser;

        void setActiveTab(ActiveViewTab tab);
        void setPresetInfo(const juce::String& name, const juce::String& bankCategory, const juce::String& statusText, bool hasMissingSamples);
        void setTelemetry(float peakL, float peakR, bool clipped, bool midiActive, int voiceCount);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        std::array<juce::TextButton, 5> tabButtons_;
        ActiveViewTab                   currentTab_ = ActiveViewTab::Main;

        juce::TextButton brandLink_          { "" };
        juce::TextButton browserToggleBtn_ { "LIB" };
        juce::TextButton prevBtn_          { "<" };
        juce::TextButton nextBtn_          { ">" };
        juce::TextButton saveBtn_          { "SAVE" };
        juce::TextButton initBtn_          { "INIT" };
        juce::TextButton exportBtn_        { "PACK" };
        juce::TextButton importBtn_        { "IMP" };

        juce::Label      presetNameLabel_;
        juce::Label      presetSubLabel_;
        juce::Label      statusLabel_;

        juce::Slider     masterGainSlider_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterGainAttach_;
        LevelMeter       meter_;

        bool midiFlash_         = false;
        bool hasMissingSamples_ = false;
        int  activeVoices_      = 0;
    };
}
