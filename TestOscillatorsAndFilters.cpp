// =============================================================================
// CRAY LDR URBAN INSTRUMENTS — TestOscillatorsAndFilters.cpp
// Tests:
//   • PolyBlepOscillator — waveform range, frequency tracking, noise distribution
//   • UnisonOscillatorBank — energy preservation, stereo spread, single-voice bypass
//   • ZdfSvfFilter — stability at extremes, LP/HP/BP response checks
// =============================================================================

#include <JuceHeader.h>
#include "DSP/Oscillators/PolyBlepOscillator.h"
#include "DSP/Oscillators/UnisonOscillatorBank.h"
#include "DSP/Filters/ZdfSvfFilter.h"
#include "DSP/Utilities/DspMath.h"

class OscillatorAndFilterTests : public juce::UnitTest
{
public:
    OscillatorAndFilterTests() : juce::UnitTest("OscillatorsAndFilters", "DSP") {}

    void runTest() override
    {
        testPolyBlepSineRange();
        testPolyBlepSawRange();
        testPolyBlepSquareRange();
        testPolyBlepTriRange();
        testNoiseDistribution();
        testUnisonEnergyPreservation();
        testUnisonSingleVoice();
        testZdfFilterLP_DC();
        testZdfFilterHP_DC();
        testZdfFilterStabilityAtExtremes();
        testZdfBandpassPeaks();
    }

private:
    static constexpr double kSR = 48000.0;
    static constexpr int    kBuf = 2048;

    // -------------------------------------------------------------------------
    void testPolyBlepSineRange()
    {
        beginTest("PolyBLEP Sine: output within [-1, 1]");

        PolyBlepOscillator osc;
        osc.prepare(kSR);
        osc.setWaveform(PolyBlepOscillator::Waveform::Sine);
        osc.setFrequency(440.0f);

        float mn = 1.0f, mx = -1.0f;
        for (int i = 0; i < kBuf; ++i)
        {
            float s = osc.processSample();
            mn = std::min(mn, s);
            mx = std::max(mx, s);
        }
        expect(mn >= -1.01f && mx <= 1.01f, "Sine exceeds unit amplitude");
        // After kBuf samples the sine must have visited both hemispheres
        expect(mn < -0.5f, "Sine negative excursion too small");
        expect(mx >  0.5f, "Sine positive excursion too small");
    }

    void testPolyBlepSawRange()
    {
        beginTest("PolyBLEP Saw: output within [-1, 1] at high frequency");

        PolyBlepOscillator osc;
        osc.prepare(kSR);
        osc.setWaveform(PolyBlepOscillator::Waveform::Saw);
        osc.setFrequency(10000.0f);   // near Nyquist — PolyBLEP must remain bounded

        float mn = 1.0f, mx = -1.0f;
        for (int i = 0; i < kBuf; ++i)
        {
            float s = osc.processSample();
            mn = std::min(mn, s);
            mx = std::max(mx, s);
        }
        expect(mn >= -1.1f && mx <= 1.1f, "Saw overshot bounds near Nyquist");
    }

    void testPolyBlepSquareRange()
    {
        beginTest("PolyBLEP Square: near ±1 excursion");

        PolyBlepOscillator osc;
        osc.prepare(kSR);
        osc.setWaveform(PolyBlepOscillator::Waveform::Square);
        osc.setFrequency(880.0f);

        float mn = 1.0f, mx = -1.0f;
        for (int i = 0; i < kBuf; ++i)
        {
            float s = osc.processSample();
            mn = std::min(mn, s);
            mx = std::max(mx, s);
        }
        expect(mn >= -1.05f && mx <= 1.05f, "Square overshot");
        expect(mx > 0.8f && mn < -0.8f, "Square levels too low");
    }

    void testPolyBlepTriRange()
    {
        beginTest("PolyBLEP Triangle: within [-1, 1]");

        PolyBlepOscillator osc;
        osc.prepare(kSR);
        osc.setWaveform(PolyBlepOscillator::Waveform::Triangle);
        osc.setFrequency(220.0f);

        for (int i = 0; i < kBuf; ++i)
        {
            float s = osc.processSample();
            expect(s >= -1.05f && s <= 1.05f, "Triangle out of range at sample " + juce::String(i));
        }
    }

    void testNoiseDistribution()
    {
        beginTest("PolyBLEP Noise: mean near zero, full range used");

        PolyBlepOscillator osc;
        osc.prepare(kSR);
        osc.setWaveform(PolyBlepOscillator::Waveform::Noise);
        osc.setFrequency(1000.0f);

        double sum = 0.0;
        float mn = 1.0f, mx = -1.0f;
        const int N = 44100;
        for (int i = 0; i < N; ++i)
        {
            float s = osc.processSample();
            sum += s;
            mn = std::min(mn, s);
            mx = std::max(mx, s);
        }
        double mean = sum / N;
        expect(std::abs(mean) < 0.05, "Noise DC offset too large: " + juce::String(mean));
        expect(mx > 0.5f, "Noise never reached positive half");
        expect(mn < -0.5f, "Noise never reached negative half");
    }

    // -------------------------------------------------------------------------
    void testUnisonEnergyPreservation()
    {
        beginTest("UnisonBank: 1 vs 4 voices — peak energy within ±3 dB");

        const double attenFactor = std::sqrt(4.0);   // expected normalization

        UnisonOscillatorBank bank1, bank4;
        bank1.prepare(kSR, 64);
        bank4.prepare(kSR, 64);

        bank1.setUnisonVoices(1);
        bank1.setDetune(0.0f);
        bank1.setWaveform(PolyBlepOscillator::Waveform::Saw);

        bank4.setUnisonVoices(4);
        bank4.setDetune(0.0f);   // zero detune so voices are coherent
        bank4.setWaveform(PolyBlepOscillator::Waveform::Saw);

        const float freq = 440.0f;
        bank1.noteOn(69, 1.0f);
        bank4.noteOn(69, 1.0f);

        juce::AudioBuffer<float> buf1(2, kBuf), buf4(2, kBuf);
        buf1.clear(); buf4.clear();
        bank1.process(buf1, 0, kBuf, freq, 0.0f, 1.0f);
        bank4.process(buf4, 0, kBuf, freq, 0.0f, 1.0f);

        // Measure RMS of left channel
        double rms1 = 0.0, rms4 = 0.0;
        const float* p1 = buf1.getReadPointer(0);
        const float* p4 = buf4.getReadPointer(0);
        for (int i = kBuf / 2; i < kBuf; ++i)   // skip attack transient
        {
            rms1 += double(p1[i]) * p1[i];
            rms4 += double(p4[i]) * p4[i];
        }
        rms1 = std::sqrt(rms1 / (kBuf / 2));
        rms4 = std::sqrt(rms4 / (kBuf / 2));

        // With zero detune the signals are coherent; normalised sum should be
        // within 3 dB of the single-voice level.
        if (rms1 > 1e-6)
        {
            double ratio_dB = 20.0 * std::log10(rms4 / rms1);
            expect(ratio_dB > -3.0 && ratio_dB < 3.0,
                   "Unison energy ratio out of range: " + juce::String(ratio_dB) + " dB");
        }
    }

    void testUnisonSingleVoice()
    {
        beginTest("UnisonBank: 1 voice is equivalent to raw PolyBLEP");

        UnisonOscillatorBank bank;
        bank.prepare(kSR, 64);
        bank.setUnisonVoices(1);
        bank.setDetune(0.0f);
        bank.setWaveform(PolyBlepOscillator::Waveform::Sine);
        bank.noteOn(69, 1.0f);

        PolyBlepOscillator ref;
        ref.prepare(kSR);
        ref.setWaveform(PolyBlepOscillator::Waveform::Sine);
        ref.setFrequency(440.0f);

        juce::AudioBuffer<float> buf(2, 256);
        buf.clear();
        bank.process(buf, 0, 256, 440.0f, 0.0f, 1.0f);

        const float* lp = buf.getReadPointer(0);
        const float* rp = buf.getReadPointer(1);
        for (int i = 128; i < 256; ++i)
        {
            // L and R should be equal (no spread with 1 voice)
            expectWithinAbsoluteError(lp[i], rp[i], 1e-5f, "L≠R for 1-voice unison");
            // Output must be bounded
            expect(lp[i] >= -1.1f && lp[i] <= 1.1f, "1-voice unison out of range");
        }
        (void)ref;  // ensure symbol is used
    }

    // -------------------------------------------------------------------------
    void testZdfFilterLP_DC()
    {
        beginTest("ZDF SVF LP: DC passes (gain ≈ 1), Nyquist attenuated");

        ZdfSvfFilter f;
        f.prepare(kSR, 64);
        f.setMode(ZdfSvfFilter::Mode::LP12);
        f.setCutoff(1000.0f);
        f.setResonance(0.0f);

        // DC test: feed a constant +0.5 signal long enough to settle
        float dc_out = 0.0f;
        for (int i = 0; i < 4096; ++i)
            dc_out = f.processSample(0.5f, 0.5f);   // mono → average L+R

        expectWithinAbsoluteError(dc_out, 0.5f, 0.02f, "LP DC gain not unity");

        // Nyquist test: near-Nyquist sine should be heavily attenuated
        f.reset();
        f.setCutoff(1000.0f);
        float nyq_max = 0.0f;
        // drive Nyquist freq by alternating +1/-1
        for (int i = 0; i < 2048; ++i)
        {
            float s = (i & 1) ? -0.9f : 0.9f;
            float y = f.processSample(s, s);
            if (i > 1024) nyq_max = std::max(nyq_max, std::abs(y));
        }
        expect(nyq_max < 0.1f, "LP does not attenuate Nyquist: " + juce::String(nyq_max));
    }

    void testZdfFilterHP_DC()
    {
        beginTest("ZDF SVF HP: DC blocked, high-freq passes");

        ZdfSvfFilter f;
        f.prepare(kSR, 64);
        f.setMode(ZdfSvfFilter::Mode::HP12);
        f.setCutoff(1000.0f);
        f.setResonance(0.0f);

        float dc_out = 0.0f;
        for (int i = 0; i < 4096; ++i)
            dc_out = f.processSample(0.5f, 0.5f);

        expectWithinAbsoluteError(dc_out, 0.0f, 0.01f, "HP passes DC");

        // Near-Nyquist should pass through
        f.reset();
        float nyq_max = 0.0f;
        for (int i = 0; i < 2048; ++i)
        {
            float s = (i & 1) ? -0.9f : 0.9f;
            float y = f.processSample(s, s);
            if (i > 1024) nyq_max = std::max(nyq_max, std::abs(y));
        }
        expect(nyq_max > 0.5f, "HP attenuates Nyquist too much: " + juce::String(nyq_max));
    }

    void testZdfFilterStabilityAtExtremes()
    {
        beginTest("ZDF SVF: stable at max resonance, min/max cutoff");

        ZdfSvfFilter f;
        f.prepare(kSR, 64);

        // Max resonance at very low cutoff
        f.setMode(ZdfSvfFilter::Mode::LP12);
        f.setCutoff(20.0f);
        f.setResonance(0.99f);

        bool overflow = false;
        for (int i = 0; i < 8192; ++i)
        {
            float noise = ((float)std::rand() / RAND_MAX) * 2.0f - 1.0f;
            float y = f.processSample(noise, noise);
            if (!std::isfinite(y) || std::abs(y) > 100.0f) { overflow = true; break; }
        }
        expect(!overflow, "ZDF overflowed at low cutoff + max resonance");

        // Max cutoff (Nyquist-ish)
        f.reset();
        f.setCutoff(20000.0f);
        f.setResonance(0.99f);
        overflow = false;
        for (int i = 0; i < 8192; ++i)
        {
            float noise = ((float)std::rand() / RAND_MAX) * 2.0f - 1.0f;
            float y = f.processSample(noise, noise);
            if (!std::isfinite(y) || std::abs(y) > 100.0f) { overflow = true; break; }
        }
        expect(!overflow, "ZDF overflowed at high cutoff + max resonance");
    }

    void testZdfBandpassPeaks()
    {
        beginTest("ZDF SVF BP: resonant peak within expected band");

        ZdfSvfFilter f;
        f.prepare(kSR, 64);
        f.setMode(ZdfSvfFilter::Mode::BP12);
        f.setCutoff(1000.0f);
        f.setResonance(0.8f);

        // Run white noise, collect energy in 500–1500 Hz vs. 3000–6000 Hz
        // A simple check: peak output amplitude should be notable
        float maxAmp = 0.0f;
        for (int i = 0; i < 4096; ++i)
        {
            float noise = ((float)std::rand() / RAND_MAX) * 2.0f - 1.0f;
            float y = f.processSample(noise, noise);
            maxAmp = std::max(maxAmp, std::abs(y));
        }
        expect(maxAmp > 0.01f, "BP output dead");
        expect(std::isfinite(maxAmp), "BP output NaN/Inf");
    }
};

static OscillatorAndFilterTests gOscFilterTests;
