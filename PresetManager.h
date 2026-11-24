#pragma once

#include "Library/InstrumentDefinition.h"
#include "Library/LibraryManager.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace cray
{
    struct PresetMetadata
    {
        juce::String  name     = "Init Urban Patch";
        juce::String  author   = "User";
        SoundCategory category = SoundCategory::PlucksAndBells;
        juce::String  bank     = "User";
        juce::String  tags;
        bool          isFactory = false;
    };

    struct PresetLoadResult
    {
        bool                 success = false;
        PresetMetadata       metadata;
        InstrumentDefinition instrument;
        juce::StringArray    missingSamples;
        juce::String         statusMessage;
    };

    /**
     * Manages loading/saving .craypreset files, importing/exporting presets,
     * exporting/importing portable .craypack archives (instrument + samples),
     * and relinking missing samples safely without crashes.
     */
    class PresetManager
    {
    public:
        PresetManager(SampleDecoder& decoder, LibraryManager& library);

        bool savePresetToFile(const juce::File& targetFile,
                              const PresetMetadata& meta,
                              juce::AudioProcessorValueTreeState& apvts,
                              const InstrumentDefinition& instrument);

        PresetLoadResult loadPresetFromFile(const juce::File& file,
                                            juce::AudioProcessorValueTreeState& apvts);

        bool exportPortablePack(const juce::File& packFile,
                                const PresetMetadata& meta,
                                juce::AudioProcessorValueTreeState& apvts,
                                const InstrumentDefinition& instrument);

        PresetLoadResult importPortablePack(const juce::File& packFile,
                                            juce::AudioProcessorValueTreeState& apvts,
                                            const juce::File& extractSamplesDir);

        /** Search searchDirectory recursively by filename to relink any missing zones. */
        static int relinkMissingSamples(InstrumentDefinition& instrument, const juce::File& searchDirectory);

    private:
        SampleDecoder&  decoder_;
        LibraryManager& library_;
    };
}
