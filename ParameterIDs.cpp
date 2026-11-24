#include "Common/ParameterIDs.h"
#include "Common/ProductInfo.h"

namespace cray
{
    namespace
    {
        juce::NormalisableRange<float> logFreqRange(float minHz, float maxHz)
        {
            juce::NormalisableRange<float> r(minHz, maxHz, 0.01f);
            r.setSkewForCentre(std::sqrt(minHz * maxHz));
            return r;
        }

        juce::NormalisableRange<float> envTimeRange(float minSec, float maxSec, float midSec)
        {
            juce::NormalisableRange<float> r(minSec, maxSec, 0.0001f);
            r.setSkewForCentre(midSec);
            return r;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout ParamIDs::createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        auto addBool = [&](const juce::String& id, const juce::String& name, bool def)
        {
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{ id, 1 }, name, def));
        };

        auto addFloat = [&](const juce::String& id, const juce::String& name,
                            juce::NormalisableRange<float> range, float def,
                            const juce::String& suffix = {})
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ id, 1 }, name, range, def,
                juce::AudioParameterFloatAttributes().withLabel(suffix)));
        };

        auto addInt = [&](const juce::String& id, const juce::String& name, int minV, int maxV, int def)
        {
            params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{ id, 1 }, name, minV, maxV, def));
        };

        auto addChoice = [&](const juce::String& id, const juce::String& name,
                             const juce::StringArray& choices, int def)
        {
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ id, 1 }, name, choices, def));
        };

        // Global / Master / Performance
        addFloat(masterGainDb, "Master Gain", juce::NormalisableRange<float>(-48.0f, 12.0f, 0.05f), -3.0f, "dB");
        addChoice(engineMode, "Engine Mode", { "Synth", "Sample", "Hybrid" }, 2);
        addFloat(synthMix, "Synth Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.85f);
        addFloat(sampleMix, "Sample Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.85f);
        addChoice(voiceMode, "Voice Mode", { "Poly", "Mono", "Legato" }, 0);
        addInt(polyphony, "Polyphony", 1, ProductInfo::kMaxPolyphony, 32);
        addFloat(glideMs, "Glide Time", envTimeRange(0.0f, 2000.0f, 150.0f), 0.0f, "ms");
        addBool(glideLegatoOnly, "Glide Legato Only", false);
        addInt(pitchBendRange, "Pitch Bend Range", 1, 24, 2);
        addFloat(velocitySensitivity, "Velocity Sensitivity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.75f);

        // 4 Oscillators
        const juce::StringArray oscWaves = { "Sine", "Saw", "Square", "Triangle", "Noise" };
        for (int i = 0; i < ProductInfo::kNumOscillators; ++i)
        {
            const juce::String prefix = "Osc " + juce::String(i + 1) + " ";
            addBool(oscEnabled(i), prefix + "Enabled", i == 0);
            addChoice(oscWave(i), prefix + "Wave", oscWaves, i == 0 ? 1 : (i == 1 ? 2 : 0));
            addFloat(oscLevel(i), prefix + "Level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), i == 0 ? 0.8f : 0.4f);
            addFloat(oscPan(i), prefix + "Pan", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f);
            addInt(oscPitch(i), prefix + "Pitch", -36, 36, (i == 2) ? -12 : 0);
            addFloat(oscFine(i), prefix + "Fine", juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, "ct");
            addInt(oscUnison(i), prefix + "Unison", 1, ProductInfo::kMaxUnisonVoices, 1);
            addFloat(oscDetune(i), prefix + "Detune", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.25f);
            addFloat(oscSpread(i), prefix + "Spread", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.50f);
        }

        // Amp ADSR
        addFloat(ampAttack,  "Amp Attack",  envTimeRange(0.001f, 8.0f, 0.15f), 0.006f, "s");
        addFloat(ampDecay,   "Amp Decay",   envTimeRange(0.002f, 10.0f, 0.35f), 0.30f, "s");
        addFloat(ampSustain, "Amp Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.80f);
        addFloat(ampRelease, "Amp Release", envTimeRange(0.002f, 12.0f, 0.40f), 0.25f, "s");

        // Filter & Filter ADSR
        addBool(filterEnabled, "Filter Enabled", true);
        addChoice(filterType, "Filter Type", { "LP 12dB", "LP 24dB", "HP 12dB", "BP 12dB" }, 1);
        addFloat(filterCutoff, "Filter Cutoff", logFreqRange(20.0f, 20000.0f), 14000.0f, "Hz");
        addFloat(filterResonance, "Filter Resonance", juce::NormalisableRange<float>(0.0f, 0.96f, 0.001f), 0.15f);
        addFloat(filterKeyTrack, "Filter Key Track", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f);
        addFloat(filterEnvAmount, "Filter Env Amt", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.35f);
        addFloat(filterAttack,  "Filter Attack",  envTimeRange(0.001f, 8.0f, 0.15f), 0.010f, "s");
        addFloat(filterDecay,   "Filter Decay",   envTimeRange(0.002f, 10.0f, 0.35f), 0.40f, "s");
        addFloat(filterSustain, "Filter Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f);
        addFloat(filterRelease, "Filter Release", envTimeRange(0.002f, 12.0f, 0.40f), 0.35f, "s");

        // LFOs
        const juce::StringArray lfoWaves = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" };
        const juce::StringArray syncDivs = { "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/32" };
        for (int i = 0; i < ProductInfo::kNumLfos; ++i)
        {
            const juce::String p = "LFO " + juce::String(i + 1) + " ";
            addChoice(lfoWave(i), p + "Wave", lfoWaves, 0);
            addFloat(lfoRateHz(i), p + "Rate", logFreqRange(0.05f, 30.0f), i == 0 ? 2.5f : 0.75f, "Hz");
            addBool(lfoSync(i), p + "Sync", false);
            addChoice(lfoSyncDiv(i), p + "Division", syncDivs, 4);
            addFloat(lfoDepth(i), p + "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f);
            addBool(lfoRetrigger(i), p + "Retrigger", false);
        }

        addFloat(lfo1ToPitch,  "LFO1 -> Pitch",  juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f);
        addFloat(lfo1ToCutoff, "LFO1 -> Cutoff", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f);
        addFloat(lfo1ToAmp,    "LFO1 -> Amp",    juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f);

        // Mod Matrix (8 slots)
        const juce::StringArray modSrcs = { "Off", "LFO 1", "LFO 2", "Amp Env", "Filter Env", "Mod Wheel", "Velocity", "Key Track", "Pitch Bend", "Aftertouch" };
        const juce::StringArray modDsts = {
            "Off", "Master Pitch", "Osc 1 Pitch", "Osc 2 Pitch", "Osc 3 Pitch", "Osc 4 Pitch",
            "Osc 1 Level", "Osc 2 Level", "Osc 3 Level", "Osc 4 Level",
            "Filter Cutoff", "Filter Reso", "Amp Level", "Stereo Pan",
            "LFO 1 Rate", "LFO 2 Rate", "Sat Drive", "Delay Mix", "Reverb Mix"
        };
        for (int s = 0; s < ProductInfo::kNumModSlots; ++s)
        {
            const juce::String p = "Mod " + juce::String(s + 1) + " ";
            // Default Slot 1: Mod Wheel -> Filter Cutoff subtly for expressive playability
            addChoice(modSource(s), p + "Source", modSrcs, s == 0 ? 5 : 0);
            addChoice(modDest(s),   p + "Dest",   modDsts, s == 0 ? 11 : 0);
            addFloat(modAmount(s),  p + "Amount", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), s == 0 ? 0.25f : 0.0f);
        }

        // Sample Engine global controls
        addFloat(sampleGainDb, "Sample Gain", juce::NormalisableRange<float>(-48.0f, 12.0f, 0.05f), 0.0f, "dB");
        addFloat(samplePan,    "Sample Pan",  juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f);
        addInt(samplePitch,    "Sample Pitch", -36, 36, 0);
        addFloat(sampleFine,   "Sample Fine",  juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, "ct");

        // EQ
        addBool(eqEnabled, "EQ Enabled", true);
        addFloat(eqLowGainDb,  "EQ Low Gain",  juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB");
        addFloat(eqLowFreq,    "EQ Low Freq",  logFreqRange(30.0f, 400.0f), 100.0f, "Hz");
        addFloat(eqMid1GainDb, "EQ Mid1 Gain", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB");
        addFloat(eqMid1Freq,   "EQ Mid1 Freq", logFreqRange(150.0f, 2500.0f), 500.0f, "Hz");
        addFloat(eqMid1Q,      "EQ Mid1 Q",    juce::NormalisableRange<float>(0.3f, 8.0f, 0.05f), 1.0f);
        addFloat(eqMid2GainDb, "EQ Mid2 Gain", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB");
        addFloat(eqMid2Freq,   "EQ Mid2 Freq", logFreqRange(800.0f, 8000.0f), 2500.0f, "Hz");
        addFloat(eqMid2Q,      "EQ Mid2 Q",    juce::NormalisableRange<float>(0.3f, 8.0f, 0.05f), 1.0f);
        addFloat(eqHighGainDb, "EQ High Gain", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f, "dB");
        addFloat(eqHighFreq,   "EQ High Freq", logFreqRange(3000.0f, 16000.0f), 8500.0f, "Hz");

        // Saturation
        addBool(satEnabled, "Saturation Enabled", false);
        addChoice(satMode, "Saturation Mode", { "Tape", "Tube", "Warm" }, 0);
        addFloat(satDrive, "Saturation Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.25f);
        addFloat(satTone,  "Saturation Tone",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.65f);
        addFloat(satMix,   "Saturation Mix",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.50f);

        // Distortion
        addBool(distEnabled, "Distortion Enabled", false);
        addChoice(distMode, "Distortion Mode", { "Hard Clip", "Foldback", "Crunch" }, 0);
        addFloat(distDrive, "Distortion Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.30f);
        addFloat(distTone,  "Distortion Tone",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.60f);
        addFloat(distMix,   "Distortion Mix",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.40f);

        // Chorus
        addBool(chorusEnabled, "Chorus Enabled", false);
        addFloat(chorusRate,     "Chorus Rate",     juce::NormalisableRange<float>(0.05f, 6.0f, 0.01f), 0.8f, "Hz");
        addFloat(chorusDepth,    "Chorus Depth",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.45f);
        addFloat(chorusFeedback, "Chorus Feedback", juce::NormalisableRange<float>(-0.7f, 0.7f, 0.01f), 0.20f);
        addFloat(chorusMix,      "Chorus Mix",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.35f);

        // Delay
        addBool(delayEnabled, "Delay Enabled", false);
        addBool(delaySync,    "Delay Sync", true);
        addFloat(delayTimeMs,  "Delay Time", juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f), 375.0f, "ms");
        addChoice(delaySyncDiv, "Delay Division", syncDivs, 8); // 1/8D
        addFloat(delayFeedback, "Delay Feedback", juce::NormalisableRange<float>(0.0f, 0.90f, 0.005f), 0.35f);
        addFloat(delayHighCut,  "Delay High Cut", logFreqRange(400.0f, 18000.0f), 6500.0f, "Hz");
        addBool(delayPingPong,  "Delay Ping Pong", true);
        addFloat(delayMix,      "Delay Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.25f);

        // Reverb
        addBool(reverbEnabled, "Reverb Enabled", false);
        addFloat(reverbPreDelayMs, "Reverb Pre-Delay", juce::NormalisableRange<float>(0.0f, 200.0f, 0.5f), 18.0f, "ms");
        addFloat(reverbSize,       "Reverb Size",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.65f);
        addFloat(reverbDamping,    "Reverb Damping",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.45f);
        addFloat(reverbWidth,      "Reverb Width",     juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.85f);
        addFloat(reverbMix,        "Reverb Mix",       juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.25f);

        return { params.begin(), params.end() };
    }
}
