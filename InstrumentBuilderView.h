#pragma once

#include "Plugin/PluginProcessor.h"
#include "UI/Components/KeyZoneMapView.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cray
{
    /**
     * Interactive Multi-Sample Instrument Builder View:
     * - Multi-sample import + Drag-and-Drop
     * - Auto-Mapping by note names
     * - Expandable Layers (4 initial + Add Layer)
     * - Visual KeyZoneMapView + Manual Root/Key/Vel/Layer inspector
     * - Real BUILD INSTRUMENT operation generating a playable, saved .crayinst
     */
    class InstrumentBuilderView : public juce::Component,
                                  public juce::ListBoxModel,
                                  public juce::FileDragAndDropTarget
    {
    public:
        explicit InstrumentBuilderView(CrayLdrAudioProcessor& processor);

        void syncFromInstrument();
        std::function<void()> onInstrumentBuilt;

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRowSelected) override;

        // FileDragAndDropTarget
        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void configureIntSlider(juce::Slider& s, int minV, int maxV, const juce::String& suffix = {});
        void applyZoneInspectorEdits();
        void refreshLayerCombo();

        CrayLdrAudioProcessor& processor_;

        juce::TextEditor nameEditor_;
        juce::ComboBox   categoryBox_;
        juce::ComboBox   targetLayerBox_;
        juce::TextButton addLayerBtn_   { "+ LAYER" };
        juce::TextButton addSamplesBtn_ { "+ SAMPLES" };
        juce::TextButton autoMapBtn_    { "AUTO-MAP" };
        juce::TextButton buildBtn_      { "BUILD INSTRUMENT" };

        juce::ToggleButton layerEnableBtn_ { "LAYER ON" };
        juce::Slider       layerGainSlider_;

        KeyZoneMapView   keyZoneMap_;
        juce::ListBox    zoneList_;

        juce::Slider     rootNoteSlider_, lowKeySlider_, highKeySlider_;
        juce::Slider     lowVelSlider_, highVelSlider_;
        juce::ComboBox   zoneLayerBox_;
        juce::TextButton autoRootBtn_ { "AUTO ROOT" };
        juce::TextButton removeZoneBtn_ { "REMOVE ZONE" };
        juce::Label      builderStatusLabel_;

        int  selectedZoneIdx_ = -1;
        bool updatingUi_      = false;
        std::unique_ptr<juce::FileChooser> fileChooser_;
    };
}
