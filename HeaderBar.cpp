#include "UI/Components/HeaderBar.h"
#include "UI/Theme/ColorPalette.h"
#include "Common/ParameterIDs.h"

namespace cray
{
    HeaderBar::HeaderBar(juce::AudioProcessorValueTreeState& apvts)
    {
        addAndMakeVisible(browserToggleBtn_);
        addAndMakeVisible(brandLink_);
        brandLink_.setTooltip("Abrir Instagram de CRAY LDR");
        brandLink_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        brandLink_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        brandLink_.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        brandLink_.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
        brandLink_.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
        brandLink_.onClick = [] { juce::URL("https://www.instagram.com/crayldr/").launchInDefaultBrowser(); };
        browserToggleBtn_.setClickingTogglesState(true);
        browserToggleBtn_.setToggleState(true, juce::dontSendNotification);
        browserToggleBtn_.onClick = [this]() { if (onToggleBrowser) onToggleBrowser(); };

        const char* tabNames[5] = { "MAIN", "EDIT", "SAMPLE EDITOR", "BUILDER", "EFFECTS" };
        for (int i = 0; i < 5; ++i)
        {
            auto& b = tabButtons_[static_cast<size_t>(i)];
            b.setButtonText(tabNames[i]);
            b.setClickingTogglesState(false);
            b.onClick = [this, i]()
            {
                const auto tab = static_cast<ActiveViewTab>(i);
                setActiveTab(tab);
                if (onTabSelected) onTabSelected(tab);
            };
            addAndMakeVisible(b);
        }
        setActiveTab(ActiveViewTab::Main);

        addAndMakeVisible(prevBtn_);
        addAndMakeVisible(nextBtn_);
        addAndMakeVisible(saveBtn_);
        addAndMakeVisible(initBtn_);
        addAndMakeVisible(exportBtn_);
        addAndMakeVisible(importBtn_);

        prevBtn_.onClick   = [this]() { if (onPrevPreset) onPrevPreset(); };
        nextBtn_.onClick   = [this]() { if (onNextPreset) onNextPreset(); };
        saveBtn_.onClick   = [this]() { if (onSavePreset) onSavePreset(); };
        initBtn_.onClick   = [this]() { if (onInitPreset) onInitPreset(); };
        exportBtn_.onClick = [this]() { if (onExportPack) onExportPack(); };
        importBtn_.onClick = [this]() { if (onImportPack) onImportPack(); };

        presetNameLabel_.setText("Init Urban Patch", juce::dontSendNotification);
        presetNameLabel_.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
        presetNameLabel_.setColour(juce::Label::textColourId, Palette::goldBright);
        presetNameLabel_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(presetNameLabel_);

        presetSubLabel_.setText("Factory  |  Plucks & Bells", juce::dontSendNotification);
        presetSubLabel_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::plain)));
        presetSubLabel_.setColour(juce::Label::textColourId, Palette::textSecondary);
        presetSubLabel_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(presetSubLabel_);

        statusLabel_.setText("Ready", juce::dontSendNotification);
        statusLabel_.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::plain)));
        statusLabel_.setColour(juce::Label::textColourId, Palette::textSecondary);
        statusLabel_.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(statusLabel_);

        masterGainSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        masterGainSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        masterGainSlider_.setTooltip("Master Output Gain (dB)");
        addAndMakeVisible(masterGainSlider_);
        masterGainAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ParamIDs::masterGainDb, masterGainSlider_);

        addAndMakeVisible(meter_);
    }

    void HeaderBar::setActiveTab(ActiveViewTab tab)
    {
        currentTab_ = tab;
        for (int i = 0; i < 5; ++i)
            tabButtons_[static_cast<size_t>(i)].setToggleState(i == static_cast<int>(tab), juce::dontSendNotification);
    }

    void HeaderBar::setPresetInfo(const juce::String& name, const juce::String& bankCategory, const juce::String& statusText, bool hasMissingSamples)
    {
        presetNameLabel_.setText(name, juce::dontSendNotification);
        presetSubLabel_.setText(bankCategory, juce::dontSendNotification);
        statusLabel_.setText(statusText, juce::dontSendNotification);
        hasMissingSamples_ = hasMissingSamples;
        statusLabel_.setColour(juce::Label::textColourId, hasMissingSamples ? Palette::ledRed : Palette::textSecondary);
        repaint();
    }

    void HeaderBar::setTelemetry(float peakL, float peakR, bool clipped, bool midiActive, int voiceCount)
    {
        meter_.setLevels(peakL, peakR, clipped);
        midiFlash_    = midiActive;
        activeVoices_ = voiceCount;
        repaint();
    }

    void HeaderBar::paint(juce::Graphics& g)
    {
        g.setColour(Palette::bgGraphite);
        g.fillRect(getLocalBounds());

        g.setColour(Palette::borderSubtle);
        g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

        // Brand emblem on left
        g.setColour(Palette::goldPrimary);
        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        g.drawText("CRAY LDR", 48, 8, 130, 20, juce::Justification::centredLeft);

        g.setColour(Palette::textSecondary);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("URBAN INSTRUMENTS", 48, 27, 145, 14, juce::Justification::centredLeft);

        // Preset LCD backdrop
        const int lcdX = 200;
        const bool compact = (getWidth() < 1280 || getHeight() >= 80);
        const int lcdW = compact
            ? juce::jmax(150, getWidth() - lcdX - 208 - 218 - 16)
            : juce::jmax(180, getWidth() - 780);
        const auto lcdRect = compact
            ? juce::Rectangle<float>(static_cast<float>(lcdX), 5.0f, static_cast<float>(lcdW), 36.0f)
            : juce::Rectangle<float>(static_cast<float>(lcdX), 6.0f, static_cast<float>(lcdW), 38.0f);
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(lcdRect, 5.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(lcdRect, 5.0f, 1.0f);

        // MIDI & Voice LEDs remain in the full-height desktop header.
        if (getHeight() >= 56 && getWidth() >= 1280)
        {
            const float ledX = static_cast<float>(getWidth() - 154);
            g.setColour(midiFlash_ ? Palette::ledGreen : Palette::bgObsidian);
            g.fillEllipse(ledX, 12.0f, 8.0f, 8.0f);
            g.setColour(Palette::textMuted);
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.drawText("MIDI", static_cast<int>(ledX + 11), 10, 30, 12, juce::Justification::left);

            g.setColour(activeVoices_ > 0 ? Palette::goldPrimary : Palette::bgObsidian);
            g.fillEllipse(ledX, 27.0f, 8.0f, 8.0f);
            g.setColour(Palette::textMuted);
            g.drawText("ACT", static_cast<int>(ledX + 11), 25, 30, 12, juce::Justification::left);
        }
    }

    void HeaderBar::resized()
    {
        const int w = getWidth();
        const bool compact = (w < 1280 || getHeight() >= 80);
        const int meterW = 104;
        const int knobW = 38;

        brandLink_.setBounds(46, 4, 148, 44);
        browserToggleBtn_.setBounds(8, compact ? 7 : 12, 34, 26);

        meter_.setBounds(w - meterW - 8, compact ? 7 : 10, meterW, 30);
        masterGainSlider_.setBounds(w - meterW - 8 - knobW - 44, compact ? 3 : 5, knobW, 40);

        if (compact)
        {
            // Two-row layout keeps the brand, preset controls and master section
            // separate from the five navigation tabs at the minimum window width.
            const int actionWidth = 218;
            const int rightSectionWidth = 208;
            const int actionX = juce::jmax(430, w - rightSectionWidth - actionWidth - 8);
            const int lcdX = 200;
            const int lcdRight = actionX - 8;
            const int lcdWidth = juce::jmax(150, lcdRight - lcdX);

            presetNameLabel_.setBounds(lcdX + 7, 5, lcdWidth, 18);
            presetSubLabel_.setBounds(lcdX + 7, 23, lcdWidth, 15);
            statusLabel_.setVisible(false);

            const int btnX = actionX;
            prevBtn_.setBounds(btnX, 7, 24, 28);
            nextBtn_.setBounds(btnX + 26, 7, 24, 28);
            saveBtn_.setBounds(btnX + 54, 7, 42, 28);
            initBtn_.setBounds(btnX + 98, 7, 38, 28);
            exportBtn_.setBounds(btnX + 138, 7, 42, 28);
            importBtn_.setBounds(btnX + 182, 7, 36, 28);

            const int tabsX = 8;
            const int tabsY = 51;
            const int tabsWidth = w - 16;
            const int tabWidth = tabsWidth / 5;
            for (int i = 0; i < 5; ++i)
                tabButtons_[static_cast<size_t>(i)].setBounds(tabsX + i * tabWidth + 2, tabsY,
                                                               tabWidth - 4, 29);
        }
        else
        {
            const int tabsW = 420;
            const int tabsX = w - meterW - knobW - 60 - tabsW;
            const int tabW = tabsW / 5;
            for (int i = 0; i < 5; ++i)
                tabButtons_[static_cast<size_t>(i)].setBounds(tabsX + i * tabW + 2, 10, tabW - 4, 30);

            const int lcdX = 200;
            const int lcdW = juce::jmax(180, tabsX - lcdX - 8);
            const int btnX = lcdX + lcdW - 224;
            presetNameLabel_.setBounds(lcdX + 8, 8, juce::jmax(80, btnX - lcdX - 10), 18);
            presetSubLabel_.setBounds(lcdX + 8, 25, juce::jmax(80, btnX - lcdX - 10), 15);
            statusLabel_.setVisible(true);
            statusLabel_.setBounds(tabsX, 41, tabsW, 13);

            prevBtn_.setBounds(btnX, 11, 24, 28);
            nextBtn_.setBounds(btnX + 26, 11, 24, 28);
            saveBtn_.setBounds(btnX + 54, 11, 42, 28);
            initBtn_.setBounds(btnX + 98, 11, 38, 28);
            exportBtn_.setBounds(btnX + 138, 11, 42, 28);
            importBtn_.setBounds(btnX + 182, 11, 36, 28);
        }
    }
}
