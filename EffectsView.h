#pragma once

#include "Plugin/PluginProcessor.h"
#include "UI/Components/RotaryKnob.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    /**
     * Master Effects Rack View with live Parametric EQ curve display,
     * Saturation, Distortion, Stereo Chorus, Tempo-Synced Delay, and Algorithmic Reverb.
     */
    class EffectsView : public juce::Component,
                        private juce::Timer
    {
    public:
        explicit EffectsView(CrayLdrAudioProcessor& processor);
        ~EffectsView() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void timerCallback() override { repaint(); }

        CrayLdrAudioProcessor& processor_;

        // EQ
        juce::ToggleButton eqEnableBtn_ { "4-BAND EQ" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqEnableAttach_;
        RotaryKnob eqLowGain_  { "LOW dB", true },  eqLowFreq_  { "LOW Hz" };
        RotaryKnob eqMid1Gain_ { "M1 dB", true },   eqMid1Freq_ { "M1 Hz" }, eqMid1Q_ { "M1 Q" };
        RotaryKnob eqMid2Gain_ { "M2 dB", true },   eqMid2Freq_ { "M2 Hz" }, eqMid2Q_ { "M2 Q" };
        RotaryKnob eqHighGain_ { "HIGH dB", true }, eqHighFreq_ { "HIGH Hz" };

        // Saturation
        juce::ToggleButton satEnableBtn_ { "SATURATION" };
        juce::ComboBox     satModeBox_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   satEnableAttach_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satModeAttach_;
        RotaryKnob satDrive_ { "DRIVE" }, satTone_ { "TONE" }, satMix_ { "MIX" };

        // Distortion
        juce::ToggleButton distEnableBtn_ { "DISTORTION" };
        juce::ComboBox     distModeBox_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   distEnableAttach_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> distModeAttach_;
        RotaryKnob distDrive_ { "DRIVE" }, distTone_ { "TONE" }, distMix_ { "MIX" };

        // Chorus
        juce::ToggleButton chorusEnableBtn_ { "CHORUS" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chorusEnableAttach_;
        RotaryKnob chRate_ { "RATE" }, chDepth_ { "DEPTH" }, chFb_ { "FEEDBACK", true }, chMix_ { "MIX" };

        // Delay
        juce::ToggleButton delayEnableBtn_   { "DELAY" };
        juce::ToggleButton delaySyncBtn_     { "SYNC" };
        juce::ToggleButton delayPingPongBtn_ { "PINGPONG" };
        juce::ComboBox     delayDivBox_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   delayEnableAttach_, delaySyncAttach_, delayPpAttach_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> delayDivAttach_;
        RotaryKnob delTime_ { "TIME ms" }, delFb_ { "FEEDBACK" }, delCut_ { "HI CUT" }, delMix_ { "MIX" };

        // Reverb
        juce::ToggleButton reverbEnableBtn_ { "REVERB" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverbEnableAttach_;
        RotaryKnob revPd_ { "PRE-DLY" }, revSize_ { "SIZE" }, revDamp_ { "DAMP" }, revWidth_ { "WIDTH" }, revMix_ { "MIX" };
    };
}
