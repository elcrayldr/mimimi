#include "UI/Views/InstrumentBuilderView.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/Categories.h"
#include "Common/ParameterIDs.h"
#include <algorithm>

namespace cray
{
    InstrumentBuilderView::InstrumentBuilderView(CrayLdrAudioProcessor& processor)
        : processor_(processor)
    {
        nameEditor_.setTextToShowWhenEmpty("Instrument Name...", Palette::textMuted);
        nameEditor_.onTextChange = [this]()
        {
            if (!updatingUi_)
                processor_.getActiveInstrument().name = nameEditor_.getText();
        };
        addAndMakeVisible(nameEditor_);

        for (int i = 0; i < static_cast<int>(kCategoryNames.size()); ++i)
            categoryBox_.addItem(kCategoryNames[static_cast<size_t>(i)], i + 1);
        categoryBox_.onChange = [this]()
        {
            if (!updatingUi_)
                processor_.getActiveInstrument().category = static_cast<SoundCategory>(categoryBox_.getSelectedId());
        };
        addAndMakeVisible(categoryBox_);

        targetLayerBox_.onChange = [this]()
        {
            if (updatingUi_) return;
            const int l = std::max(0, targetLayerBox_.getSelectedItemIndex());
            auto& inst = processor_.getActiveInstrument();
            if (l < static_cast<int>(inst.layers.size()))
            {
                updatingUi_ = true;
                layerEnableBtn_.setToggleState(inst.layers[static_cast<size_t>(l)].enabled, juce::dontSendNotification);
                layerGainSlider_.setValue(inst.layers[static_cast<size_t>(l)].gain, juce::dontSendNotification);
                updatingUi_ = false;
            }
            keyZoneMap_.setInstrument(&inst, -1, selectedZoneIdx_);
        };
        addAndMakeVisible(targetLayerBox_);

        addLayerBtn_.onClick = [this]()
        {
            auto& inst = processor_.getActiveInstrument();
            inst.ensureLayerExists(static_cast<int>(inst.layers.size()));
            refreshLayerCombo();
            targetLayerBox_.setSelectedItemIndex(static_cast<int>(inst.layers.size()) - 1, juce::dontSendNotification);
            builderStatusLabel_.setText("Added Layer " + juce::String(static_cast<int>(inst.layers.size())), juce::dontSendNotification);
        };
        addAndMakeVisible(addLayerBtn_);

        addAndMakeVisible(layerEnableBtn_);
        layerEnableBtn_.onClick = [this]()
        {
            if (updatingUi_) return;
            const int l = std::max(0, targetLayerBox_.getSelectedItemIndex());
            auto& inst = processor_.getActiveInstrument();
            if (l < static_cast<int>(inst.layers.size()))
            {
                inst.layers[static_cast<size_t>(l)].enabled = layerEnableBtn_.getToggleState();
                processor_.rebuildAndPublishInstrument();
            }
        };

        layerGainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
        layerGainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
        layerGainSlider_.setRange(0.0, 1.5, 0.01);
        layerGainSlider_.setValue(1.0, juce::dontSendNotification);
        layerGainSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            const int l = std::max(0, targetLayerBox_.getSelectedItemIndex());
            auto& inst = processor_.getActiveInstrument();
            if (l < static_cast<int>(inst.layers.size()))
            {
                inst.layers[static_cast<size_t>(l)].gain = static_cast<float>(layerGainSlider_.getValue());
                processor_.rebuildAndPublishInstrument();
            }
        };
        addAndMakeVisible(layerGainSlider_);

        addSamplesBtn_.onClick = [this]()
        {
            fileChooser_ = std::make_unique<juce::FileChooser>("Import Samples into Instrument Builder",
                                                               processor_.getLibraryManager().getUserLibraryRoot(),
                                                               "*.wav;*.aif;*.aiff;*.flac");
            fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::canSelectMultipleItems,
                [this](const juce::FileChooser& fc)
                {
                    const auto results = fc.getResults();
                    if (!results.isEmpty())
                    {
                        const int layer = std::max(0, targetLayerBox_.getSelectedItemIndex());
                        const int added = processor_.getInstrumentBuilder().addSamples(
                            processor_.getActiveInstrument(), results, layer, true);
                        builderStatusLabel_.setText("Added " + juce::String(added) + " sample(s) to Layer " + juce::String(layer + 1),
                                                    juce::dontSendNotification);
                        syncFromInstrument();
                    }
                });
        };
        addAndMakeVisible(addSamplesBtn_);

        autoMapBtn_.onClick = [this]()
        {
            auto& inst = processor_.getActiveInstrument();
            InstrumentBuilder::autoMapZonesByRootNote(inst, -1);
            processor_.rebuildAndPublishInstrument();
            builderStatusLabel_.setText("Auto-mapped zones across keyboard by root note.", juce::dontSendNotification);
            syncFromInstrument();
        };
        addAndMakeVisible(autoMapBtn_);

        buildBtn_.getProperties().set("accent", true);
        buildBtn_.onClick = [this]()
        {
            auto& inst = processor_.getActiveInstrument();
            inst.isFactory = false; // Built user instruments belong to User Library
            inst.name = nameEditor_.getText().trim().isEmpty() ? "Custom Urban Instrument" : nameEditor_.getText().trim();

            const auto result = processor_.getInstrumentBuilder().buildInstrument(inst, true);
            builderStatusLabel_.setText(result.statusMessage, juce::dontSendNotification);

            if (result.success)
            {
                // Switch engine to Hybrid or Sample so built instrument plays right away
                if (auto* modeP = processor_.getApvts().getParameter(ParamIDs::engineMode))
                {
                    if (modeP->getValue() < 0.25f) // SynthOnly -> switch to SampleOnly
                        modeP->setValueNotifyingHost(0.5f);
                }
                processor_.rebuildAndPublishInstrument();
                if (onInstrumentBuilt)
                    onInstrumentBuilt();
            }
            syncFromInstrument();
        };
        addAndMakeVisible(buildBtn_);

        keyZoneMap_.onZoneSelected = [this](int idx)
        {
            selectedZoneIdx_ = idx;
            zoneList_.selectRow(idx);
            syncFromInstrument();
        };
        keyZoneMap_.onZonesModified = [this]()
        {
            processor_.rebuildAndPublishInstrument();
            syncFromInstrument();
        };
        addAndMakeVisible(keyZoneMap_);

        zoneList_.setModel(this);
        zoneList_.setRowHeight(24);
        addAndMakeVisible(zoneList_);

        configureIntSlider(rootNoteSlider_, 0, 127);
        configureIntSlider(lowKeySlider_,   0, 127);
        configureIntSlider(highKeySlider_,  0, 127);
        configureIntSlider(lowVelSlider_,   1, 127);
        configureIntSlider(highVelSlider_,  1, 127);

        rootNoteSlider_.textFromValueFunction = [](double v)
        {
            const int n = static_cast<int>(v);
            return juce::String(n) + " (" + InstrumentBuilder::midiNoteToName(n) + ")";
        };
        lowKeySlider_.textFromValueFunction  = rootNoteSlider_.textFromValueFunction;
        highKeySlider_.textFromValueFunction = rootNoteSlider_.textFromValueFunction;

        addAndMakeVisible(zoneLayerBox_);
        zoneLayerBox_.onChange = [this]() { applyZoneInspectorEdits(); };

        removeZoneBtn_.onClick = [this]()
        {
            auto& zones = processor_.getActiveInstrument().zones;
            if (selectedZoneIdx_ >= 0 && selectedZoneIdx_ < static_cast<int>(zones.size()))
            {
                zones.erase(zones.begin() + selectedZoneIdx_);
                selectedZoneIdx_ = std::min(selectedZoneIdx_, static_cast<int>(zones.size()) - 1);
                processor_.rebuildAndPublishInstrument();
                syncFromInstrument();
            }
        };
        addAndMakeVisible(removeZoneBtn_);

        builderStatusLabel_.setText("Drag samples here or click + SAMPLES, then BUILD INSTRUMENT.", juce::dontSendNotification);
        builderStatusLabel_.setColour(juce::Label::textColourId, Palette::goldBright);
        builderStatusLabel_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
        addAndMakeVisible(builderStatusLabel_);

        syncFromInstrument();
    }

    void InstrumentBuilderView::configureIntSlider(juce::Slider& s, int minV, int maxV, const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 20);
        s.setRange(minV, maxV, 1);
        s.setTextValueSuffix(suffix);
        s.onValueChange = [this]() { applyZoneInspectorEdits(); };
        addAndMakeVisible(s);
    }

    void InstrumentBuilderView::refreshLayerCombo()
    {
        const auto& inst = processor_.getActiveInstrument();
        const int prevTarget = std::max(0, targetLayerBox_.getSelectedItemIndex());
        const int prevZoneL  = std::max(0, zoneLayerBox_.getSelectedItemIndex());

        targetLayerBox_.clear(juce::dontSendNotification);
        zoneLayerBox_.clear(juce::dontSendNotification);

        for (int i = 0; i < static_cast<int>(inst.layers.size()); ++i)
        {
            const auto label = "Layer " + juce::String(i + 1);
            targetLayerBox_.addItem(label, i + 1);
            zoneLayerBox_.addItem(label, i + 1);
        }

        targetLayerBox_.setSelectedItemIndex(std::clamp(prevTarget, 0, std::max(0, targetLayerBox_.getNumItems() - 1)), juce::dontSendNotification);
        zoneLayerBox_.setSelectedItemIndex(std::clamp(prevZoneL, 0, std::max(0, zoneLayerBox_.getNumItems() - 1)), juce::dontSendNotification);
    }

    void InstrumentBuilderView::applyZoneInspectorEdits()
    {
        if (updatingUi_)
            return;

        auto& inst = processor_.getActiveInstrument();
        if (selectedZoneIdx_ < 0 || selectedZoneIdx_ >= static_cast<int>(inst.zones.size()))
            return;

        auto& z = inst.zones[static_cast<size_t>(selectedZoneIdx_)];
        z.rootNote     = static_cast<int>(rootNoteSlider_.getValue());
        z.lowKey       = std::min(static_cast<int>(lowKeySlider_.getValue()), static_cast<int>(highKeySlider_.getValue()));
        z.highKey      = std::max(static_cast<int>(lowKeySlider_.getValue()), static_cast<int>(highKeySlider_.getValue()));
        z.lowVelocity  = std::min(static_cast<int>(lowVelSlider_.getValue()), static_cast<int>(highVelSlider_.getValue()));
        z.highVelocity = std::max(static_cast<int>(lowVelSlider_.getValue()), static_cast<int>(highVelSlider_.getValue()));
        z.layerIndex   = std::max(0, zoneLayerBox_.getSelectedItemIndex());
        inst.ensureLayerExists(z.layerIndex);

        processor_.rebuildAndPublishInstrument();
        keyZoneMap_.setInstrument(&inst, -1, selectedZoneIdx_);
        zoneList_.repaint();
    }

    void InstrumentBuilderView::syncFromInstrument()
    {
        updatingUi_ = true;
        auto& inst = processor_.getActiveInstrument();

        nameEditor_.setText(inst.name, juce::dontSendNotification);
        categoryBox_.setSelectedId(static_cast<int>(inst.category), juce::dontSendNotification);
        refreshLayerCombo();

        const int activeLayer = std::max(0, targetLayerBox_.getSelectedItemIndex());
        if (activeLayer < static_cast<int>(inst.layers.size()))
        {
            layerEnableBtn_.setToggleState(inst.layers[static_cast<size_t>(activeLayer)].enabled, juce::dontSendNotification);
            layerGainSlider_.setValue(inst.layers[static_cast<size_t>(activeLayer)].gain, juce::dontSendNotification);
        }

        if (inst.zones.empty())
            selectedZoneIdx_ = -1;
        else if (selectedZoneIdx_ < 0 || selectedZoneIdx_ >= static_cast<int>(inst.zones.size()))
            selectedZoneIdx_ = 0;

        zoneList_.updateContent();
        if (selectedZoneIdx_ >= 0)
        {
            zoneList_.selectRow(selectedZoneIdx_, false, true);
            const auto& z = inst.zones[static_cast<size_t>(selectedZoneIdx_)];
            rootNoteSlider_.setValue(z.rootNote, juce::dontSendNotification);
            lowKeySlider_.setValue(z.lowKey, juce::dontSendNotification);
            highKeySlider_.setValue(z.highKey, juce::dontSendNotification);
            lowVelSlider_.setValue(z.lowVelocity, juce::dontSendNotification);
            highVelSlider_.setValue(z.highVelocity, juce::dontSendNotification);
            zoneLayerBox_.setSelectedItemIndex(std::clamp(z.layerIndex, 0, std::max(0, zoneLayerBox_.getNumItems() - 1)),
                                               juce::dontSendNotification);
        }

        keyZoneMap_.setInstrument(&inst, -1, selectedZoneIdx_);
        updatingUi_ = false;
    }

    int InstrumentBuilderView::getNumRows()
    {
        return static_cast<int>(processor_.getActiveInstrument().zones.size());
    }

    void InstrumentBuilderView::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
    {
        const auto& zones = processor_.getActiveInstrument().zones;
        if (rowNumber < 0 || rowNumber >= static_cast<int>(zones.size()))
            return;

        const auto& z = zones[static_cast<size_t>(rowNumber)];
        g.setColour(rowIsSelected ? Palette::goldPrimary.withAlpha(0.24f) : Palette::bgCard);
        g.fillRect(0, 0, width, height - 1);

        g.setColour(z.missingSourceFile ? Palette::ledRed : Palette::textPrimary);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
        const auto txt = "L" + juce::String(z.layerIndex + 1) + " | " + z.displayName
                       + " [" + InstrumentBuilder::midiNoteToName(z.lowKey) + " - "
                       + InstrumentBuilder::midiNoteToName(z.highKey) + "] Root:"
                       + InstrumentBuilder::midiNoteToName(z.rootNote);
        g.drawText(txt, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
    }

    void InstrumentBuilderView::selectedRowsChanged(int lastRowSelected)
    {
        if (updatingUi_ || lastRowSelected < 0)
            return;
        selectedZoneIdx_ = lastRowSelected;
        processor_.getSampleEditorModel().setSelectedZoneIndex(selectedZoneIdx_);
        syncFromInstrument();
    }

    bool InstrumentBuilderView::isInterestedInFileDrag(const juce::StringArray&)
    {
        return true;
    }

    void InstrumentBuilderView::filesDropped(const juce::StringArray& files, int, int)
    {
        juce::Array<juce::File> fileObjs;
        for (const auto& s : files)
            fileObjs.add(juce::File(s));

        const int layer = std::max(0, targetLayerBox_.getSelectedItemIndex());
        const int added = processor_.getInstrumentBuilder().addSamples(
            processor_.getActiveInstrument(), fileObjs, layer, true);
        if (added > 0)
        {
            InstrumentBuilder::autoMapZonesByRootNote(processor_.getActiveInstrument(), layer);
            processor_.rebuildAndPublishInstrument();
            builderStatusLabel_.setText("Dropped & auto-mapped " + juce::String(added) + " sample(s) to Layer " + juce::String(layer + 1),
                                        juce::dontSendNotification);
            syncFromInstrument();
        }
    }

    void InstrumentBuilderView::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);
        const auto area = getLocalBounds().reduced(10);
        auto botArea = area.withTop(area.getBottom() - 180);
        g.setColour(Palette::bgPanel);
        g.fillRoundedRectangle(botArea.toFloat(), 7.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(botArea.toFloat(), 7.0f, 1.0f);

        const int inspX = botArea.getX() + botArea.getWidth() / 2 + 8;
        g.setColour(Palette::textSecondary);
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText("ZONE INSPECTOR (ROOT / KEY / VELOCITY / LAYER)", inspX, botArea.getY() + 6,
                   botArea.getWidth() / 2 - 16, 16, juce::Justification::left);
    }

    void InstrumentBuilderView::resized()
    {
        auto area = getLocalBounds().reduced(10);

        auto topBar = area.removeFromTop(28);
        nameEditor_.setBounds(topBar.removeFromLeft(175));
        topBar.removeFromLeft(4);
        categoryBox_.setBounds(topBar.removeFromLeft(135));
        topBar.removeFromLeft(4);
        targetLayerBox_.setBounds(topBar.removeFromLeft(88));
        topBar.removeFromLeft(4);
        addLayerBtn_.setBounds(topBar.removeFromLeft(66));
        topBar.removeFromLeft(4);
        addSamplesBtn_.setBounds(topBar.removeFromLeft(88));
        topBar.removeFromLeft(4);
        autoMapBtn_.setBounds(topBar.removeFromLeft(82));
        topBar.removeFromLeft(6);
        buildBtn_.setBounds(topBar.removeFromRight(140));

        area.removeFromTop(6);
        auto layerBar = area.removeFromTop(22);
        layerEnableBtn_.setBounds(layerBar.removeFromLeft(85));
        layerGainSlider_.setBounds(layerBar.removeFromLeft(140));
        layerBar.removeFromLeft(8);
        builderStatusLabel_.setBounds(layerBar);

        area.removeFromTop(6);
        auto botArea = area.removeFromBottom(180).reduced(8);
        area.removeFromBottom(6);

        keyZoneMap_.setBounds(area);

        auto leftList = botArea.removeFromLeft(botArea.getWidth() / 2 - 6);
        zoneList_.setBounds(leftList);

        botArea.removeFromLeft(12);
        botArea.removeFromTop(18);
        const int rh = 24;
        rootNoteSlider_.setBounds(botArea.removeFromTop(rh));
        lowKeySlider_.setBounds(botArea.removeFromTop(rh));
        highKeySlider_.setBounds(botArea.removeFromTop(rh));
        lowVelSlider_.setBounds(botArea.removeFromTop(rh));
        highVelSlider_.setBounds(botArea.removeFromTop(rh));

        auto lastRow = botArea.removeFromTop(rh);
        zoneLayerBox_.setBounds(lastRow.removeFromLeft(110));
        lastRow.removeFromLeft(8);
        removeZoneBtn_.setBounds(lastRow.removeFromLeft(115));
    }
}
