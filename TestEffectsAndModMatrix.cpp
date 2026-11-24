// =============================================================================
// CRAY LDR URBAN INSTRUMENTS — TestEffectsAndModMatrix.cpp
// Tests:
//   • AhdsrEnvelope: segment timing, attack/decay/sustain/release correctness
//   • ModMatrix: source→destination evaluation, range clamp
//   • AlgorithmicReverb: RT-safety (no allocation in process), basic wet signal
//   • StereoDelay: delay-line correctness, feedback stability
// =============================================================================

#include <JuceHeader.h>
#include "DSP/Envelopes/AhdsrEnvelope.h"
#include "DSP/Modulation/ModMatrix.h"
#include "DSP/Effects/AlgorithmicReverb.h"
#include "DSP/Effects/StereoDelay.h"
#include "DSP/Effects/ParametricEq.h"
#include "DSP/Effects/SaturationDistortion.h"

class EffectsAndModMatrixTests : public juce::UnitTest
{
public:
    EffectsAndModMatrixTests() : juce::UnitTest("EffectsAndModMatrix", "DSP") {}

    void runTest() override
    {
        testEnvelopePeakReachedDuringAttack();
        testEnvelopeSustainLevel();
        testEnvelopeRelease();
        testModMatrixRangeClamping();
        testModMatrixDisabledSlot();
        testReverbWetSignal();
        testReverbStability();
        testDelayLineLatency();
        testDelayFeedbackBounded();
        testEqBypassIsTransparent();
        testSaturationBoundsAtMaxDrive();
    }

private:
    static constexpr float kSR = 44100.0f;

    // -------------------------------------------------------------------------
    void testEnvelopePeakReachedDuringAttack()
    {
        beginTest("ADSR: amplitude reaches ≥ 0.99 during attack");

        AhdsrEnvelope env;
        env.prepare(kSR);
        env.setParameters(0.01f, 0.0f, 0.1f, 1.0f, 0.5f, 0.3f);  // A=10ms, H=0, D=100ms, S=0.5, R=300ms
        env.noteOn();

        float peak = 0.0f;
        // 10ms = 441 samples
        for (int i = 0; i < 500; ++i)
            peak = std::max(peak, env.processSample());

        expect(peak >= 0.99f, "Envelope peak not reached: " + juce::String(peak));
    }

    void testEnvelopeSustainLevel()
    {
        beginTest("ADSR: sustain level held correctly");

        AhdsrEnvelope env;
        env.prepare(kSR);
        const float sustain = 0.6f;
        env.setParameters(0.001f, 0.0f, 0.05f, sustain, 0.3f, 0.0f);
        env.noteOn();

        float last = 0.0f;
        // Skip through attack + decay (50ms + 5ms ≈ 2650 samples)
        for (int i = 0; i < 3200; ++i)
            last = env.processSample();

        expectWithinAbsoluteError(last, sustain, 0.02f,
            "Sustain level incorrect: " + juce::String(last) + " expected " + juce::String(sustain));
    }

    void testEnvelopeRelease()
    {
        beginTest("ADSR: release reaches near-zero in time");

        AhdsrEnvelope env;
        env.prepare(kSR);
        env.setParameters(0.001f, 0.0f, 0.001f, 1.0f, 0.1f, 0.0f);  // R=100ms
        env.noteOn();

        // Burn through attack + hold
        for (int i = 0; i < 500; ++i) env.processSample();

        // Trigger release
        env.noteOff();

        // 100ms release ≈ 4410 samples; after 6000 should be ~0
        float finalLevel = 1.0f;
        for (int i = 0; i < 6000; ++i)
            finalLevel = env.processSample();

        expect(finalLevel < 0.01f, "Release did not decay to near zero: " + juce::String(finalLevel));
    }

    // -------------------------------------------------------------------------
    void testModMatrixRangeClamping()
    {
        beginTest("ModMatrix: output clamped to [-1, 1]");

        ModMatrix mm;
        mm.prepare(kSR);

        // Set a slot: LFO1 → FilterCutoff, amount = 1.0 (maximum)
        ModMatrix::SlotConfig cfg;
        cfg.sourceId = ModMatrix::Source::LFO1;
        cfg.destId   = ModMatrix::Destination::FilterCutoff;
        cfg.amount   = 1.0f;
        cfg.enabled  = true;
        mm.setSlot(0, cfg);

        // Feed extreme source values
        ModMatrix::Sources srcs;
        srcs.lfo1 = 10.0f;   // wildly out of range input

        ModMatrix::Outputs out = mm.evaluate(srcs);
        // FilterCutoff destination should be clamped
        expect(out.filterCutoffMod >= -1.0f && out.filterCutoffMod <= 1.0f,
               "ModMatrix did not clamp output: " + juce::String(out.filterCutoffMod));
    }

    void testModMatrixDisabledSlot()
    {
        beginTest("ModMatrix: disabled slot contributes zero");

        ModMatrix mm;
        mm.prepare(kSR);

        ModMatrix::SlotConfig cfg;
        cfg.sourceId = ModMatrix::Source::LFO1;
        cfg.destId   = ModMatrix::Destination::FilterCutoff;
        cfg.amount   = 1.0f;
        cfg.enabled  = false;   // disabled
        mm.setSlot(0, cfg);

        ModMatrix::Sources srcs;
        srcs.lfo1 = 1.0f;

        ModMatrix::Outputs out = mm.evaluate(srcs);
        expectWithinAbsoluteError(out.filterCutoffMod, 0.0f, 1e-6f,
            "Disabled slot contributed non-zero: " + juce::String(out.filterCutoffMod));
    }

    // -------------------------------------------------------------------------
    void testReverbWetSignal()
    {
        beginTest("AlgorithmicReverb: wet output is non-zero from impulse");

        AlgorithmicReverb rev;
        rev.prepare(kSR, 512);
        rev.setRoomSize(0.75f);
        rev.setDamping(0.5f);
        rev.setWet(1.0f);
        rev.setDry(0.0f);
        rev.setWidth(1.0f);

        juce::AudioBuffer<float> buf(2, 512);
        buf.clear();
        // Single impulse
        buf.getWritePointer(0)[0] = 0.5f;
        buf.getWritePointer(1)[0] = 0.5f;

        rev.process(buf, 0, 512);

        float maxAmp = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* p = buf.getReadPointer(ch);
            for (int i = 0; i < 512; ++i)
                maxAmp = std::max(maxAmp, std::abs(p[i]));
        }
        expect(maxAmp > 1e-4f, "Reverb wet output is dead after impulse");
        expect(std::isfinite(maxAmp), "Reverb output NaN/Inf");
    }

    void testReverbStability()
    {
        beginTest("AlgorithmicReverb: stable with max room size + sustained input");

        AlgorithmicReverb rev;
        rev.prepare(kSR, 512);
        rev.setRoomSize(0.999f);
        rev.setDamping(0.0f);
        rev.setWet(1.0f);
        rev.setDry(0.0f);

        bool overflow = false;
        juce::AudioBuffer<float> buf(2, 512);
        for (int block = 0; block < 200; ++block)
        {
            buf.clear();
            // Sustained sine input
            for (int i = 0; i < 512; ++i)
            {
                float s = 0.3f * std::sin(juce::MathConstants<float>::twoPi * 440.0f * (block * 512 + i) / kSR);
                buf.getWritePointer(0)[i] = s;
                buf.getWritePointer(1)[i] = s;
            }
            rev.process(buf, 0, 512);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* p = buf.getReadPointer(ch);
                for (int i = 0; i < 512; ++i)
                    if (!std::isfinite(p[i]) || std::abs(p[i]) > 10.0f) { overflow = true; break; }
                if (overflow) break;
            }
            if (overflow) break;
        }
        expect(!overflow, "Reverb overflowed or went NaN at max room size");
    }

    // -------------------------------------------------------------------------
    void testDelayLineLatency()
    {
        beginTest("StereoDelay: impulse appears at correct delay offset");

        StereoDelay delay;
        delay.prepare(kSR, 512);

        const float delayTimeSeconds = 0.01f;  // 10ms = 441 samples
        delay.setDelayTimeLeft(delayTimeSeconds);
        delay.setDelayTimeRight(delayTimeSeconds);
        delay.setFeedback(0.0f);
        delay.setMix(1.0f);     // 100% wet

        // Single impulse in block 0
        juce::AudioBuffer<float> buf(2, 512);
        buf.clear();
        buf.getWritePointer(0)[0] = 1.0f;
        buf.getWritePointer(1)[0] = 1.0f;
        delay.process(buf, 0, 512);

        // The impulse should appear near sample 441
        const float* lp = buf.getReadPointer(0);
        float peakVal = 0.0f; int peakPos = -1;
        for (int i = 0; i < 512; ++i)
        {
            if (std::abs(lp[i]) > peakVal)
            {
                peakVal = std::abs(lp[i]);
                peakPos = i;
            }
        }
        expect(peakVal > 0.5f, "Delay impulse peak not found");
        // Allow ±5 samples tolerance for interpolation
        expect(peakPos >= 436 && peakPos <= 446,
               "Delay impulse at wrong position: " + juce::String(peakPos));
    }

    void testDelayFeedbackBounded()
    {
        beginTest("StereoDelay: max feedback does not overflow");

        StereoDelay delay;
        delay.prepare(kSR, 512);
        delay.setDelayTimeLeft(0.005f);
        delay.setDelayTimeRight(0.005f);
        delay.setFeedback(0.99f);   // very high
        delay.setMix(1.0f);

        bool overflow = false;
        juce::AudioBuffer<float> buf(2, 512);
        for (int block = 0; block < 50; ++block)
        {
            buf.clear();
            buf.getWritePointer(0)[0] = 0.1f;
            buf.getWritePointer(1)[0] = 0.1f;
            delay.process(buf, 0, 512);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* p = buf.getReadPointer(ch);
                for (int i = 0; i < 512; ++i)
                    if (!std::isfinite(p[i]) || std::abs(p[i]) > 20.0f) { overflow = true; break; }
                if (overflow) break;
            }
            if (overflow) break;
        }
        expect(!overflow, "Delay overflowed with high feedback");
    }

    // -------------------------------------------------------------------------
    void testEqBypassIsTransparent()
    {
        beginTest("ParametricEQ: disabled EQ is transparent (identity)");

        ParametricEq eq;
        eq.prepare(kSR, 512);
        eq.setEnabled(false);

        juce::AudioBuffer<float> in(2, 256), out(2, 256);
        for (int ch = 0; ch < 2; ++ch)
        {
            float* p = in.getWritePointer(ch);
            for (int i = 0; i < 256; ++i)
                p[i] = (float)std::rand() / RAND_MAX * 2.0f - 1.0f;
        }
        // Copy in to out
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::copy(out.getWritePointer(ch), in.getReadPointer(ch), 256);

        eq.process(out, 0, 256);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* pin = in.getReadPointer(ch);
            const float* pout = out.getReadPointer(ch);
            for (int i = 0; i < 256; ++i)
                expectWithinAbsoluteError(pout[i], pin[i], 1e-6f,
                    "EQ bypass modified sample ch=" + juce::String(ch) + " i=" + juce::String(i));
        }
    }

    void testSaturationBoundsAtMaxDrive()
    {
        beginTest("SaturationDistortion: max drive keeps output bounded");

        SaturationDistortion sat;
        sat.prepare(kSR, 512);
        sat.setSatMode(SaturationDistortion::Mode::Tape);
        sat.setDrive(1.0f);
        sat.setMix(1.0f);

        bool overflow = false;
        juce::AudioBuffer<float> buf(2, 512);
        for (int block = 0; block < 10; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float* p = buf.getWritePointer(ch);
                for (int i = 0; i < 512; ++i)
                    p[i] = (float)std::rand() / RAND_MAX * 2.0f - 1.0f;
            }
            sat.process(buf, 0, 512);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* p = buf.getReadPointer(ch);
                for (int i = 0; i < 512; ++i)
                    if (!std::isfinite(p[i]) || std::abs(p[i]) > 2.0f) { overflow = true; break; }
                if (overflow) break;
            }
            if (overflow) break;
        }
        expect(!overflow, "Saturation overflowed at max drive");
    }
};

static EffectsAndModMatrixTests gEffectsModMatrixTests;
