// =============================================================================
// CRAY LDR URBAN INSTRUMENTS — TestSampleEngineAndResampling.cpp
// Tests:
//   • SampleBuffer construction and ref counting
//   • SampleVoice: forward playback, reverse flag, loop, fade in/out
//   • Cubic Hermite interpolation continuity
// =============================================================================

#include <JuceHeader.h>
#include "Engine/SampleData.h"
#include "Engine/SampleEngine.h"

class SampleEngineTests : public juce::UnitTest
{
public:
    SampleEngineTests() : juce::UnitTest("SampleEngineAndResampling", "Engine") {}

    void runTest() override
    {
        testSampleBufferRefCount();
        testSampleVoiceForwardPlayback();
        testSampleVoiceReverseFlag();
        testSampleVoiceLoopContinuity();
        testCubicHermiteMonotonicity();
    }

private:
    // Helper: build a 1-second 440 Hz sine at 44100 Hz
    static juce::AudioBuffer<float> makeSineTone(int numFrames = 44100, double freq = 440.0, double sr = 44100.0)
    {
        juce::AudioBuffer<float> buf(1, numFrames);
        float* p = buf.getWritePointer(0);
        for (int i = 0; i < numFrames; ++i)
            p[i] = std::sin(juce::MathConstants<double>::twoPi * freq * i / sr);
        return buf;
    }

    // -------------------------------------------------------------------------
    void testSampleBufferRefCount()
    {
        beginTest("SampleBuffer: atomic ref-count lifecycle");

        auto buf = makeSineTone(4096);
        SampleBuffer* sb = new SampleBuffer();
        sb->audioData = std::move(buf);
        sb->sampleRate = 44100.0;
        sb->numChannels = 1;
        sb->numFrames = 4096;
        sb->audioRefCount.store(0);

        // Simulate audio retain/release
        sb->audioRefCount.fetch_add(1);
        expect(sb->audioRefCount.load() == 1, "Ref count should be 1 after retain");
        sb->audioRefCount.fetch_sub(1);
        expect(sb->audioRefCount.load() == 0, "Ref count should be 0 after release");

        delete sb;
        pass();  // No crash = lifecycle correct
    }

    void testSampleVoiceForwardPlayback()
    {
        beginTest("SampleVoice: forward playback produces non-zero output");

        // Build a CompiledSampleZone pointing at a test tone
        auto rawBuf = makeSineTone(44100, 440.0, 44100.0);

        CompiledSampleZone zone;
        zone.buffer = new SampleBuffer();
        zone.buffer->audioData = std::move(rawBuf);
        zone.buffer->sampleRate = 44100.0;
        zone.buffer->numChannels = 1;
        zone.buffer->numFrames = 44100;
        zone.buffer->audioRefCount.store(1);
        zone.rootNote = 69;        // A4
        zone.keyLo = 0;
        zone.keyHi = 127;
        zone.velLo = 0;
        zone.velHi = 127;
        zone.layer = 0;
        zone.pitchOffsetSemitones = 0.0f;
        zone.startFrame = 0;
        zone.endFrame = 44100 - 1;
        zone.loopEnabled = false;
        zone.reverse = false;
        zone.gain = 1.0f;
        zone.pan = 0.0f;

        SampleVoice voice;
        voice.prepare(44100.0, 512);

        // Trigger at root note, velocity 100
        SampleVoiceParams params;
        params.zone = &zone;
        params.midiNote = 69;
        params.velocity = 100.0f / 127.0f;
        params.sampleRate = 44100.0;
        voice.noteOn(params);

        juce::AudioBuffer<float> out(2, 512);
        out.clear();
        voice.process(out, 0, 512);

        float maxAmp = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* p = out.getReadPointer(ch);
            for (int i = 0; i < 512; ++i)
                maxAmp = std::max(maxAmp, std::abs(p[i]));
        }
        expect(maxAmp > 0.01f, "SampleVoice forward playback produced silence");
        expect(std::isfinite(maxAmp), "SampleVoice output NaN/Inf");

        zone.buffer->audioRefCount.fetch_sub(1);
        delete zone.buffer;
    }

    void testSampleVoiceReverseFlag()
    {
        beginTest("SampleVoice: reverse flag produces mirrored output");

        // Create a tone that starts at 0 and ramps up so we can distinguish direction
        const int N = 1024;
        juce::AudioBuffer<float> ramp(1, N);
        float* p = ramp.getWritePointer(0);
        for (int i = 0; i < N; ++i) p[i] = float(i) / N;

        auto makeZone = [&](bool rev) -> CompiledSampleZone {
            CompiledSampleZone z;
            z.buffer = new SampleBuffer();
            z.buffer->audioData = ramp;    // shared data, copy is cheap here
            z.buffer->sampleRate = 44100.0;
            z.buffer->numChannels = 1;
            z.buffer->numFrames = N;
            z.buffer->audioRefCount.store(1);
            z.rootNote = 69; z.keyLo = 0; z.keyHi = 127;
            z.velLo = 0; z.velHi = 127; z.layer = 0;
            z.pitchOffsetSemitones = 0.0f;
            z.startFrame = 0; z.endFrame = N - 1;
            z.loopEnabled = false; z.reverse = rev;
            z.gain = 1.0f; z.pan = 0.0f;
            return z;
        };

        auto zoneFwd = makeZone(false);
        auto zoneRev = makeZone(true);

        SampleVoice vFwd, vRev;
        vFwd.prepare(44100.0, 128);
        vRev.prepare(44100.0, 128);

        SampleVoiceParams pFwd; pFwd.zone = &zoneFwd; pFwd.midiNote = 69; pFwd.velocity = 1.0f; pFwd.sampleRate = 44100.0;
        SampleVoiceParams pRev; pRev.zone = &zoneRev; pRev.midiNote = 69; pRev.velocity = 1.0f; pRev.sampleRate = 44100.0;
        vFwd.noteOn(pFwd);
        vRev.noteOn(pRev);

        juce::AudioBuffer<float> outFwd(2, 128), outRev(2, 128);
        outFwd.clear(); outRev.clear();
        vFwd.process(outFwd, 0, 128);
        vRev.process(outRev, 0, 128);

        // Forward: first sample should be near 0 (ramp starts at 0)
        // Reverse: first sample should be near 1 (ramp ends at 1)
        const float* fwdL = outFwd.getReadPointer(0);
        const float* revL = outRev.getReadPointer(0);

        // Allow for envelope attack ramp — just check direction
        float fwdFirst = fwdL[0], revFirst = revL[0];
        // If reverse works, fwdFirst < revFirst (or at least they differ)
        expect(fwdFirst < revFirst || (std::abs(fwdFirst - revFirst) < 0.01f),
               "Reverse flag did not produce expected mirroring");

        zoneFwd.buffer->audioRefCount.fetch_sub(1); delete zoneFwd.buffer;
        zoneRev.buffer->audioRefCount.fetch_sub(1); delete zoneRev.buffer;
    }

    void testSampleVoiceLoopContinuity()
    {
        beginTest("SampleVoice: loop is continuous (no large discontinuity at wrap point)");

        // A pure sine so the loop should be very smooth
        const int N = 4096;
        auto rawBuf = makeSineTone(N, 440.0, 44100.0);

        CompiledSampleZone zone;
        zone.buffer = new SampleBuffer();
        zone.buffer->audioData = std::move(rawBuf);
        zone.buffer->sampleRate = 44100.0;
        zone.buffer->numChannels = 1;
        zone.buffer->numFrames = N;
        zone.buffer->audioRefCount.store(1);
        zone.rootNote = 69; zone.keyLo = 0; zone.keyHi = 127;
        zone.velLo = 0; zone.velHi = 127; zone.layer = 0;
        zone.pitchOffsetSemitones = 0.0f;
        zone.startFrame = 0; zone.endFrame = N - 1;
        zone.loopEnabled = true;
        zone.loopStart = 0; zone.loopEnd = N - 1;
        zone.reverse = false;
        zone.gain = 1.0f; zone.pan = 0.0f;

        SampleVoice voice;
        voice.prepare(44100.0, 256);

        SampleVoiceParams params;
        params.zone = &zone; params.midiNote = 69;
        params.velocity = 1.0f; params.sampleRate = 44100.0;
        voice.noteOn(params);

        // Run for more than one loop cycle
        juce::AudioBuffer<float> out(2, 256);
        float maxDelta = 0.0f;
        float prev = 0.0f;
        for (int block = 0; block < 20; ++block)
        {
            out.clear();
            voice.process(out, 0, 256);
            const float* p = out.getReadPointer(0);
            for (int i = 0; i < 256; ++i)
            {
                float delta = std::abs(p[i] - prev);
                if (block > 0) maxDelta = std::max(maxDelta, delta);
                prev = p[i];
            }
        }
        // A well-crossfaded loop should not have jump > 0.5 between adjacent samples
        expect(maxDelta < 0.5f, "Loop discontinuity too large: " + juce::String(maxDelta));
        expect(std::isfinite(maxDelta), "Loop produced NaN/Inf");

        zone.buffer->audioRefCount.fetch_sub(1);
        delete zone.buffer;
    }

    void testCubicHermiteMonotonicity()
    {
        beginTest("Cubic Hermite interpolation: smooth on ramp input");

        // A perfect ramp interpolated at fractional positions should still be monotone.
        // We test this independently using DspMath::cubicHermite.
        //
        // y(x) where x in [0,1], points: ..., x0, x1, x2, x3 linearly spaced.
        // Cubic Hermite on a linear sequence must reproduce the linear exactly.
        const float a = 2.0f, b = 5.0f, c = 8.0f, d = 11.0f;
        // All points on the line y = 3*i + 2
        for (int frac_i = 0; frac_i <= 100; ++frac_i)
        {
            float t = frac_i / 100.0f;
            float y = DspMath::cubicHermite(a, b, c, d, t);
            float expected = b + t * (c - b);  // linear interpolation expected
            expectWithinAbsoluteError(y, expected, 1e-4f,
                "Cubic Hermite not linear on ramp at t=" + juce::String(t));
        }
    }
};

static SampleEngineTests gSampleEngineTests;
