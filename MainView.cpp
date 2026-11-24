#include "UI/Views/MainView.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/ParameterIDs.h"

namespace cray
{
    MainView::MainView(juce::AudioProcessorValueTreeState& apvts)
        : apvts_(apvts)
    {
        engineModeBox_.addItemList({ "Synth Only", "Sample Only", "Hybrid Engine" }, 1);
        addAndMakeVisible(engineModeBox_);
        engineModeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, ParamIDs::engineMode, engineModeBox_);

        auto initKnob = [&](RotaryKnob& k, const juce::String& id)
        {
            k.attachToParameter(apvts, id);
            addAndMakeVisible(k);
        };

        initKnob(synthMixKnob_,    ParamIDs::synthMix);
        initKnob(sampleMixKnob_,   ParamIDs::sampleMix);
        initKnob(sampleGainKnob_,  ParamIDs::sampleGainDb);
        initKnob(samplePitchKnob_, ParamIDs::samplePitch);
        initKnob(samplePanKnob_,   ParamIDs::samplePan);

        addAndMakeVisible(filterEnableBtn_);
        filterEnableAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, ParamIDs::filterEnabled, filterEnableBtn_);

        filterTypeBox_.addItemList({ "LP 12dB", "LP 24dB", "HP 12dB", "BP 12dB" }, 1);
        addAndMakeVisible(filterTypeBox_);
        filterTypeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, ParamIDs::filterType, filterTypeBox_);

        initKnob(cutoffKnob_,   ParamIDs::filterCutoff);
        initKnob(resoKnob_,     ParamIDs::filterResonance);
        initKnob(keyTrackKnob_, ParamIDs::filterKeyTrack);
        initKnob(envAmtKnob_,   ParamIDs::filterEnvAmount);
        initKnob(glideKnob_,    ParamIDs::glideMs);
        initKnob(polyKnob_,     ParamIDs::polyphony);
        initKnob(pbRangeKnob_,  ParamIDs::pitchBendRange);
        initKnob(velSensKnob_,  ParamIDs::velocitySensitivity);

        addAndMakeVisible(ampEnvDisplay_);
        initKnob(ampAKnob_, ParamIDs::ampAttack);
        initKnob(ampDKnob_, ParamIDs::ampDecay);
        initKnob(ampSKnob_, ParamIDs::ampSustain);
        initKnob(ampRKnob_, ParamIDs::ampRelease);

        addAndMakeVisible(filtEnvDisplay_);
        initKnob(filtAKnob_, ParamIDs::filterAttack);
        initKnob(filtDKnob_, ParamIDs::filterDecay);
        initKnob(filtSKnob_, ParamIDs::filterSustain);
        initKnob(filtRKnob_, ParamIDs::filterRelease);

        initKnob(lfo1RateKnob_,  ParamIDs::lfoRateHz(0));
        initKnob(lfo1PitchKnob_, ParamIDs::lfo1ToPitch);
        initKnob(lfo1CutKnob_,   ParamIDs::lfo1ToCutoff);
        initKnob(lfo1AmpKnob_,   ParamIDs::lfo1ToAmp);
        initKnob(satDriveKnob_,  ParamIDs::satDrive);
        initKnob(chorusMixKnob_, ParamIDs::chorusMix);
        initKnob(delayMixKnob_,  ParamIDs::delayMix);
        initKnob(reverbMixKnob_, ParamIDs::reverbMix);

        startTimerHz(20);
    }

    MainView::~MainView()
    {
        stopTimer();
    }

    void MainView::timerCallback()
    {
        auto readP = [&](const char* id, float def) -> float
        {
            if (auto* p = apvts_.getRawParameterValue(id))
                return p->load(std::memory_order_relaxed);
            return def;
        };

        ampEnvDisplay_.setAdsr(readP(ParamIDs::ampAttack, 0.01f),
                               readP(ParamIDs::ampDecay, 0.3f),
                               readP(ParamIDs::ampSustain, 0.8f),
                               readP(ParamIDs::ampRelease, 0.25f));

        filtEnvDisplay_.setAdsr(readP(ParamIDs::filterAttack, 0.01f),
                                readP(ParamIDs::filterDecay, 0.4f),
                                readP(ParamIDs::filterSustain, 0.35f),
                                readP(ParamIDs::filterRelease, 0.35f));
    }

    void MainView::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);

        const auto area = getLocalBounds().reduced(10);
        const int halfH = (area.getHeight() - 10) / 2;
        const int halfW = (area.getWidth() - 10) / 2;

        auto drawCard = [&](juce::Rectangle<int> r, const juce::String& title)
        {
            g.setColour(Palette::bgPanel);
            g.fillRoundedRectangle(r.toFloat(), 8.0f);
            g.setColour(Palette::borderSubtle);
            g.drawRoundedRectangle(r.toFloat(), 8.0f, 1.0f);

            g.setColour(Palette::goldPrimary);
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText(title, r.getX() + 12, r.getY() + 8, r.getWidth() - 24, 18, juce::Justification::centredLeft);
        };

        drawCard({ area.getX(), area.getY(), halfW, halfH }, "HYBRID ENGINE & LAYER MIX");
        drawCard({ area.getX() + halfW + 10, area.getY(), halfW, halfH }, "FILTER & PERFORMANCE");
        drawCard({ area.getX(), area.getY() + halfH + 10, halfW, halfH }, "AMP & FILTER ENVELOPES");
        drawCard({ area.getX() + halfW + 10, area.getY() + halfH + 10, halfW, halfH }, "MODULATION & FX MACROS");
    }

    void MainView::resized()
    {
        const auto area = getLocalBounds().reduced(10);
        const int halfH = (area.getHeight() - 10) / 2;
        const int halfW = (area.getWidth() - 10) / 2;

        // Card 1: Hybrid Mix
        {
            auto c1 = juce::Rectangle<int>(area.getX(), area.getY(), halfW, halfH).reduced(12, 32);
            auto topRow = c1.removeFromTop(26);
            engineModeBox_.setBounds(topRow.removeFromLeft(160));
            c1.removeFromTop(10);
            const int kw = c1.getWidth() / 5;
            synthMixKnob_.setBounds(c1.removeFromLeft(kw).reduced(2));
            sampleMixKnob_.setBounds(c1.removeFromLeft(kw).reduced(2));
            sampleGainKnob_.setBounds(c1.removeFromLeft(kw).reduced(2));
            samplePitchKnob_.setBounds(c1.removeFromLeft(kw).reduced(2));
            samplePanKnob_.setBounds(c1.reduced(2));
        }

        // Card 2: Filter & Perf
        {
            auto c2 = juce::Rectangle<int>(area.getX() + halfW + 10, area.getY(), halfW, halfH).reduced(12, 32);
            auto topRow = c2.removeFromTop(24);
            filterEnableBtn_.setBounds(topRow.removeFromLeft(84));
            filterTypeBox_.setBounds(topRow.removeFromLeft(110));
            c2.removeFromTop(6);

            const int rH = c2.getHeight() / 2;
            auto row1 = c2.removeFromTop(rH);
            const int kw = row1.getWidth() / 4;
            cutoffKnob_.setBounds(row1.removeFromLeft(kw).reduced(2));
            resoKnob_.setBounds(row1.removeFromLeft(kw).reduced(2));
            keyTrackKnob_.setBounds(row1.removeFromLeft(kw).reduced(2));
            envAmtKnob_.setBounds(row1.reduced(2));

            glideKnob_.setBounds(c2.removeFromLeft(kw).reduced(2));
            polyKnob_.setBounds(c2.removeFromLeft(kw).reduced(2));
            pbRangeKnob_.setBounds(c2.removeFromLeft(kw).reduced(2));
            velSensKnob_.setBounds(c2.reduced(2));
        }

        // Card 3: Envelopes
        {
            auto c3 = juce::Rectangle<int>(area.getX(), area.getY() + halfH + 10, halfW, halfH).reduced(12, 32);
            auto topAmp = c3.removeFromTop(c3.getHeight() / 2);
            ampEnvDisplay_.setBounds(topAmp.removeFromLeft(130).reduced(4));
            const int kw = topAmp.getWidth() / 4;
            ampAKnob_.setBounds(topAmp.removeFromLeft(kw).reduced(2));
            ampDKnob_.setBounds(topAmp.removeFromLeft(kw).reduced(2));
            ampSKnob_.setBounds(topAmp.removeFromLeft(kw).reduced(2));
            ampRKnob_.setBounds(topAmp.reduced(2));

            filtEnvDisplay_.setBounds(c3.removeFromLeft(130).reduced(4));
            filtAKnob_.setBounds(c3.removeFromLeft(kw).reduced(2));
            filtDKnob_.setBounds(c3.removeFromLeft(kw).reduced(2));
            filtSKnob_.setBounds(c3.removeFromLeft(kw).reduced(2));
            filtRKnob_.setBounds(c3.reduced(2));
        }

        // Card 4: Mod & FX Macros
        {
            auto c4 = juce::Rectangle<int>(area.getX() + halfW + 10, area.getY() + halfH + 10, halfW, halfH).reduced(12, 32);
            const int rH = c4.getHeight() / 2;
            auto row1 = c4.removeFromTop(rH);
            const int kw = row1.getWidth() / 4;
            lfo1RateKnob_.setBounds(row1.removeFromLeft(kw).reduced(2));
            lfo1PitchKnob_.setBounds(row1.removeFromLeft(kw).reduced(2));
            lfo1CutKnob_.setBounds(row1.removeFromLeft(kw).reduced(2));
            lfo1AmpKnob_.setBounds(row1.reduced(2));

            satDriveKnob_.setBounds(c4.removeFromLeft(kw).reduced(2));
            chorusMixKnob_.setBounds(c4.removeFromLeft(kw).reduced(2));
            delayMixKnob_.setBounds(c4.removeFromLeft(kw).reduced(2));
            reverbMixKnob_.setBounds(c4.reduced(2));
        }
    }
}
