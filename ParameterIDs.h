#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace cray
{
    /**
     * Stable APVTS parameter IDs.
     * NEVER rename existing IDs after release; DAW automation and project recall depend on them.
     */
    namespace ParamIDs
    {
        // Global / Master / Performance
        inline constexpr const char* masterGainDb       = "master_gain_db";
        inline constexpr const char* engineMode         = "engine_mode"; // 0=Synth, 1=Sample, 2=Hybrid
        inline constexpr const char* synthMix           = "synth_mix";
        inline constexpr const char* sampleMix          = "sample_mix";
        inline constexpr const char* voiceMode          = "voice_mode"; // 0=Poly, 1=Mono, 2=Legato
        inline constexpr const char* polyphony          = "polyphony";  // 1..128
        inline constexpr const char* glideMs            = "glide_ms";
        inline constexpr const char* glideLegatoOnly    = "glide_legato_only";
        inline constexpr const char* pitchBendRange     = "pitch_bend_range";
        inline constexpr const char* velocitySensitivity = "vel_sensitivity";

        // Oscillator helper builders (index 0..3 -> osc1..osc4)
        inline juce::String oscEnabled(int i)    { return "osc" + juce::String(i + 1) + "_enabled"; }
        inline juce::String oscWave(int i)       { return "osc" + juce::String(i + 1) + "_wave"; }
        inline juce::String oscLevel(int i)      { return "osc" + juce::String(i + 1) + "_level"; }
        inline juce::String oscPan(int i)        { return "osc" + juce::String(i + 1) + "_pan"; }
        inline juce::String oscPitch(int i)      { return "osc" + juce::String(i + 1) + "_pitch"; }
        inline juce::String oscFine(int i)       { return "osc" + juce::String(i + 1) + "_fine"; }
        inline juce::String oscUnison(int i)     { return "osc" + juce::String(i + 1) + "_unison"; }
        inline juce::String oscDetune(int i)     { return "osc" + juce::String(i + 1) + "_detune"; }
        inline juce::String oscSpread(int i)     { return "osc" + juce::String(i + 1) + "_spread"; }

        // Synth Amp ADSR
        inline constexpr const char* ampAttack          = "amp_attack";
        inline constexpr const char* ampDecay           = "amp_decay";
        inline constexpr const char* ampSustain         = "amp_sustain";
        inline constexpr const char* ampRelease         = "amp_release";

        // Synth Filter + Filter ADSR
        inline constexpr const char* filterEnabled      = "filter_enabled";
        inline constexpr const char* filterType         = "filter_type"; // 0=LP12, 1=LP24, 2=HP12, 3=BP12
        inline constexpr const char* filterCutoff       = "filter_cutoff";
        inline constexpr const char* filterResonance    = "filter_resonance";
        inline constexpr const char* filterKeyTrack     = "filter_key_track";
        inline constexpr const char* filterEnvAmount    = "filter_env_amount";
        inline constexpr const char* filterAttack       = "filter_attack";
        inline constexpr const char* filterDecay        = "filter_decay";
        inline constexpr const char* filterSustain      = "filter_sustain";
        inline constexpr const char* filterRelease      = "filter_release";

        // LFOs (index 0..1 -> lfo1..lfo2)
        inline juce::String lfoWave(int i)       { return "lfo" + juce::String(i + 1) + "_wave"; }
        inline juce::String lfoRateHz(int i)     { return "lfo" + juce::String(i + 1) + "_rate_hz"; }
        inline juce::String lfoSync(int i)       { return "lfo" + juce::String(i + 1) + "_sync"; }
        inline juce::String lfoSyncDiv(int i)    { return "lfo" + juce::String(i + 1) + "_sync_div"; }
        inline juce::String lfoDepth(int i)      { return "lfo" + juce::String(i + 1) + "_depth"; }
        inline juce::String lfoRetrigger(int i)  { return "lfo" + juce::String(i + 1) + "_retrigger"; }

        // Direct LFO1 quick targets (for convenience in Main/Edit view)
        inline constexpr const char* lfo1ToPitch        = "lfo1_to_pitch";
        inline constexpr const char* lfo1ToCutoff       = "lfo1_to_cutoff";
        inline constexpr const char* lfo1ToAmp          = "lfo1_to_amp";

        // Modulation Matrix (slot 0..7)
        inline juce::String modSource(int slot)  { return "mod" + juce::String(slot + 1) + "_src"; }
        inline juce::String modDest(int slot)    { return "mod" + juce::String(slot + 1) + "_dst"; }
        inline juce::String modAmount(int slot)  { return "mod" + juce::String(slot + 1) + "_amt"; }

        // Sample Engine global overrides
        inline constexpr const char* sampleGainDb       = "sample_gain_db";
        inline constexpr const char* samplePan          = "sample_pan";
        inline constexpr const char* samplePitch        = "sample_pitch";
        inline constexpr const char* sampleFine         = "sample_fine";

        // Effects: EQ (4-band)
        inline constexpr const char* eqEnabled          = "eq_enabled";
        inline constexpr const char* eqLowGainDb        = "eq_low_gain";
        inline constexpr const char* eqLowFreq          = "eq_low_freq";
        inline constexpr const char* eqMid1GainDb       = "eq_mid1_gain";
        inline constexpr const char* eqMid1Freq         = "eq_mid1_freq";
        inline constexpr const char* eqMid1Q            = "eq_mid1_q";
        inline constexpr const char* eqMid2GainDb       = "eq_mid2_gain";
        inline constexpr const char* eqMid2Freq         = "eq_mid2_freq";
        inline constexpr const char* eqMid2Q            = "eq_mid2_q";
        inline constexpr const char* eqHighGainDb       = "eq_high_gain";
        inline constexpr const char* eqHighFreq         = "eq_high_freq";

        // Effects: Saturation
        inline constexpr const char* satEnabled         = "sat_enabled";
        inline constexpr const char* satMode            = "sat_mode"; // 0=Tape, 1=Tube, 2=Warm
        inline constexpr const char* satDrive           = "sat_drive";
        inline constexpr const char* satTone            = "sat_tone";
        inline constexpr const char* satMix             = "sat_mix";

        // Effects: Distortion
        inline constexpr const char* distEnabled        = "dist_enabled";
        inline constexpr const char* distMode           = "dist_mode"; // 0=HardClip, 1=Foldback, 2=Crunch
        inline constexpr const char* distDrive          = "dist_drive";
        inline constexpr const char* distTone           = "dist_tone";
        inline constexpr const char* distMix            = "dist_mix";

        // Effects: Chorus
        inline constexpr const char* chorusEnabled      = "chorus_enabled";
        inline constexpr const char* chorusRate         = "chorus_rate";
        inline constexpr const char* chorusDepth        = "chorus_depth";
        inline constexpr const char* chorusFeedback     = "chorus_feedback";
        inline constexpr const char* chorusMix          = "chorus_mix";

        // Effects: Delay
        inline constexpr const char* delayEnabled       = "delay_enabled";
        inline constexpr const char* delaySync          = "delay_sync";
        inline constexpr const char* delayTimeMs        = "delay_time_ms";
        inline constexpr const char* delaySyncDiv       = "delay_sync_div";
        inline constexpr const char* delayFeedback      = "delay_feedback";
        inline constexpr const char* delayHighCut       = "delay_high_cut";
        inline constexpr const char* delayPingPong      = "delay_ping_pong";
        inline constexpr const char* delayMix           = "delay_mix";

        // Effects: Reverb
        inline constexpr const char* reverbEnabled      = "reverb_enabled";
        inline constexpr const char* reverbPreDelayMs   = "reverb_predelay";
        inline constexpr const char* reverbSize         = "reverb_size";
        inline constexpr const char* reverbDamping      = "reverb_damping";
        inline constexpr const char* reverbWidth        = "reverb_width";
        inline constexpr const char* reverbMix          = "reverb_mix";

        juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    }
}
