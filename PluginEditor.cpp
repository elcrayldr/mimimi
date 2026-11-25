#include "Plugin/PluginEditor.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/Categories.h"
#include "Common/ProductInfo.h"
#include <juce_core/juce_core.h>
#include <BinaryData.h>

namespace cray
{
    StartupSplash::StartupSplash()
    {
        if (auto logoXml = juce::XmlDocument::parse(
                juce::String::fromUTF8(BinaryData::crayldr_brand_svg, BinaryData::crayldr_brand_svgSize));
            logoXml != nullptr)
        {
            logo_ = juce::Drawable::createFromSVG(*logoXml);
        }
        setInterceptsMouseClicks(true, true);
    }

    void StartupSplash::setProgress(float progress)
    {
        progress_ = juce::jlimit(0.0f, 1.0f, progress);
        repaint();
    }

    void StartupSplash::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);

        const auto card = getLocalBounds().toFloat().withSizeKeepingCentre(
            juce::jmin(620.0f, getWidth() - 48.0f), 300.0f);
        g.setColour(Palette::bgGraphite);
        g.fillRoundedRectangle(card, 12.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(card, 12.0f, 1.0f);

        if (logo_ != nullptr)
            logo_->drawWithin(g, juce::Rectangle<float>(card.getCentreX() - 215.0f, card.getY() + 42.0f,
                                                       430.0f, 145.0f),
                              juce::RectanglePlacement::centred, 1.0f);

        g.setColour(Palette::textSecondary);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("STARTING  /  URBAN INSTRUMENTS",
                   card.withY(card.getCentreY() + 38.0f).withHeight(22.0f).toNearestInt(),
                   juce::Justification::centred);

        const auto track = juce::Rectangle<float>(card.getCentreX() - 190.0f, card.getBottom() - 48.0f, 380.0f, 4.0f);
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(track, 2.0f);
        g.setColour(Palette::goldPrimary);
        g.fillRoundedRectangle(track.withWidth(track.getWidth() * progress_), 2.0f);

        g.setColour(Palette::textMuted);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
        g.drawText("CRAY LDR  /  V1.0.2",
                   card.withY(card.getBottom() - 31.0f).withHeight(16.0f).toNearestInt(),
                   juce::Justification::centred);
    }

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
        addAndMakeVisible(startupSplash_);
        startupSplash_.setAlwaysOnTop(true);

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
            const auto res = processor_.getLibraryManager().importAudioFiles(files, SoundCategory::DrumsAndPercussion,
                browserSidebar_.getSelectedUserFolder(), true);
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

        juce::PropertiesFile::Options settingsOptions;
        settingsOptions.applicationName = "CRAY LDR Urban Instruments";
        settingsOptions.filenameSuffix = "settings";
        settingsOptions.folderName = "CRAY LDR";
        settingsOptions.storageFormat = juce::PropertiesFile::storeAsXML;
        editorSettings_ = std::make_unique<juce::PropertiesFile>(settingsOptions);

        setResizable(true, true);
        setResizeLimits(960, 620, 2200, 1400);
        const int savedWidth = editorSettings_->getIntValue("editorWidth", 1280);
        const int savedHeight = editorSettings_->getIntValue("editorHeight", 800);
        restoringEditorSize_ = true;
        setSize(juce::jlimit(960, 2200, savedWidth), juce::jlimit(620, 1400, savedHeight));
        restoringEditorSize_ = false;

        syncAllViews();
        startupSplashStartMs_ = juce::Time::getMillisecondCounter();
        startupSplash_.setProgress(0.0f);
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
                    const bool saved = processor_.saveCurrentPresetToUserLibrary(name, cat, tags);
                    if (!saved)
                        processor_.reportStatus("Could not save preset. Check the User Library folder permissions.");
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
                    processor_.getLibraryManager().importAudioFiles(files, SoundCategory::DrumsAndPercussion,
                        browserSidebar_.getSelectedUserFolder(), true);
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
                    const bool exported = processor_.getPresetManager().exportPortablePack(
                        target,
                        processor_.getCurrentPresetMetadata(),
                        processor_.getApvts(),
                        processor_.getActiveInstrument());
                    processor_.reportStatus(exported
                        ? ("Portable pack exported: " + target.getFileName())
                        : "Portable pack export failed. Check that every sample file is available and writable.");
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
                    processor_.applyImportedPack(res);
                }
            });
    }

    void CrayLdrAudioProcessorEditor::timerCallback()
    {
        if (startupSplashVisible_)
        {
            const auto elapsedMs = juce::Time::getMillisecondCounter() - startupSplashStartMs_;
            startupSplash_.setProgress(juce::jmin(1.0f, static_cast<float>(elapsedMs) / 900.0f));
            if (elapsedMs >= 1050)
            {
                startupSplashVisible_ = false;
                startupSplash_.setVisible(false);
            }
        }

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
        if (editorSettings_ != nullptr && !restoringEditorSize_)
        {
            editorSettings_->setValue("editorWidth", getWidth());
            editorSettings_->setValue("editorHeight", getHeight());
            editorSettings_->saveIfNeeded();
        }
        auto area = getLocalBounds();
        const int headerHeight = getWidth() < 1280 ? 96 : 64;
        headerBar_.setBounds(area.removeFromTop(headerHeight));
        keyboardBar_.setBounds(area.removeFromBottom(76));

        if (browserVisible_)
        {
            const int sidebarWidth = juce::jlimit(210, 280, static_cast<int>(getWidth() * 0.197f));
            browserSidebar_.setBounds(area.removeFromLeft(sidebarWidth));
        }

        mainView_.setBounds(area);
        editView_.setBounds(area);
        sampleEditorView_.setBounds(area);
        instrumentBuilderView_.setBounds(area);
        effectsView_.setBounds(area);
        startupSplash_.setBounds(getLocalBounds());
    }
}
