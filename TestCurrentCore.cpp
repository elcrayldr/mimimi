#include <JuceHeader.h>
#include <cmath>

#include "DSP/Envelopes/AhdsrEnvelope.h"
#include "DSP/Oscillators/PolyBlepOscillator.h"
#include "Engine/SampleEngine.h"
#include "Library/InstrumentDefinition.h"
#include "Library/InstrumentBuilder.h"

namespace
{
    class CurrentCoreTests final : public juce::UnitTest
    {
    public:
        CurrentCoreTests() : juce::UnitTest("CurrentCoreSmokeTests", "CRAY LDR") {}

        void runTest() override
        {
            testOscillatorOutput();
            testEnvelopeLifecycle();
            testInstrumentDefinitionRoundTrip();
            testBuilderNoteDetectionAndKeyMapping();
            testSampleVoicePlaybackAndPinning();
            testSampleVoiceLoopFilterAndRelease();
        }

    private:
        void testOscillatorOutput()
        {
            beginTest("PolyBLEP oscillator produces finite bounded samples");
            cray::PolyBlepOscillator oscillator;
            oscillator.prepare(48000.0);
            oscillator.setWaveform(cray::OscWaveform::Saw);
            oscillator.setFrequency(440.0f);

            float peak = 0.0f;
            for (int i = 0; i < 4096; ++i)
            {
                const float value = oscillator.processSample();
                expect(std::isfinite(value), "Oscillator emitted NaN/Inf");
                peak = juce::jmax(peak, std::abs(value));
            }
            expect(peak > 0.1f, "Oscillator output is unexpectedly silent");
            expect(peak < 1.2f, "Oscillator output exceeded the expected bounded range");
        }

        void testEnvelopeLifecycle()
        {
            beginTest("AHDSR envelope enters attack and returns to idle after release");
            cray::AhdsrEnvelope envelope;
            envelope.prepare(48000.0);
            envelope.setParameters(0.002f, 0.01f, 0.5f, 0.01f);
            envelope.noteOn();

            float peak = 0.0f;
            for (int i = 0; i < 2048; ++i)
                peak = juce::jmax(peak, envelope.getNextSample());

            expect(envelope.isActive(), "Envelope unexpectedly idle before note-off");
            expect(peak > 0.1f, "Envelope did not rise during attack");
            envelope.noteOff();
            for (int i = 0; i < 8192 && envelope.isActive(); ++i)
                envelope.getNextSample();
            expect(!envelope.isActive(), "Envelope did not reach idle after release");
        }

        void testInstrumentDefinitionRoundTrip()
        {
            beginTest("InstrumentDefinition JSON round-trip preserves zone mapping");
            cray::InstrumentDefinition source;
            source.name = "Unit Test Instrument";
            source.author = "CRAY LDR Tests";

            cray::SampleZoneSpec zone;
            zone.zoneId = "zone-test";
            zone.sourceFilePath = "Samples/C4.wav";
            zone.displayName = "C4";
            zone.rootNote = 60;
            zone.lowKey = 48;
            zone.highKey = 71;
            zone.lowVelocity = 1;
            zone.highVelocity = 100;
            zone.layerIndex = 0;
            source.zones.push_back(zone);

            cray::InstrumentDefinition restored;
            const bool ok = cray::InstrumentDefinition::fromJson(source.toJson(), restored);
            expect(ok, "InstrumentDefinition::fromJson rejected its own serialized data");
            if (ok)
            {
                expect(restored.name == source.name, "Instrument name did not survive round-trip");
                expectEquals(static_cast<int>(restored.zones.size()), 1);
                if (!restored.zones.empty())
                {
                    expectEquals(restored.zones[0].rootNote, 60);
                    expectEquals(restored.zones[0].lowKey, 48);
                    expectEquals(restored.zones[0].highKey, 71);
                    expect(restored.zones[0].sourceFilePath == "Samples/C4.wav",
                           "Relative sample path changed during round-trip");
                }
            }
        }

        void testSampleVoiceLoopFilterAndRelease()
        {
            beginTest("SampleVoice loops with filtering and stops after envelope release");
            auto sample = std::make_shared<cray::SampleBuffer>();
            sample->sampleRate = 48000.0;
            sample->numChannels = 1;
            sample->numSamples = 4096;
            sample->audio.setSize(1, sample->numSamples);
            auto* data = sample->audio.getWritePointer(0);
            for (int i = 0; i < sample->numSamples; ++i)
                data[i] = 0.2f * std::sin(juce::MathConstants<double>::twoPi * 220.0 * i / 48000.0);

            cray::CompiledSampleInstrument instrument;
            instrument.layerEnabled.push_back(true);
            instrument.layerGain.push_back(1.0f);

            cray::CompiledSampleZone zone;
            zone.sample = sample;
            zone.layerIndex = 0;
            zone.rootNote = 57;
            zone.lowKey = 0;
            zone.highKey = 127;
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
            zone.startSample = 0;
            zone.endSample = sample->numSamples - 1;
            zone.loopEnabled = true;
            zone.loopStartSample = 256;
            zone.loopEndSample = 2048;
            zone.loopCrossfade = 128;
            zone.filterEnabled = true;
            zone.filterType = cray::SvfFilterType::LowPass12;
            zone.filterCutoffHz = 9000.0f;
            zone.filterResonance = 0.25f;
            zone.attackSec = 0.001f;
            zone.decaySec = 0.01f;
            zone.sustainLevel = 0.8f;
            zone.releaseSec = 0.01f;
            instrument.zones.push_back(zone);

            cray::SampleVoice voice;
            voice.prepare(48000.0);
            voice.startNote(&instrument, 57, 1.0f, false);
            expect(voice.isActive(), "Looping sample voice did not start");

            cray::ModOffsets offsets{};
            cray::SampleGlobalParams params;
            for (int i = 0; i < 1024; ++i)
            {
                float left = 0.0f, right = 0.0f;
                voice.renderSample(params, 57.0f, 0.0f, offsets, left, right);
                expect(std::isfinite(left) && std::isfinite(right), "Filtered loop produced NaN/Inf");
            }

            expect(voice.isActive(), "Looped sample stopped before note-off");
            voice.releaseNote();
            for (int i = 0; i < 4096 && voice.isActive(); ++i)
            {
                float left = 0.0f, right = 0.0f;
                voice.renderSample(params, 57.0f, 0.0f, offsets, left, right);
            }
            expect(!voice.isActive(), "Looped sample did not stop after release envelope");
            expectEquals(instrument.audioRefCount.load(), 0);
        }

        void testBuilderNoteDetectionAndKeyMapping()
        {
            beginTest("Instrument Builder detects note names and maps adjacent key zones");
            const auto c1 = cray::InstrumentBuilder::parseMidiNoteFromName("808_Kick_C1.wav");
            const auto fs3 = cray::InstrumentBuilder::parseMidiNoteFromName("Lead_F#3.wav");
            const auto bb2 = cray::InstrumentBuilder::parseMidiNoteFromName("Pad_Bb2.wav");
            const auto falseMatch = cray::InstrumentBuilder::parseMidiNoteFromName("BASS.wav");

            expect(c1.has_value() && *c1 == 24, "C1 filename was not detected as MIDI 24");
            expect(fs3.has_value() && *fs3 == 54, "F#3 filename was not detected as MIDI 54");
            expect(bb2.has_value() && *bb2 == 46, "Bb2 filename was not detected as MIDI 46");
            expect(!falseMatch.has_value(), "Parser incorrectly treated a word as a note name");

            cray::InstrumentDefinition instrument;
            instrument.zones.clear();
            for (const int root : { 36, 48, 60, 72 })
            {
                cray::SampleZoneSpec zone;
                zone.rootNote = root;
                zone.layerIndex = 0;
                instrument.zones.push_back(zone);
            }

            cray::InstrumentBuilder::autoMapZonesByRootNote(instrument, 0);
            expectEquals(instrument.zones[0].lowKey, 0);
            expectEquals(instrument.zones[0].highKey, 42);
            expectEquals(instrument.zones[1].lowKey, 43);
            expectEquals(instrument.zones[1].highKey, 54);
            expectEquals(instrument.zones[2].lowKey, 55);
            expectEquals(instrument.zones[2].highKey, 66);
            expectEquals(instrument.zones[3].lowKey, 67);
            expectEquals(instrument.zones[3].highKey, 127);
        }

        void testSampleVoicePlaybackAndPinning()
        {
            beginTest("SampleVoice renders a matching zone and releases its instrument pin");
            auto sample = std::make_shared<cray::SampleBuffer>();
            sample->sampleRate = 44100.0;
            sample->numChannels = 1;
            sample->numSamples = 4096;
            sample->audio.setSize(1, sample->numSamples);
            auto* data = sample->audio.getWritePointer(0);
            for (int i = 0; i < sample->numSamples; ++i)
                data[i] = 0.25f * std::sin(juce::MathConstants<double>::twoPi * 440.0 * i / 44100.0);

            cray::CompiledSampleInstrument instrument;
            instrument.layerEnabled.push_back(true);
            instrument.layerGain.push_back(1.0f);

            cray::CompiledSampleZone zone;
            zone.sample = sample;
            zone.layerIndex = 0;
            zone.rootNote = 69;
            zone.lowKey = 0;
            zone.highKey = 127;
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
            zone.startSample = 0;
            zone.endSample = sample->numSamples - 1;
            instrument.zones.push_back(zone);

            cray::SampleVoice voice;
            voice.prepare(44100.0);
            voice.startNote(&instrument, 69, 1.0f, false);
            expectEquals(instrument.audioRefCount.load(), 1);
            expect(voice.isActive(), "SampleVoice did not activate for a matching zone");

            cray::ModOffsets offsets{};
            cray::SampleGlobalParams params;
            float maxPeak = 0.0f;
            for (int i = 0; i < 512; ++i)
            {
                float left = 0.0f, right = 0.0f;
                voice.renderSample(params, 69.0f, 0.0f, offsets, left, right);
                maxPeak = juce::jmax(maxPeak, juce::jmax(std::abs(left), std::abs(right)));
            }
            expect(maxPeak > 0.001f, "SampleVoice rendered silence");
            voice.reset();
            expectEquals(instrument.audioRefCount.load(), 0);
        }
    };

    CurrentCoreTests currentCoreTests;
}
