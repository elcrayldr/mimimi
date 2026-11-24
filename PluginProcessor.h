#pragma once

#include "Common/ProductInfo.h"
#include "Common/ParameterIDs.h"
#include "Common/RealTimeLogger.h"
#include "Engine/AudioEngine.h"
#include "Library/SampleDecoder.h"
#include "Library/LibraryManager.h"
#include "Library/InstrumentDefinition.h"
#include "Library/SampleEditorModel.h"
#include "Library/InstrumentBuilder.h"
#include "Library/PresetManager.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

namespace cray
{
    class CrayLdrAudioProcessor : public juce::AudioProcessor,
                                  private juce::Timer
    {
    public:
        CrayLdrAudioProcessor();
        ~CrayLdrAudioProcessor() override;

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;

        bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return ProductInfo::kProductName; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 3.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return currentPresetMeta_.name; }
        void changeProgramName(int, const juce::String&) override {}

        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        // Accessors for UI and tests
        juce::AudioProcessorValueTreeState&       getApvts() noexcept { return apvts_; }
        const juce::AudioProcessorValueTreeState& getApvts() const noexcept { return apvts_; }
        AudioTelemetry&                           getTelemetry() noexcept { return telemetry_; }
        const AudioEngine&                        getAudioEngine() const noexcept { return audioEngine_; }

        SampleDecoder&        getSampleDecoder() noexcept { return sampleDecoder_; }
        LibraryManager&       getLibraryManager() noexcept { return libraryManager_; }
        InstrumentDefinition& getActiveInstrument() noexcept { return activeInstrument_; }
        SampleEditorModel&    getSampleEditorModel() noexcept { return sampleEditorModel_; }
        InstrumentBuilder&    getInstrumentBuilder() noexcept { return instrumentBuilder_; }
        PresetManager&        getPresetManager() noexcept { return presetManager_; }

        const PresetMetadata&    getCurrentPresetMetadata() const noexcept { return currentPresetMeta_; }
        const juce::StringArray& getMissingSamples() const noexcept { return missingSamples_; }
        const juce::String&      getStatusBanner() const noexcept { return statusBanner_; }

        // High-level actions called by GUI
        bool loadLibraryItem(const LibraryItem& item);
        void rebuildAndPublishInstrument();
        void resetToInitPatch();
        bool saveCurrentPresetToUserLibrary(const juce::String& name, SoundCategory category, const juce::String& tags);
        void applyImportedPack(const PresetLoadResult& result);
        void reportStatus(const juce::String& message);
        void relinkMissingSamplesFromFolder(const juce::File& folder);
        void pushUiMidiNote(int midiNote, float velocity01, bool isNoteOn) noexcept;

        /** Optional listener callback (e.g. PluginEditor refreshes UI after preset/instrument change). */
        std::function<void()> onStateChanged;

    private:
        void timerCallback() override;
        void notifyStateChanged();

        juce::AudioProcessorValueTreeState apvts_;
        AudioEngine                        audioEngine_;
        AudioTelemetry                     telemetry_;

        SampleDecoder                      sampleDecoder_;
        LibraryManager                     libraryManager_;
        InstrumentDefinition               activeInstrument_;
        SampleEditorModel                  sampleEditorModel_;
        InstrumentBuilder                  instrumentBuilder_;
        PresetManager                      presetManager_;

        PresetMetadata                     currentPresetMeta_;
        juce::StringArray                  missingSamples_;
        juce::String                       statusBanner_ { "Ready" };
        double                             lastHostBpm_ = 120.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrayLdrAudioProcessor)
    };
}
