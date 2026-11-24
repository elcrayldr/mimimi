#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"

namespace cray
{
    CrayLdrAudioProcessor::CrayLdrAudioProcessor()
        : AudioProcessor(BusesProperties()
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts_(*this, nullptr, "CRAY_LDR_PARAMETERS", ParamIDs::createParameterLayout()),
          libraryManager_(sampleDecoder_),
          sampleEditorModel_(sampleDecoder_, activeInstrument_),
          instrumentBuilder_(sampleDecoder_, libraryManager_),
          presetManager_(sampleDecoder_, libraryManager_)
    {
        libraryManager_.initialize(LibraryManager::resolveDefaultFactoryRoot(),
                                   LibraryManager::resolveDefaultConfigFile());

        sampleEditorModel_.onInstrumentModified = [this]()
        {
            rebuildAndPublishInstrument();
        };

        // Load first factory preset if present so plugin opens with a polished urban sound
        LibraryQuery q;
        q.scope = LibraryScope::Factory;
        q.type = LibraryItemType::Preset;
        const auto factoryPresets = libraryManager_.query(q);
        if (!factoryPresets.empty())
            loadLibraryItem(factoryPresets.front());
        else
            rebuildAndPublishInstrument();

        startTimer(200);
    }

    CrayLdrAudioProcessor::~CrayLdrAudioProcessor()
    {
        stopTimer();
    }

    void CrayLdrAudioProcessor::timerCallback()
    {
        audioEngine_.collectGarbage();
    }

    void CrayLdrAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        audioEngine_.prepare(sampleRate, samplesPerBlock);
    }

    void CrayLdrAudioProcessor::releaseResources()
    {
        audioEngine_.reset();
    }

    bool CrayLdrAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void CrayLdrAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        juce::ScopedNoDenormals noDenormals;

        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (auto bpm = pos->getBpm())
                {
                    if (*bpm > 10.0 && *bpm < 400.0)
                        lastHostBpm_ = *bpm;
                }
            }
        }

        audioEngine_.processBlock(buffer, midiMessages, apvts_, lastHostBpm_, telemetry_);
    }

    void CrayLdrAudioProcessor::rebuildAndPublishInstrument()
    {
        auto report = activeInstrument_.compile(sampleDecoder_);
        missingSamples_ = report.missingFiles;
        audioEngine_.setSampleInstrument(report.compiled);

        if (!missingSamples_.isEmpty())
            statusBanner_ = "Missing " + juce::String(missingSamples_.size()) + " sample(s)";
        else if (report.compiled != nullptr && !report.compiled->zones.empty())
            statusBanner_ = activeInstrument_.name + " (" + juce::String(static_cast<int>(report.compiled->zones.size())) + " zones)";
        else
            statusBanner_ = "Synth Ready";
    }

    bool CrayLdrAudioProcessor::loadLibraryItem(const LibraryItem& item)
    {
        const juce::File f(item.filePath);
        if (!f.existsAsFile())
        {
            statusBanner_ = "File missing: " + item.name;
            notifyStateChanged();
            return false;
        }

        if (item.type == LibraryItemType::Preset)
        {
            const auto res = presetManager_.loadPresetFromFile(f, apvts_);
            if (res.success)
            {
                currentPresetMeta_ = res.metadata;
                activeInstrument_  = res.instrument;
                sampleEditorModel_.setSelectedZoneIndex(0);
                rebuildAndPublishInstrument();
                statusBanner_ = res.statusMessage;
                notifyStateChanged();
                return true;
            }
            statusBanner_ = res.statusMessage;
            notifyStateChanged();
            return false;
        }

        if (item.type == LibraryItemType::Instrument)
        {
            InstrumentDefinition def;
            if (InstrumentDefinition::loadFromFile(f, def))
            {
                activeInstrument_ = std::move(def);
                currentPresetMeta_.name     = activeInstrument_.name;
                currentPresetMeta_.category = activeInstrument_.category;
                currentPresetMeta_.author   = activeInstrument_.author;
                currentPresetMeta_.bank     = item.isFactory ? "Factory" : "User";
                currentPresetMeta_.isFactory = item.isFactory;

                // Switch to Sample/Hybrid so user hears the loaded instrument immediately
                if (auto* modeParam = apvts_.getParameter(ParamIDs::engineMode))
                    modeParam->setValueNotifyingHost(0.5f); // SampleOnly (1/2) -> wait: 3 choices (0, 0.5, 1.0) -> 0.5 is SampleOnly

                sampleEditorModel_.setSelectedZoneIndex(0);
                rebuildAndPublishInstrument();
                notifyStateChanged();
                return true;
            }
            return false;
        }

        if (item.type == LibraryItemType::Sample)
        {
            InstrumentDefinition singleSampleInst;
            singleSampleInst.name      = item.name;
            singleSampleInst.category  = item.category;
            singleSampleInst.author    = item.author;
            singleSampleInst.isFactory = item.isFactory;

            juce::Array<juce::File> files;
            files.add(f);
            instrumentBuilder_.addSamples(singleSampleInst, files, 0, true);
            if (!singleSampleInst.zones.empty())
            {
                singleSampleInst.zones[0].lowKey  = 0;
                singleSampleInst.zones[0].highKey = 127;
            }

            activeInstrument_ = std::move(singleSampleInst);
            currentPresetMeta_.name     = item.name;
            currentPresetMeta_.category = item.category;
            currentPresetMeta_.bank     = item.isFactory ? "Factory" : "User";

            if (auto* modeParam = apvts_.getParameter(ParamIDs::engineMode))
                modeParam->setValueNotifyingHost(0.5f); // SampleOnly

            sampleEditorModel_.setSelectedZoneIndex(0);
            rebuildAndPublishInstrument();
            notifyStateChanged();
            return true;
        }

        return false;
    }

    void CrayLdrAudioProcessor::resetToInitPatch()
    {
        for (auto* p : getParameters())
            p->setValueNotifyingHost(p->getDefaultValue());

        activeInstrument_ = InstrumentDefinition{};
        currentPresetMeta_ = PresetMetadata{};
        sampleEditorModel_.setSelectedZoneIndex(0);
        rebuildAndPublishInstrument();
        statusBanner_ = "Init Patch Loaded";
        notifyStateChanged();
    }

    bool CrayLdrAudioProcessor::saveCurrentPresetToUserLibrary(const juce::String& name,
                                                               SoundCategory category,
                                                               const juce::String& tags)
    {
        PresetMetadata meta;
        meta.name      = name.trim().isEmpty() ? "Untitled Urban Preset" : name.trim();
        meta.author    = "User";
        meta.category  = category;
        meta.bank      = "User";
        meta.tags      = tags;
        meta.isFactory = false;

        const auto dest = libraryManager_.getUserLibraryRoot()
            .getChildFile("Presets")
            .getChildFile(juce::File::createLegalFileName(meta.name) + ProductInfo::kPresetExtension);

        const bool ok = presetManager_.savePresetToFile(dest, meta, apvts_, activeInstrument_);
        if (ok)
        {
            currentPresetMeta_ = meta;
            statusBanner_ = "Saved preset: " + meta.name;
            notifyStateChanged();
        }
        return ok;
    }

    void CrayLdrAudioProcessor::reportStatus(const juce::String& message)
    {
        statusBanner_ = message;
        notifyStateChanged();
    }

    void CrayLdrAudioProcessor::applyImportedPack(const PresetLoadResult& result)
    {
        if (!result.success)
        {
            statusBanner_ = result.statusMessage;
            notifyStateChanged();
            return;
        }

        currentPresetMeta_ = result.metadata;
        activeInstrument_ = result.instrument;
        sampleEditorModel_.setSelectedZoneIndex(0);
        rebuildAndPublishInstrument();
        statusBanner_ = result.statusMessage;
        notifyStateChanged();
    }

    void CrayLdrAudioProcessor::relinkMissingSamplesFromFolder(const juce::File& folder)
    {
        const int relinked = PresetManager::relinkMissingSamples(activeInstrument_, folder);
        rebuildAndPublishInstrument();
        statusBanner_ = "Relinked " + juce::String(relinked) + " missing sample(s)";
        notifyStateChanged();
    }

    void CrayLdrAudioProcessor::pushUiMidiNote(int midiNote, float velocity01, bool isNoteOn) noexcept
    {
        audioEngine_.pushUiMidiEvent(midiNote, velocity01, isNoteOn);
    }

    void CrayLdrAudioProcessor::notifyStateChanged()
    {
        if (onStateChanged)
            onStateChanged();
    }

    void CrayLdrAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
    {
        auto* root = new juce::DynamicObject();
        root->setProperty("schemaVersion", ProductInfo::kStateSchemaVersion);
        root->setProperty("presetName", currentPresetMeta_.name);
        root->setProperty("presetCategory", categoryToString(currentPresetMeta_.category));
        root->setProperty("presetAuthor", currentPresetMeta_.author);
        root->setProperty("presetBank", currentPresetMeta_.bank);
        root->setProperty("presetTags", currentPresetMeta_.tags);

        if (auto xml = apvts_.copyState().createXml())
            root->setProperty("apvtsXml", xml->toString());

        root->setProperty("instrument", activeInstrument_.toJson());

        const auto jsonText = juce::JSON::toString(juce::var(root), false);
        destData.replaceAll(jsonText.toRawUTF8(), jsonText.getNumBytesAsUTF8());
    }

    void CrayLdrAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        if (data == nullptr || sizeInBytes <= 0)
            return;

        const auto jsonText = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
        const auto parsed   = juce::JSON::parse(jsonText);
        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
            return;

        const int schema = static_cast<int>(root->getProperty("schemaVersion"));
        if (schema < 1 || schema > ProductInfo::kStateSchemaVersion)
            return;

        // Decode and validate every component before changing the live processor state.
        const auto xmlStr = root->getProperty("apvtsXml").toString();
        if (xmlStr.isEmpty())
            return;

        auto xml = juce::XmlDocument::parse(xmlStr);
        if (xml == nullptr)
            return;

        auto restoredParameterTree = juce::ValueTree::fromXml(*xml);
        if (!restoredParameterTree.isValid() || !restoredParameterTree.hasType(apvts_.state.getType()))
            return;

        InstrumentDefinition candidateInstrument;
        // Older/synth-only project states may intentionally store a null instrument.
        if (root->hasProperty("instrument"))
        {
            const auto instrumentValue = root->getProperty("instrument");
            if (!instrumentValue.isVoid()
                && !InstrumentDefinition::fromJson(instrumentValue, candidateInstrument))
                return;
        }

        PresetMetadata candidateMetadata;
        candidateMetadata.name     = root->getProperty("presetName").toString();
        candidateMetadata.category = categoryFromString(root->getProperty("presetCategory").toString());
        candidateMetadata.author   = root->getProperty("presetAuthor").toString();
        candidateMetadata.bank     = root->getProperty("presetBank").toString();
        candidateMetadata.tags     = root->getProperty("presetTags").toString();

        // Commit only after the saved state has passed schema, XML and instrument checks.
        apvts_.replaceState(restoredParameterTree);
        currentPresetMeta_ = std::move(candidateMetadata);
        activeInstrument_  = std::move(candidateInstrument);

        sampleEditorModel_.setSelectedZoneIndex(0);
        rebuildAndPublishInstrument();
        notifyStateChanged();
    }

    juce::AudioProcessorEditor* CrayLdrAudioProcessor::createEditor()
    {
        return new CrayLdrAudioProcessorEditor(*this);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cray::CrayLdrAudioProcessor();
}
