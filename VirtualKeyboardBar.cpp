#include "UI/Components/VirtualKeyboardBar.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/ParameterIDs.h"
#include <algorithm>

namespace cray
{
    namespace
    {
        bool isBlackMidi(int note)
        {
            const int n = note % 12;
            return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
        }
    }

    VirtualKeyboardBar::VirtualKeyboardBar(juce::AudioProcessorValueTreeState& apvts)
    {
        addAndMakeVisible(octDownBtn_);
        addAndMakeVisible(octUpBtn_);
        addAndMakeVisible(octLabel_);
        addAndMakeVisible(voiceModeBox_);
        addAndMakeVisible(voiceCounterLabel_);

        octDownBtn_.onClick = [this]()
        {
            baseOctave_ = std::max(0, baseOctave_ - 1);
            octLabel_.setText("OCT C" + juce::String(baseOctave_), juce::dontSendNotification);
            repaint();
        };
        octUpBtn_.onClick = [this]()
        {
            baseOctave_ = std::min(6, baseOctave_ + 1);
            octLabel_.setText("OCT C" + juce::String(baseOctave_), juce::dontSendNotification);
            repaint();
        };

        octLabel_.setText("OCT C" + juce::String(baseOctave_), juce::dontSendNotification);
        octLabel_.setJustificationType(juce::Justification::centred);
        octLabel_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        octLabel_.setColour(juce::Label::textColourId, Palette::goldPrimary);

        voiceModeBox_.addItemList({ "Poly", "Mono", "Legato" }, 1);
        voiceModeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, ParamIDs::voiceMode, voiceModeBox_);

        voiceCounterLabel_.setText("VOICES: 0", juce::dontSendNotification);
        voiceCounterLabel_.setJustificationType(juce::Justification::centred);
        voiceCounterLabel_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        voiceCounterLabel_.setColour(juce::Label::textColourId, Palette::textSecondary);
    }

    void VirtualKeyboardBar::setActiveVoiceCount(int totalVoices, int lastNote)
    {
        voiceCounterLabel_.setText("VOICES: " + juce::String(totalVoices), juce::dontSendNotification);
        if (lastExternalNote_ != lastNote)
        {
            lastExternalNote_ = lastNote;
            repaint();
        }
    }

    juce::Rectangle<float> VirtualKeyboardBar::getPianoArea() const
    {
        auto area = getLocalBounds().toFloat().reduced(6.0f, 4.0f);
        area.removeFromLeft(165.0f);
        area.removeFromRight(105.0f);
        return area;
    }

    void VirtualKeyboardBar::resized()
    {
        auto area = getLocalBounds().reduced(6, 6);
        auto leftCtrl = area.removeFromLeft(155);
        auto topRow = leftCtrl.removeFromTop(24);
        octDownBtn_.setBounds(topRow.removeFromLeft(28));
        octUpBtn_.setBounds(topRow.removeFromRight(28));
        octLabel_.setBounds(topRow);
        leftCtrl.removeFromTop(4);
        voiceModeBox_.setBounds(leftCtrl.removeFromTop(24));

        auto rightCtrl = area.removeFromRight(95);
        voiceCounterLabel_.setBounds(rightCtrl);
    }

    void VirtualKeyboardBar::paint(juce::Graphics& g)
    {
        g.setColour(Palette::bgGraphite);
        g.fillRect(getLocalBounds());
        g.setColour(Palette::borderSubtle);
        g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

        const auto piano = getPianoArea();
        const int startNote = (baseOctave_ + 1) * 12;
        const int totalKeys = numVisibleOctaves_ * 12;

        int whiteCount = 0;
        for (int i = 0; i < totalKeys; ++i)
            if (!isBlackMidi(startNote + i))
                ++whiteCount;

        const float whiteW = piano.getWidth() / static_cast<float>(std::max(1, whiteCount));

        // Pass 1: white keys
        int wIdx = 0;
        for (int i = 0; i < totalKeys; ++i)
        {
            const int note = startNote + i;
            if (isBlackMidi(note)) continue;

            const auto kr = juce::Rectangle<float>(piano.getX() + wIdx * whiteW, piano.getY(),
                                                   whiteW - 1.0f, piano.getHeight());
            const bool active = (note == pressedMidiNote_ || note == lastExternalNote_);
            g.setColour(active ? Palette::goldPrimary : juce::Colour(0xFFE7E9EF));
            g.fillRoundedRectangle(kr, 2.0f);
            g.setColour(Palette::bgObsidian);
            g.drawRoundedRectangle(kr, 2.0f, 0.8f);

            if (note % 12 == 0)
            {
                g.setColour(active ? Palette::bgObsidian : Palette::textMuted);
                g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                g.drawText("C" + juce::String((note / 12) - 1), kr.reduced(1.0f, 3.0f), juce::Justification::centredBottom);
            }
            ++wIdx;
        }

        // Pass 2: black keys
        wIdx = 0;
        const float blackW = whiteW * 0.62f;
        const float blackH = piano.getHeight() * 0.60f;
        for (int i = 0; i < totalKeys; ++i)
        {
            const int note = startNote + i;
            if (!isBlackMidi(note))
            {
                ++wIdx;
                continue;
            }

            const float bx = piano.getX() + static_cast<float>(wIdx) * whiteW - blackW * 0.5f;
            const auto kr = juce::Rectangle<float>(bx, piano.getY(), blackW, blackH);
            const bool active = (note == pressedMidiNote_ || note == lastExternalNote_);
            g.setColour(active ? Palette::goldBright : Palette::bgObsidian);
            g.fillRoundedRectangle(kr, 2.0f);
            g.setColour(Palette::borderBright);
            g.drawRoundedRectangle(kr, 2.0f, 0.7f);
        }
    }

    int VirtualKeyboardBar::hitTestPianoKey(juce::Point<float> pt, float& outVel) const
    {
        const auto piano = getPianoArea();
        if (!piano.contains(pt))
            return -1;

        outVel = juce::jlimit(0.15f, 1.0f, (pt.y - piano.getY()) / std::max(1.0f, piano.getHeight()));

        const int startNote = (baseOctave_ + 1) * 12;
        const int totalKeys = numVisibleOctaves_ * 12;

        int whiteCount = 0;
        for (int i = 0; i < totalKeys; ++i)
            if (!isBlackMidi(startNote + i))
                ++whiteCount;

        const float whiteW = piano.getWidth() / static_cast<float>(std::max(1, whiteCount));
        const float blackW = whiteW * 0.62f;
        const float blackH = piano.getHeight() * 0.60f;

        // Check black keys first
        int wIdx = 0;
        for (int i = 0; i < totalKeys; ++i)
        {
            const int note = startNote + i;
            if (!isBlackMidi(note))
            {
                ++wIdx;
                continue;
            }
            const float bx = piano.getX() + static_cast<float>(wIdx) * whiteW - blackW * 0.5f;
            if (juce::Rectangle<float>(bx, piano.getY(), blackW, blackH).contains(pt))
                return std::clamp(note, 0, 127);
        }

        // Check white keys
        wIdx = 0;
        for (int i = 0; i < totalKeys; ++i)
        {
            const int note = startNote + i;
            if (isBlackMidi(note)) continue;

            const auto kr = juce::Rectangle<float>(piano.getX() + wIdx * whiteW, piano.getY(), whiteW, piano.getHeight());
            if (kr.contains(pt))
                return std::clamp(note, 0, 127);
            ++wIdx;
        }
        return -1;
    }

    void VirtualKeyboardBar::mouseDown(const juce::MouseEvent& e)
    {
        float vel = 0.85f;
        const int note = hitTestPianoKey(e.position, vel);
        if (note >= 0)
        {
            pressedMidiNote_ = note;
            if (onMidiNote) onMidiNote(note, vel, true);
            repaint();
        }
    }

    void VirtualKeyboardBar::mouseDrag(const juce::MouseEvent& e)
    {
        float vel = 0.85f;
        const int note = hitTestPianoKey(e.position, vel);
        if (note != pressedMidiNote_)
        {
            if (pressedMidiNote_ >= 0 && onMidiNote)
                onMidiNote(pressedMidiNote_, 0.0f, false);
            pressedMidiNote_ = note;
            if (pressedMidiNote_ >= 0 && onMidiNote)
                onMidiNote(pressedMidiNote_, vel, true);
            repaint();
        }
    }

    void VirtualKeyboardBar::mouseUp(const juce::MouseEvent&)
    {
        if (pressedMidiNote_ >= 0)
        {
            if (onMidiNote) onMidiNote(pressedMidiNote_, 0.0f, false);
            pressedMidiNote_ = -1;
            repaint();
        }
    }
}
