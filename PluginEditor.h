#pragma once

#include "Plugin/PluginProcessor.h"
#include "UI/Theme/CrayLookAndFeel.h"
#include "UI/Components/HeaderBar.h"
#include "UI/Components/BrowserSidebar.h"
#include "UI/Components/VirtualKeyboardBar.h"
#include "UI/Views/MainView.h"
#include "UI/Views/EditView.h"
#include "UI/Views/SampleEditorView.h"
#include "UI/Views/InstrumentBuilderView.h"
#include "UI/Views/EffectsView.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace cray
{
    class CrayLdrAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
    {
    public:
        explicit CrayLdrAudioProcessorEditor(CrayLdrAudioProcessor& p);
        ~CrayLdrAudioProcessorEditor() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void timerCallback() override;
        void showActiveView(ActiveViewTab tab);
        void syncAllViews();
        void stepPreset(int delta);
        void openSavePresetDialog();
        void openImportSamplesDialog();
        void openChangeUserFolderDialog();
        void openExportPackDialog();
        void openImportPackDialog();

        CrayLdrAudioProcessor& processor_;
        CrayLookAndFeel        lookAndFeel_;

        HeaderBar              headerBar_;
        BrowserSidebar         browserSidebar_;
        VirtualKeyboardBar     keyboardBar_;

        MainView               mainView_;
        EditView               editView_;
        SampleEditorView       sampleEditorView_;
        InstrumentBuilderView  instrumentBuilderView_;
        EffectsView            effectsView_;

        ActiveViewTab          activeTab_      = ActiveViewTab::Main;
        bool                   browserVisible_ = true;
        uint64_t               lastMidiCount_  = 0;

        std::unique_ptr<juce::FileChooser> fileChooser_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrayLdrAudioProcessorEditor)
    };
}
