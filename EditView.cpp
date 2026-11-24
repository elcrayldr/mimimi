#include "UI/Views/EditView.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/ParameterIDs.h"

namespace cray
{
    EditView::EditView(juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < ProductInfo::kNumOscillators; ++i)
        {
            auto& s = oscStrips_[static_cast<size_t>(i)];
            s.enabledBtn.setButtonText("OSC " + juce::String(i + 1));
            addAndMakeVisible(s.enabledBtn);
            s.enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, ParamIDs::oscEnabled(i), s.enabledBtn);

            s.waveBox.addItemList({ "Sine", "Saw", "Square", "Triangle", "Noise" }, 1);
            addAndMakeVisible(s.waveBox);
            s.waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, ParamIDs::oscWave(i), s.waveBox);

            auto bind = [&](RotaryKnob& k, const juce::String& id)
            {
                k.attachToParameter(apvts, id);
                addAndMakeVisible(k);
            };
            bind(s.levelKnob,  ParamIDs::oscLevel(i));
            bind(s.panKnob,    ParamIDs::oscPan(i));
            bind(s.pitchKnob,  ParamIDs::oscPitch(i));
            bind(s.fineKnob,   ParamIDs::oscFine(i));
            bind(s.unisonKnob, ParamIDs::oscUnison(i));
            bind(s.detuneKnob, ParamIDs::oscDetune(i));
            bind(s.spreadKnob, ParamIDs::oscSpread(i));
        }

        for (int i = 0; i < ProductInfo::kNumLfos; ++i)
        {
            auto& l = lfoStrips_[static_cast<size_t>(i)];
            l.title.setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
            l.title.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            l.title.setColour(juce::Label::textColourId, Palette::goldPrimary);
            addAndMakeVisible(l.title);

            l.waveBox.addItemList({ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }, 1);
            addAndMakeVisible(l.waveBox);
            l.waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, ParamIDs::lfoWave(i), l.waveBox);

            addAndMakeVisible(l.syncBtn);
            l.syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, ParamIDs::lfoSync(i), l.syncBtn);

            addAndMakeVisible(l.retrigBtn);
            l.retrigAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, ParamIDs::lfoRetrigger(i), l.retrigBtn);

            l.divBox.addItemList({ "1/1", "1/2", "1/2D", "1/2T", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/32" }, 1);
            addAndMakeVisible(l.divBox);
            l.divAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, ParamIDs::lfoSyncDiv(i), l.divBox);

            l.rateKnob.attachToParameter(apvts, ParamIDs::lfoRateHz(i));
            addAndMakeVisible(l.rateKnob);
            l.depthKnob.attachToParameter(apvts, ParamIDs::lfoDepth(i));
            addAndMakeVisible(l.depthKnob);
        }

        const juce::StringArray modSrcs = { "Off", "LFO 1", "LFO 2", "Amp Env", "Filter Env", "Mod Wheel", "Velocity", "Key Track", "Pitch Bend", "Aftertouch" };
        const juce::StringArray modDsts = {
            "Off", "Master Pitch", "Osc 1 Pitch", "Osc 2 Pitch", "Osc 3 Pitch", "Osc 4 Pitch",
            "Osc 1 Level", "Osc 2 Level", "Osc 3 Level", "Osc 4 Level",
            "Filter Cutoff", "Filter Reso", "Amp Level", "Stereo Pan",
            "LFO 1 Rate", "LFO 2 Rate", "Sat Drive", "Delay Mix", "Reverb Mix"
        };

        for (int i = 0; i < ProductInfo::kNumModSlots; ++i)
        {
            auto& r = modRows_[static_cast<size_t>(i)];
            r.idxLabel.setText(juce::String(i + 1), juce::dontSendNotification);
            r.idxLabel.setColour(juce::Label::textColourId, Palette::goldPrimary);
            r.idxLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(r.idxLabel);

            r.srcBox.addItemList(modSrcs, 1);
            addAndMakeVisible(r.srcBox);
            r.srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, ParamIDs::modSource(i), r.srcBox);

            r.dstBox.addItemList(modDsts, 1);
            addAndMakeVisible(r.dstBox);
            r.dstAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, ParamIDs::modDest(i), r.dstBox);

            r.amtSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            r.amtSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
            addAndMakeVisible(r.amtSlider);
            r.amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, ParamIDs::modAmount(i), r.amtSlider);
        }
    }

    void EditView::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);
        const auto area = getLocalBounds().reduced(8);
        const int topH = static_cast<int>(area.getHeight() * 0.56f);
        const int oscW = (area.getWidth() - 18) / 4;

        for (int i = 0; i < 4; ++i)
        {
            const auto r = juce::Rectangle<float>(static_cast<float>(area.getX() + i * (oscW + 6)),
                                                  static_cast<float>(area.getY()),
                                                  static_cast<float>(oscW),
                                                  static_cast<float>(topH));
            g.setColour(Palette::bgPanel);
            g.fillRoundedRectangle(r, 7.0f);
            g.setColour(Palette::borderSubtle);
            g.drawRoundedRectangle(r, 7.0f, 1.0f);
        }

        const int botY = area.getY() + topH + 8;
        const int botH = area.getBottom() - botY;
        const int lfoW = static_cast<int>(area.getWidth() * 0.42f);

        const auto lfoRect = juce::Rectangle<float>(static_cast<float>(area.getX()), static_cast<float>(botY),
                                                    static_cast<float>(lfoW), static_cast<float>(botH));
        g.setColour(Palette::bgPanel);
        g.fillRoundedRectangle(lfoRect, 7.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(lfoRect, 7.0f, 1.0f);

        const auto modRect = juce::Rectangle<float>(lfoRect.getRight() + 8.0f, static_cast<float>(botY),
                                                    static_cast<float>(area.getRight()) - (lfoRect.getRight() + 8.0f),
                                                    static_cast<float>(botH));
        g.setColour(Palette::bgPanel);
        g.fillRoundedRectangle(modRect, 7.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(modRect, 7.0f, 1.0f);

        g.setColour(Palette::goldPrimary);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("MODULATION MATRIX (8 SLOTS)", modRect.toNearestInt().reduced(10, 6).removeFromTop(18),
                   juce::Justification::centredLeft);
    }

    void EditView::resized()
    {
        const auto area = getLocalBounds().reduced(8);
        const int topH = static_cast<int>(area.getHeight() * 0.56f);
        const int oscW = (area.getWidth() - 18) / 4;

        for (int i = 0; i < 4; ++i)
        {
            auto card = juce::Rectangle<int>(area.getX() + i * (oscW + 6), area.getY(), oscW, topH).reduced(8);
            auto hdr  = card.removeFromTop(24);
            oscStrips_[static_cast<size_t>(i)].enabledBtn.setBounds(hdr.removeFromLeft(78));
            oscStrips_[static_cast<size_t>(i)].waveBox.setBounds(hdr);

            card.removeFromTop(6);
            const int rH = card.getHeight() / 2;
            auto row1 = card.removeFromTop(rH);
            const int kw1 = row1.getWidth() / 4;
            oscStrips_[static_cast<size_t>(i)].levelKnob.setBounds(row1.removeFromLeft(kw1).reduced(1));
            oscStrips_[static_cast<size_t>(i)].panKnob.setBounds(row1.removeFromLeft(kw1).reduced(1));
            oscStrips_[static_cast<size_t>(i)].pitchKnob.setBounds(row1.removeFromLeft(kw1).reduced(1));
            oscStrips_[static_cast<size_t>(i)].fineKnob.setBounds(row1.reduced(1));

            const int kw2 = card.getWidth() / 3;
            oscStrips_[static_cast<size_t>(i)].unisonKnob.setBounds(card.removeFromLeft(kw2).reduced(2));
            oscStrips_[static_cast<size_t>(i)].detuneKnob.setBounds(card.removeFromLeft(kw2).reduced(2));
            oscStrips_[static_cast<size_t>(i)].spreadKnob.setBounds(card.reduced(2));
        }

        const int botY = area.getY() + topH + 8;
        const int botH = area.getBottom() - botY;
        const int lfoW = static_cast<int>(area.getWidth() * 0.42f);

        auto lfoArea = juce::Rectangle<int>(area.getX(), botY, lfoW, botH).reduced(8);
        const int eachLfoW = (lfoArea.getWidth() - 8) / 2;
        for (int i = 0; i < 2; ++i)
        {
            auto box = lfoArea.removeFromLeft(eachLfoW);
            lfoArea.removeFromLeft(8);
            auto& l = lfoStrips_[static_cast<size_t>(i)];

            auto top = box.removeFromTop(22);
            l.title.setBounds(top.removeFromLeft(50));
            l.waveBox.setBounds(top);

            box.removeFromTop(4);
            auto mid = box.removeFromTop(22);
            l.syncBtn.setBounds(mid.removeFromLeft(58));
            l.retrigBtn.setBounds(mid.removeFromRight(66));
            l.divBox.setBounds(mid);

            box.removeFromTop(4);
            l.rateKnob.setBounds(box.removeFromLeft(box.getWidth() / 2).reduced(2));
            l.depthKnob.setBounds(box.reduced(2));
        }

        auto modArea = juce::Rectangle<int>(area.getX() + lfoW + 8, botY, area.getWidth() - lfoW - 8, botH).reduced(10, 8);
        modArea.removeFromTop(20);
        const int rowH = std::max(18, modArea.getHeight() / ProductInfo::kNumModSlots);
        for (int i = 0; i < ProductInfo::kNumModSlots; ++i)
        {
            auto row = modArea.removeFromTop(rowH).reduced(0, 1);
            auto& mr = modRows_[static_cast<size_t>(i)];
            mr.idxLabel.setBounds(row.removeFromLeft(20));
            mr.srcBox.setBounds(row.removeFromLeft(95).reduced(2, 0));
            mr.dstBox.setBounds(row.removeFromLeft(110).reduced(2, 0));
            mr.amtSlider.setBounds(row);
        }
    }
}
