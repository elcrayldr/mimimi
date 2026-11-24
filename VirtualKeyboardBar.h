#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cray
{
    /**
     * Interactive bottom Virtual MIDI Keyboard bar with:
     * - Velocity-sensitive piano keys (vertical click position controls velocity)
     * - Octave -/+ shift
     * - Pitch Bend & Mod Wheel controls
     * - Play Mode & Polyphony quick indicators
     */
    class VirtualKeyboardBar : public juce::Component
    {
    public:
        explicit VirtualKeyboardBar(juce::AudioProcessorValueTreeState& apvts);

        std::function<void(int midiNote, float velocity01, bool isNoteOn)> onMidiNote;
        void setActiveVoiceCount(int totalVoices, int lastNote);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

    private:
        int hitTestPianoKey(juce::Point<float> pt, float& outVel) const;
        juce::Rectangle<float> getPianoArea() const;

        juce::TextButton octDownBtn_ { "-" };
        juce::TextButton octUpBtn_   { "+" };
        juce::Label      octLabel_;
        juce::ComboBox   voiceModeBox_;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceModeAttach_;

        juce::Slider     pitchWheelVisual_;
        juce::Slider     modWheelVisual_;
        juce::Label      voiceCounterLabel_;

        int baseOctave_       = 2; // C2 = MIDI 36, spans 4.5 octaves
        int numVisibleOctaves_ = 5;
        int pressedMidiNote_  = -1;
        int lastExternalNote_ = -1;
    };
}
