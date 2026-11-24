// =============================================================================
// CRAY LDR URBAN INSTRUMENTS — TestLibraryAndInstrumentBuilder.cpp
// Tests:
//   • InstrumentBuilder MIDI note name parser
//   • Auto-mapping by root note name
//   • InstrumentDefinition JSON round-trip (serialize → deserialize)
//   • InstrumentDefinition compile() builds valid CompiledSampleInstrument
// =============================================================================

#include <JuceHeader.h>
#include "Library/InstrumentBuilder.h"
#include "Library/InstrumentDefinition.h"
#include "Engine/SampleData.h"
#include "Common/Categories.h"

class LibraryAndInstrumentBuilderTests : public juce::UnitTest
{
public:
    LibraryAndInstrumentBuilderTests() : juce::UnitTest("LibraryAndInstrumentBuilder", "Library") {}

    void runTest() override
    {
        testNoteNameParser_Standard();
        testNoteNameParser_EdgeCases();
        testNoteNameParser_InvalidNames();
        testAutoMapByRootNote_Minimal();
        testAutoMapByRootNote_KeyBoundaries();
        testInstrumentDefinitionJsonRoundTrip();
        testInstrumentDefinitionCompileEmpty();
    }

private:
    // -------------------------------------------------------------------------
    void testNoteNameParser_Standard()
    {
        beginTest("NoteNameParser: standard note names");

        expectEquals(InstrumentBuilder::parseNoteNameToMidi("C4"),  60);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("A4"),  69);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("C0"),  12);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("G9"), 127);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("D#4"), 63);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("Eb4"), 63);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("F#3"), 54);
        expectEquals(InstrumentBuilder::parseNoteNameToMidi("Bb5"), 94);
    }

    void testNoteNameParser_EdgeCases()
    {
        beginTest("NoteNameParser: edge cases (C-1, B9)");

        int cMinus1 = InstrumentBuilder::parseNoteNameToMidi("C-1");
        expect(cMinus1 == 0, "C-1 should map to MIDI 0, got " + juce::String(cMinus1));

        int b9 = InstrumentBuilder::parseNoteNameToMidi("B9");
        expect(b9 == 131 || b9 == 127, "B9 should map to 131 (or clamped to 127)");
    }

    void testNoteNameParser_InvalidNames()
    {
        beginTest("NoteNameParser: invalid names return -1");

        expect(InstrumentBuilder::parseNoteNameToMidi("") == -1,      "Empty string should return -1");
        expect(InstrumentBuilder::parseNoteNameToMidi("X4") == -1,    "X4 is not a valid note");
        expect(InstrumentBuilder::parseNoteNameToMidi("C") == -1,     "No octave should return -1");
        expect(InstrumentBuilder::parseNoteNameToMidi("12") == -1,    "Pure number should return -1");
        expect(InstrumentBuilder::parseNoteNameToMidi("Note") == -1,  "Word should return -1");
    }

    // -------------------------------------------------------------------------
    void testAutoMapByRootNote_Minimal()
    {
        beginTest("AutoMap: 3 samples span keys correctly");

        InstrumentBuilder builder;

        // Fake sample specs
        SampleZoneSpec a4, c5, e5;
        a4.filePath = juce::File("/fake/piano_A4.wav");
        a4.rootNote = 69;   // A4
        c5.filePath = juce::File("/fake/piano_C5.wav");
        c5.rootNote = 72;   // C5
        e5.filePath = juce::File("/fake/piano_E5.wav");
        e5.rootNote = 76;   // E5

        builder.addZoneSpec(a4);
        builder.addZoneSpec(c5);
        builder.addZoneSpec(e5);

        builder.autoMapZonesByRootNote();

        const auto& zones = builder.getZoneSpecs();
        expectEquals((int)zones.size(), 3);

        // Key ranges should be non-overlapping and cover 0–127 collectively
        // Check that all zone ranges are valid (lo ≤ root ≤ hi)
        for (const auto& z : zones)
        {
            expect(z.keyLo <= z.rootNote, "keyLo > rootNote for note " + juce::String(z.rootNote));
            expect(z.keyHi >= z.rootNote, "keyHi < rootNote for note " + juce::String(z.rootNote));
        }

        // Verify no overlap between zones
        for (int i = 0; i < (int)zones.size() - 1; ++i)
        {
            expect(zones[i].keyHi < zones[i + 1].keyLo,
                   "Zone " + juce::String(i) + " overlaps zone " + juce::String(i + 1));
        }
    }

    void testAutoMapByRootNote_KeyBoundaries()
    {
        beginTest("AutoMap: boundary zones reach MIDI 0 and 127");

        InstrumentBuilder builder;

        SampleZoneSpec c3, c4, c5;
        c3.filePath = juce::File("/fake/s_C3.wav"); c3.rootNote = 48;
        c4.filePath = juce::File("/fake/s_C4.wav"); c4.rootNote = 60;
        c5.filePath = juce::File("/fake/s_C5.wav"); c5.rootNote = 72;

        builder.addZoneSpec(c3);
        builder.addZoneSpec(c4);
        builder.addZoneSpec(c5);
        builder.autoMapZonesByRootNote();

        const auto& zones = builder.getZoneSpecs();
        expectEquals((int)zones.size(), 3);

        // Lowest zone must start at 0
        expect(zones[0].keyLo == 0, "Lowest zone keyLo != 0");
        // Highest zone must end at 127
        expect(zones[2].keyHi == 127, "Highest zone keyHi != 127");
    }

    // -------------------------------------------------------------------------
    void testInstrumentDefinitionJsonRoundTrip()
    {
        beginTest("InstrumentDefinition: JSON serialize → deserialize is lossless");

        InstrumentDefinition def;
        def.id = juce::Uuid().toDashedString();
        def.name = "Test Piano";
        def.category = SoundCategory::KeysAndPianos;
        def.author = "CRAY LDR Test";
        def.tags = "piano keys test";

        SampleZoneSpec spec;
        spec.filePath = juce::File("C:/samples/piano_C4.wav");
        spec.rootNote = 60;
        spec.keyLo = 48; spec.keyHi = 71;
        spec.velLo = 0;  spec.velHi = 127;
        spec.layer = 0;
        spec.pitchOffsetSemitones = 0.0f;
        spec.startFrame = 0; spec.endFrame = -1;
        spec.loopEnabled = false;
        spec.reverse = false;
        spec.gain = 1.0f; spec.pan = 0.0f;
        def.zones.push_back(spec);

        // Serialize
        juce::var json = def.toJson();
        juce::String jsonStr = juce::JSON::toString(json, true);
        expect(jsonStr.isNotEmpty(), "JSON serialization produced empty string");

        // Deserialize
        juce::var parsed;
        juce::Result r = juce::JSON::parse(jsonStr, parsed);
        expect(r.wasOk(), "JSON parse failed: " + r.getErrorMessage());

        InstrumentDefinition def2;
        def2.fromJson(parsed);

        expectEquals(def2.id, def.id, "ID mismatch after round-trip");
        expectEquals(def2.name, def.name, "Name mismatch");
        expect(def2.category == def.category, "Category mismatch");
        expectEquals(def2.author, def.author, "Author mismatch");
        expectEquals((int)def2.zones.size(), 1, "Zone count mismatch");
        expectEquals(def2.zones[0].rootNote, 60, "Zone rootNote mismatch");
        expectEquals(def2.zones[0].keyLo, 48, "Zone keyLo mismatch");
        expectEquals(def2.zones[0].keyHi, 71, "Zone keyHi mismatch");
    }

    void testInstrumentDefinitionCompileEmpty()
    {
        beginTest("InstrumentDefinition::compile(): empty zones produces valid empty instrument");

        InstrumentDefinition def;
        def.id = juce::Uuid().toDashedString();
        def.name = "Empty";
        def.category = SoundCategory::PlucksAndBells;
        def.author = "Test";

        // compile() with no zones should not crash and return a valid pointer
        auto compiled = def.compile(juce::File::getSpecialLocation(juce::File::tempDirectory));
        expect(compiled != nullptr, "compile() returned nullptr for empty definition");

        if (compiled != nullptr)
        {
            expectEquals((int)compiled->zones.size(), 0,
                "Compiled empty instrument should have 0 zones");
        }
    }
};

static LibraryAndInstrumentBuilderTests gLibraryBuilderTests;
