#include "Plugin/PluginEditor.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/Categories.h"
#include "Common/ProductInfo.h"

namespace cray
{
    CrayLdrAudioProcessorEditor::CrayLdrAudioProcessorEditor(CrayLdrAudioProcessor& p)
        : AudioProcessorEditor(&p),
          processor_(p),
          headerBar_(p.getApvts()),
          browserSidebar_(p.getLibraryManager()),
          keyboardBar_(p.getApvts()),
          mainView_(p.getApvts()),
          editView_(p.getApvts()),
          sampleEditorView_(p),
          instrumentBuilderView_(p),
          effectsView_(p)
    {
        setLookAndFeel(&lookAndFeel_);

        addAndMakeVisible(headerBar_);
        addAndMakeVisible(browserSidebar_);
        addAndMakeVisible(keyboardBar_);

        addAndMakeVisible(mainView_);
        addChildComponent(editView_);
        addChildComponent(sampleEditorView_);
        addChildComponent(instrumentBuilderView_);
        addChildComponent(effectsView_);

        headerBar_.onTabSelected = [this](ActiveViewTab tab) { showActiveView(tab); };
        headerBar_.onToggleBrowser = [this]()
        {
            browserVisible_ = !browserVisible_;
            browserSidebar_.setVisible(browserVisible_);
            resized();
        };
        headerBar_.onPrevPreset = [this]() { stepPreset(-1); };
        headerBar_.onNextPreset = [this]() { stepPreset(1); };
        headerBar_.onSavePreset = [this]() { openSavePresetDialog(); };
        headerBar_.onInitPreset = [this]() { processor_.resetToInitPatch(); };
        headerBar_.onExportPack = [this]() { openExportPackDialog(); };
        headerBar_.onImportPack = [this]() { openImportPackDialog(); };

        browserSidebar_.onItemActivated = [this](const LibraryItem& item)
        {
            processor_.loadLibraryItem(item);
        };
        browserSidebar_.onFilesDropped = [this](const juce::Array<juce::File>& files)
        {
            const auto res = processor_.getLibraryManager().importAudioFiles(files);
            browserSidebar_.refreshItems();
            if (!res.importedIds.isEmpty())
            {
                if (const auto* item = processor_.getLibraryManager().findById(res.importedIds[0]))
                    processor_.loadLibraryItem(*item);
            }
        };
        browserSidebar_.onImportSamplesRequested    = [this]() { openImportSamplesDialog(); };
        browserSidebar_.onChangeUserFolderRequested = [this]() { openChangeUserFolderDialog(); };

        keyboardBar_.onMidiNote = [this](int note, float vel, bool on)
        {
            processor_.pushUiMidiNote(note, vel, on);
        };

        instrumentBuilderView_.onInstrumentBuilt = [this]()
        {
            browserSidebar_.refreshItems();
            syncAllViews();
        };

        processor_.onStateChanged = [this]()
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<CrayLdrAudioProcessorEditor>(this)]()
            {
                if (safeThis != nullptr)
                    safeThis->syncAllViews();
            });
        };

        setResizable(true, true);
        setResizeLimits(960, 620, 2200, 1400);
        setSize(1280, 800);

        syncAllViews();
        startTimerHz(30);
    }

    CrayLdrAudioProcessorEditor::~CrayLdrAudioProcessorEditor()
    {
        stopTimer();
        processor_.onStateChanged = nullptr;
        setLookAndFeel(nullptr);
    }

    void CrayLdrAudioProcessorEditor::showActiveView(ActiveViewTab tab)
    {
        activeTab_ = tab;
        mainView_.setVisible(tab == ActiveViewTab::Main);
        editView_.setVisible(tab == ActiveViewTab::Edit);
        sampleEditorView_.setVisible(tab == ActiveViewTab::SampleEditor);
        instrumentBuilderView_.setVisible(tab == ActiveViewTab::InstrumentBuilder);
        effectsView_.setVisible(tab == ActiveViewTab::Effects);

        if (tab == ActiveViewTab::SampleEditor)
            sampleEditorView_.syncFromModel();
        else if (tab == ActiveViewTab::InstrumentBuilder)
            instrumentBuilderView_.syncFromInstrument();
    }

    void CrayLdrAudioProcessorEditor::syncAllViews()
    {
        const auto& meta = processor_.getCurrentPresetMetadata();
        const auto sub   = meta.bank + "  |  " + categoryToString(meta.category);
        headerBar_.setPresetInfo(meta.name, sub, processor_.getStatusBanner(), !processor_.getMissingSamples().isEmpty());
        sampleEditorView_.syncFromModel();
        instrumentBuilderView_.syncFromInstrument();
        browserSidebar_.refreshItems();
    }

    void CrayLdrAudioProcessorEditor::stepPreset(int delta)
    {
        const auto& items = browserSidebar_.getFilteredItems();
        if (items.empty())
            return;

        const auto currentName = processor_.getCurrentPresetMetadata().name;
        int idx = 0;
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            if (items[static_cast<size_t>(i)].name == currentName)
            {
                idx = i;
                break;
            }
        }
        const int nextIdx = (idx + delta + static_cast<int>(items.size())) % static_cast<int>(items.size());
        processor_.loadLibraryItem(items[static_cast<size_t>(nextIdx)]);
    }

    void CrayLdrAudioProcessorEditor::openSavePresetDialog()
    {
        auto* alert = new juce::AlertWindow("Save User Preset",
                                            "Save current synth & instrument state to User Library:",
                                            juce::MessageBoxIconType::NoIcon);
        alert->addTextEditor("name", processor_.getCurrentPresetMetadata().name);
        juce::StringArray cats;
        for (const auto* c : kCategoryNames) cats.add(c);
        alert->addComboBox("category", cats, "Category");
        alert->addTextEditor("tags", processor_.getCurrentPresetMetadata().tags);
        alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        alert->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, alert](int r)
            {
                if (r == 1)
                {
                    const auto name = alert->getTextEditorContents("name");
                    const auto cat  = static_cast<SoundCategory>(alert->getComboBoxComponent("category")->getSelectedId());
                    const auto tags = alert->getTextEditorContents("tags");
                    processor_.saveCurrentPresetToUserLibrary(name, cat, tags);
                }
                delete alert;
            }), false);
    }

    void CrayLdrAudioProcessorEditor::openImportSamplesDialog()
    {
        fileChooser_ = std::make_unique<juce::FileChooser>("Import WAV / AIFF / FLAC Samples",
                                                           processor_.getLibraryManager().getUserLibraryRoot(),
                                                           "*.wav;*.aif;*.aiff;*.flac");
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc)
            {
                const auto files = fc.getResults();
                if (!files.isEmpty())
                {
                    processor_.getLibraryManager().importAudioFiles(files);
                    browserSidebar_.refreshItems();
                }
            });
    }

    void CrayLdrAudioProcessorEditor::openChangeUserFolderDialog()
    {
        fileChooser_ = std::make_unique<juce::FileChooser>("Select User Library Root Folder (Any Drive)",
                                                           processor_.getLibraryManager().getUserLibraryRoot());
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (dir.isDirectory())
                {
                    processor_.getLibraryManager().setUserLibraryRoot(dir);
                    browserSidebar_.refreshItems();
                }
            });
    }

    void CrayLdrAudioProcessorEditor::openExportPackDialog()
    {
        const auto defaultPack = processor_.getLibraryManager().getUserLibraryRoot()
            .getChildFile(juce::File::createLegalFileName(processor_.getCurrentPresetMetadata().name) + ProductInfo::kPackExtension);

        fileChooser_ = std::make_unique<juce::FileChooser>("Export Portable Instrument & Sample Pack (.craypack)",
                                                           defaultPack,
                                                           "*" + juce::String(ProductInfo::kPackExtension));
        fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto target = fc.getResult();
                if (target != juce::File())
                {
                    processor_.getPresetManager().exportPortablePack(
                        target,
                        processor_.getCurrentPresetMetadata(),
                        processor_.getApvts(),
                        processor_.getActiveInstrument());
                }
            });
    }

    void CrayLdrAudioProcessorEditor::openImportPackDialog()
    {
        fileChooser_ = std::make_unique<juce::FileChooser>("Import Portable Pack (.craypack)",
                                                           processor_.getLibraryManager().getUserLibraryRoot(),
                                                           "*" + juce::String(ProductInfo::kPackExtension));
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto packFile = fc.getResult();
                if (packFile.existsAsFile())
                {
                    const auto extractDir = processor_.getLibraryManager().getUserLibraryRoot().getChildFile("ImportedPacks");
                    const auto res = processor_.getPresetManager().importPortablePack(packFile, processor_.getApvts(), extractDir);
                    if (res.success)
                    {
                        processor_.getActiveInstrument() = res.instrument;
                        processor_.rebuildAndPublishInstrument();
                        syncAllViews();
                    }
                }
            });
    }

    void CrayLdrAudioProcessorEditor::timerCallback()
    {
        auto& tel = processor_.getTelemetry();
        const float pL = tel.peakLeft.load(std::memory_order_relaxed);
        const float pR = tel.peakRight.load(std::memory_order_relaxed);
        const bool clip = tel.clippingDetected.load(std::memory_order_relaxed);
        const int voices = tel.totalVoices.load(std::memory_order_relaxed);
        const int lastNote = tel.lastTriggeredMidiNote.load(std::memory_order_relaxed);
        const uint64_t midiCnt = tel.midiActivityCounter.load(std::memory_order_relaxed);

        const bool midiActive = (midiCnt != lastMidiCount_);
        lastMidiCount_ = midiCnt;

        tel.resetPeaks();
        headerBar_.setTelemetry(pL, pR, clip, midiActive, voices);
        keyboardBar_.setActiveVoiceCount(voices, voices > 0 ? lastNote : -1);
    }

    void CrayLdrAudioProcessorEditor::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);
    }

    void CrayLdrAudioProcessorEditor::resized()
    {
        auto area = getLocalBounds();
        headerBar_.setBounds(area.removeFromTop(56));
        keyboardBar_.setBounds(area.removeFromBottom(76));

        if (browserVisible_)
            browserSidebar_.setBounds(area.removeFromLeft(252));

        mainView_.setBounds(area);
        editView_.setBounds(area);
        sampleEditorView_.setBounds(area);
        instrumentBuilderView_.setBounds(area);
        effectsView_.setBounds(area);
    }
}
