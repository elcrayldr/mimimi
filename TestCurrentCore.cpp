#include <JuceHeader.h>

#include "DSP/Envelopes/AhdsrEnvelope.h"
#include "DSP/Oscillators/PolyBlepOscillator.h"
#include "Engine/SampleEngine.h"
#include "Library/InstrumentDefinition.h"

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
            testSampleVoicePlaybackAndPinning();
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
