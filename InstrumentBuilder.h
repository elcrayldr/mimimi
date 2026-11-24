#pragma once

#include "Library/InstrumentDefinition.h"
#include "Library/LibraryManager.h"
#include "Library/SampleDecoder.h"
#include <optional>

namespace cray
{
    struct BuildInstrumentResult
    {
        bool         success = false;
        juce::File   savedInstrumentFile;
        CompileInstrumentReport compileReport;
        juce::String statusMessage;
    };

    /**
     * Multi-sample Instrument Builder:
     * - Note-name parser & auto-mapping across key & velocity zones
     * - Layer management (4 initial layers, expandable)
     * - Real BUILD INSTRUMENT action that compiles, validates, saves, and activates playback
     */
    class InstrumentBuilder
    {
    public:
        InstrumentBuilder(SampleDecoder& decoder, LibraryManager& library);

        /** Parse a note name (e.g., "C3", "F#4", "Bb2") or MIDI note number from filename. */
        static std::optional<int> parseMidiNoteFromName(const juce::String& filename);
        static juce::String midiNoteToName(int midiNote);

        /** Add multiple sample files into draft instrument on targetLayer, auto-detecting root notes. */
        int addSamples(InstrumentDefinition& draft,
                       const juce::Array<juce::File>& files,
                       int targetLayer = 0,
                       bool autoDetectRootNotes = true);

        /**
         * Distribute lowKey/highKey cleanly across [0..127] sorted by rootNote.
         * @param layerIndex If >= 0, maps only that layer; if < 0, maps every layer independently.
         */
        static void autoMapZonesByRootNote(InstrumentDefinition& draft, int layerIndex = -1);

        /**
         * Execute the real BUILD INSTRUMENT pipeline:
         * validates zones, compiles SampleBuffers into a lock-free CompiledSampleInstrument,
         * saves .crayinst to User Library, and registers it in LibraryManager.
         */
        BuildInstrumentResult buildInstrument(InstrumentDefinition& draft, bool saveToUserLibrary = true);

    private:
        SampleDecoder&  decoder_;
        LibraryManager& library_;
    };
}
