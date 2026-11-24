#include "UI/Views/EffectsView.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/ParameterIDs.h"
#include <cmath>

namespace cray
{
    EffectsView::EffectsView(CrayLdrAudioProcessor& processor)
        : processor_(processor)
    {
        auto& apvts = processor_.getApvts();
        auto bindKnob = [&](RotaryKnob& k, const juce::String& id)
        {
            k.attachToParameter(apvts, id);
            addAndMakeVisible(k);
        };

        // EQ
        addAndMakeVisible(eqEnableBtn_);
        eqEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::eqEnabled, eqEnableBtn_);
        bindKnob(eqLowGain_,  ParamIDs::eqLowGainDb);
        bindKnob(eqLowFreq_,  ParamIDs::eqLowFreq);
        bindKnob(eqMid1Gain_, ParamIDs::eqMid1GainDb);
        bindKnob(eqMid1Freq_, ParamIDs::eqMid1Freq);
        bindKnob(eqMid1Q_,    ParamIDs::eqMid1Q);
        bindKnob(eqMid2Gain_, ParamIDs::eqMid2GainDb);
        bindKnob(eqMid2Freq_, ParamIDs::eqMid2Freq);
        bindKnob(eqMid2Q_,    ParamIDs::eqMid2Q);
        bindKnob(eqHighGain_, ParamIDs::eqHighGainDb);
        bindKnob(eqHighFreq_, ParamIDs::eqHighFreq);

        // Saturation
        addAndMakeVisible(satEnableBtn_);
        satEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::satEnabled, satEnableBtn_);
        satModeBox_.addItemList({ "Tape", "Tube", "Warm" }, 1);
        addAndMakeVisible(satModeBox_);
        satModeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamIDs::satMode, satModeBox_);
        bindKnob(satDrive_, ParamIDs::satDrive);
        bindKnob(satTone_,  ParamIDs::satTone);
        bindKnob(satMix_,   ParamIDs::satMix);

        // Distortion
        addAndMakeVisible(distEnableBtn_);
        distEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::distEnabled, distEnableBtn_);
        distModeBox_.addItemList({ "Hard Clip", "Foldback", "Crunch" }, 1);
        addAndMakeVisible(distModeBox_);
        distModeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamIDs::distMode, distModeBox_);
        bindKnob(distDrive_, ParamIDs::distDrive);
        bindKnob(distTone_,  ParamIDs::distTone);
        bindKnob(distMix_,   ParamIDs::distMix);

        // Chorus
        addAndMakeVisible(chorusEnableBtn_);
        chorusEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::chorusEnabled, chorusEnableBtn_);
        bindKnob(chRate_,  ParamIDs::chorusRate);
        bindKnob(chDepth_, ParamIDs::chorusDepth);
        bindKnob(chFb_,    ParamIDs::chorusFeedback);
        bindKnob(chMix_,   ParamIDs::chorusMix);

        // Delay
        addAndMakeVisible(delayEnableBtn_);
        addAndMakeVisible(delaySyncBtn_);
        addAndMakeVisible(delayPingPongBtn_);
        delayEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::delayEnabled, delayEnableBtn_);
        delaySyncAttach_   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::delaySync, delaySyncBtn_);
        delayPpAttach_     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::delayPingPong, delayPingPongBtn_);
        delayDivBox_.addItemList({ "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/32" }, 1);
        addAndMakeVisible(delayDivBox_);
        delayDivAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, ParamIDs::delaySyncDiv, delayDivBox_);
        bindKnob(delTime_, ParamIDs::delayTimeMs);
        bindKnob(delFb_,   ParamIDs::delayFeedback);
        bindKnob(delCut_,  ParamIDs::delayHighCut);
        bindKnob(delMix_,  ParamIDs::delayMix);

        // Reverb
        addAndMakeVisible(reverbEnableBtn_);
        reverbEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, ParamIDs::reverbEnabled, reverbEnableBtn_);
        bindKnob(revPd_,    ParamIDs::reverbPreDelayMs);
        bindKnob(revSize_,  ParamIDs::reverbSize);
        bindKnob(revDamp_,  ParamIDs::reverbDamping);
        bindKnob(revWidth_, ParamIDs::reverbWidth);
        bindKnob(revMix_,   ParamIDs::reverbMix);

        startTimerHz(15);
    }

    EffectsView::~EffectsView()
    {
        stopTimer();
    }

    void EffectsView::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);
        const auto area = getLocalBounds().reduced(10);
        const int topH  = static_cast<int>(area.getHeight() * 0.36f);

        // Top EQ Card
        const auto eqCard = juce::Rectangle<float>(static_cast<float>(area.getX()), static_cast<float>(area.getY()),
                                                   static_cast<float>(area.getWidth()), static_cast<float>(topH));
        g.setColour(Palette::bgPanel);
        g.fillRoundedRectangle(eqCard, 8.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(eqCard, 8.0f, 1.0f);

        // Live EQ Frequency Response Canvas on left side of EQ card
        const auto eqGraph = juce::Rectangle<float>(eqCard.getX() + 10.0f, eqCard.getY() + 34.0f,
                                                    eqCard.getWidth() * 0.35f, eqCard.getHeight() - 44.0f);
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(eqGraph, 5.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(eqGraph, 5.0f, 1.0f);

        const float midY = eqGraph.getCentreY();
        g.setColour(Palette::borderSubtle.withAlpha(0.55f));
        g.drawHorizontalLine(static_cast<int>(midY), eqGraph.getX() + 2.0f, eqGraph.getRight() - 2.0f);

        const auto& eq = processor_.getAudioEngine().getEffectsChain().getEq();
        juce::Path curve;
        const int steps = 96;
        for (int i = 0; i <= steps; ++i)
        {
            const float norm = static_cast<float>(i) / static_cast<float>(steps);
            const float freq = 20.0f * std::pow(1000.0f, norm); // 20 Hz .. 20 kHz
            const float db   = juce::jlimit(-18.0f, 18.0f, eq.getMagnitudeDbAt(freq));
            const float px   = eqGraph.getX() + norm * eqGraph.getWidth();
            const float py   = midY - (db / 18.0f) * (eqGraph.getHeight() * 0.44f);
            if (i == 0) curve.startNewSubPath(px, py);
            else        curve.lineTo(px, py);
        }
        g.setColour(Palette::goldBright);
        g.strokePath(curve, juce::PathStrokeType(2.0f));

        // Bottom 5 FX Cards
        const int botY = area.getY() + topH + 8;
        const int botH = area.getBottom() - botY;
        const int cardW = (area.getWidth() - 24) / 5;
        for (int i = 0; i < 5; ++i)
        {
            const auto r = juce::Rectangle<float>(static_cast<float>(area.getX() + i * (cardW + 6)),
                                                  static_cast<float>(botY),
                                                  static_cast<float>(cardW),
                                                  static_cast<float>(botH));
            g.setColour(Palette::bgPanel);
            g.fillRoundedRectangle(r, 7.0f);
            g.setColour(Palette::borderSubtle);
            g.drawRoundedRectangle(r, 7.0f, 1.0f);
        }
    }

    void EffectsView::resized()
    {
        auto area = getLocalBounds().reduced(10);
        const int topH = static_cast<int>(area.getHeight() * 0.36f);
        auto eqArea = area.removeFromTop(topH).reduced(10);

        auto eqHdr = eqArea.removeFromTop(24);
        eqEnableBtn_.setBounds(eqHdr.removeFromLeft(120));

        // Leave left 36% for live EQ curve graph
        eqArea.removeFromLeft(static_cast<int>(area.getWidth() * 0.36f));
        const int eqKw = eqArea.getWidth() / 10;
        eqLowGain_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqLowFreq_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqMid1Gain_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqMid1Freq_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqMid1Q_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqMid2Gain_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqMid2Freq_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqMid2Q_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqHighGain_.setBounds(eqArea.removeFromLeft(eqKw).reduced(1));
        eqHighFreq_.setBounds(eqArea.reduced(1));

        area.removeFromTop(8);
        const int cardW = (area.getWidth() - 24) / 5;

        auto layoutCard2x2 = [](juce::Rectangle<int> r, RotaryKnob& k1, RotaryKnob& k2, RotaryKnob& k3, RotaryKnob* k4)
        {
            const int rh = r.getHeight() / 2;
            auto r1 = r.removeFromTop(rh);
            k1.setBounds(r1.removeFromLeft(r1.getWidth() / 2).reduced(2));
            k2.setBounds(r1.reduced(2));
            if (k4 != nullptr)
            {
                k3.setBounds(r.removeFromLeft(r.getWidth() / 2).reduced(2));
                k4->setBounds(r.reduced(2));
            }
            else
            {
                k3.setBounds(r.reduced(2));
            }
        };

        // 1: Saturation
        {
            auto c = area.removeFromLeft(cardW).reduced(8);
            area.removeFromLeft(6);
            satEnableBtn_.setBounds(c.removeFromTop(22));
            satModeBox_.setBounds(c.removeFromTop(22).reduced(0, 1));
            c.removeFromTop(4);
            layoutCard2x2(c, satDrive_, satTone_, satMix_, nullptr);
        }

        // 2: Distortion
        {
            auto c = area.removeFromLeft(cardW).reduced(8);
            area.removeFromLeft(6);
            distEnableBtn_.setBounds(c.removeFromTop(22));
            distModeBox_.setBounds(c.removeFromTop(22).reduced(0, 1));
            c.removeFromTop(4);
            layoutCard2x2(c, distDrive_, distTone_, distMix_, nullptr);
        }

        // 3: Chorus
        {
            auto c = area.removeFromLeft(cardW).reduced(8);
            area.removeFromTop(0);
            area.removeFromLeft(6);
            chorusEnableBtn_.setBounds(c.removeFromTop(22));
            c.removeFromTop(26);
            layoutCard2x2(c, chRate_, chDepth_, chFb_, &chMix_);
        }

        // 4: Delay
        {
            auto c = area.removeFromLeft(cardW).reduced(8);
            area.removeFromLeft(6);
            auto r0 = c.removeFromTop(22);
            delayEnableBtn_.setBounds(r0.removeFromLeft(78));
            delaySyncBtn_.setBounds(r0);
            auto r1 = c.removeFromTop(22);
            delayDivBox_.setBounds(r1.removeFromLeft(72));
            delayPingPongBtn_.setBounds(r1);
            c.removeFromTop(4);
            layoutCard2x2(c, delTime_, delFb_, delCut_, &delMix_);
        }

        // 5: Reverb
        {
            auto c = area.reduced(8);
            reverbEnableBtn_.setBounds(c.removeFromTop(22));
            c.removeFromTop(4);
            const int rh = c.getHeight() / 2;
            auto row1 = c.removeFromTop(rh);
            revPd_.setBounds(row1.removeFromLeft(row1.getWidth() / 3).reduced(1));
            revSize_.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(1));
            revDamp_.setBounds(row1.reduced(1));
            revWidth_.setBounds(c.removeFromLeft(c.getWidth() / 2).reduced(2));
            revMix_.setBounds(c.reduced(2));
        }
    }
}
