#pragma once

#include "Common/Categories.h"
#include "Common/ProductInfo.h"
#include "DSP/Filters/ZdfSvfFilter.h"
#include "Engine/SampleData.h"
#include "Library/SampleDecoder.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace cray
{
    struct LayerConfig
    {
        juce::String name    = "Layer 1";
        bool         enabled = true;
        float        gain    = 1.0f;
    };

    struct SampleZoneSpec
    {
        juce::String  zoneId;
        juce::String  sourceFilePath;
        juce::String  displayName;
        bool          missingSourceFile = false;

        int           layerIndex        = 0;
        int           rootNote          = 60;
        bool          rootNoteKnown     = false;
        int           lowKey            = 0;
        int           highKey           = 127;
        int           lowVelocity       = 1;
        int           highVelocity      = 127;

        int           pitchSemitones    = 0;
        float         fineTuneCents     = 0.0f;
        bool          reverse           = false;

        int           startSample       = 0;
        int           endSample         = 0; // 0 = auto full length on load
        bool          loopEnabled       = false;
        int           loopStartSample   = 0;
        int           loopEndSample     = 0;
        int           loopCrossfade     = 256;

        int           fadeInSamples     = 0;
        int           fadeOutSamples    = 0;

        float         gainDb            = 0.0f;
        float         normalizeGainDb   = 0.0f;
        float         pan               = 0.0f;

        float         attackSec         = 0.005f;
        float         decaySec          = 0.25f;
        float         sustainLevel      = 1.0f;
        float         releaseSec        = 0.20f;

        bool          filterEnabled     = false;
        SvfFilterType filterType        = SvfFilterType::LowPass12;
        float         filterCutoffHz    = 18000.0f;
        float         filterResonance   = 0.1f;
    };

    struct CompileInstrumentReport
    {
        std::shared_ptr<const CompiledSampleInstrument> compiled;
        juce::StringArray missingFiles;
        juce::StringArray errors;
    };

    /**
     * Editable multisample instrument definition saved as .crayinst JSON.
     * Supports 4 initial layers with dynamically expandable layer vectors.
     */
    class InstrumentDefinition
    {
    public:
        InstrumentDefinition();

        juce::String             id;
        juce::String             name        = "Init Urban Instrument";
        juce::String             author      = "User";
        SoundCategory            category    = SoundCategory::KeysAndPianos;
        juce::String             tags;
        bool                     isFactory   = false;

        std::vector<LayerConfig>    layers;
        std::vector<SampleZoneSpec> zones;

        void ensureLayerExists(int layerIndex);
        CompileInstrumentReport compile(SampleDecoder& decoder, const juce::File& baseDir = juce::File()) const;

        juce::var toJson(const juce::File& relativeToDir = juce::File()) const;
        static bool fromJson(const juce::var& json, InstrumentDefinition& outDef, const juce::File& baseDir = juce::File());

        bool saveToFile(const juce::File& file, bool useRelativeSamplePaths = false) const;
        static bool loadFromFile(const juce::File& file, InstrumentDefinition& outDef);
    };
}
