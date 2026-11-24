#pragma once

#include "UI/Components/RotaryKnob.h"
#include "UI/Components/EnvelopeDisplay.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    class MainView : public juce::Component,
                     private juce::Timer
    {
    public:
        explicit MainView(juce::AudioProcessorValueTreeState& apvts);
        ~MainView() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void timerCallback() override;

        juce::AudioProcessorValueTreeState& apvts_;

        // Engine & Sample Mix
        juce::ComboBox engineModeBox_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineModeAttach_;
        RotaryKnob synthMixKnob_   { "SYNTH MIX" };
        RotaryKnob sampleMixKnob_  { "SAMPLE MIX" };
        RotaryKnob sampleGainKnob_ { "SMP GAIN" };
        RotaryKnob samplePitchKnob_{ "SMP PITCH" };
        RotaryKnob samplePanKnob_  { "SMP PAN", true };

        // Filter & Performance
        juce::ToggleButton filterEnableBtn_ { "FILTER" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterEnableAttach_;
        juce::ComboBox filterTypeBox_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttach_;

        RotaryKnob cutoffKnob_     { "CUTOFF" };
        RotaryKnob resoKnob_       { "RESO" };
        RotaryKnob keyTrackKnob_   { "KEY TRK" };
        RotaryKnob envAmtKnob_     { "ENV AMT", true };
        RotaryKnob glideKnob_      { "GLIDE" };
        RotaryKnob polyKnob_       { "POLY" };
        RotaryKnob pbRangeKnob_    { "PB RNG" };
        RotaryKnob velSensKnob_    { "VEL SENS" };

        // Envelopes
        EnvelopeDisplay ampEnvDisplay_;
        RotaryKnob ampAKnob_ { "AMP A" };
        RotaryKnob ampDKnob_ { "AMP D" };
        RotaryKnob ampSKnob_ { "AMP S" };
        RotaryKnob ampRKnob_ { "AMP R" };

        EnvelopeDisplay filtEnvDisplay_;
        RotaryKnob filtAKnob_ { "FLT A" };
        RotaryKnob filtDKnob_ { "FLT D" };
        RotaryKnob filtSKnob_ { "FLT S" };
        RotaryKnob filtRKnob_ { "FLT R" };

        // Quick Mod & FX Macros
        RotaryKnob lfo1RateKnob_   { "LFO1 Hz" };
        RotaryKnob lfo1PitchKnob_  { "VIBRATO", true };
        RotaryKnob lfo1CutKnob_    { "WOBBLE", true };
        RotaryKnob lfo1AmpKnob_    { "TREMOLO", true };
        RotaryKnob satDriveKnob_   { "SAT DRV" };
        RotaryKnob chorusMixKnob_  { "CHORUS" };
        RotaryKnob delayMixKnob_   { "DELAY" };
        RotaryKnob reverbMixKnob_  { "REVERB" };
    };
}
