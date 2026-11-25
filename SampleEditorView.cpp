#include "UI/Views/SampleEditorView.h"
#include "UI/Theme/ColorPalette.h"
#include <algorithm>

namespace cray
{
    SampleEditorView::SampleEditorView(CrayLdrAudioProcessor& processor)
        : processor_(processor)
    {
        addAndMakeVisible(zoneSelector_);
        zoneSelector_.onChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setSelectedZoneIndex(zoneSelector_.getSelectedItemIndex());
            syncFromModel();
        };

        auditionBtn_.onStateChange = [this]()
        {
            const auto* z = processor_.getSampleEditorModel().getSelectedZone();
            const int note = (z != nullptr) ? z->rootNote : 60;
            if (auditionBtn_.isDown())
                processor_.pushUiMidiNote(note, 0.9f, true);
            else
                processor_.pushUiMidiNote(note, 0.0f, false);
        };
        addAndMakeVisible(auditionBtn_);

        normalizeBtn_.onClick = [this]()
        {
            processor_.getSampleEditorModel().normalize(
                waveformView_.getSelectionStartSample(),
                waveformView_.getSelectionEndSample(),
                -0.3f);
            syncFromModel();
        };
        addAndMakeVisible(normalizeBtn_);

        configureSlider(rootNoteSlider_, 0, 127, 1, "");
        rootNoteSlider_.setTooltip("Root Note: the original pitch of this sample");
        rootNoteSlider_.textFromValueFunction = [](double value)
        {
            return InstrumentBuilder::midiNoteToName(static_cast<int>(value));
        };
        rootNoteSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setRootNote(static_cast<int>(rootNoteSlider_.getValue()));
            syncFromModel();
        };

        autoRootBtn_.setTooltip("Detect the sample's fundamental and set its Root Note when confidence is sufficient.");
        autoRootBtn_.onClick = [this]()
        {
            const auto* zone = processor_.getSampleEditorModel().getSelectedZone();
            if (zone == nullptr)
                return;
            const auto detected = processor_.getSampleDecoder().detectRootNote(juce::File(zone->sourceFilePath));
            if (detected.has_value())
            {
                processor_.getSampleEditorModel().setRootNote(*detected);
                syncFromModel();
                infoLabel_.setText("AUTO ROOT: " + InstrumentBuilder::midiNoteToName(*detected),
                                    juce::dontSendNotification);
            }
            else
            {
                infoLabel_.setText("AUTO ROOT: pitch unclear - set manually",
                                    juce::dontSendNotification);
            }
        };
        addAndMakeVisible(autoRootBtn_);

        relinkBtn_.onClick = [this]()
        {
            relinkChooser_ = std::make_unique<juce::FileChooser>("Select folder containing missing samples");
            relinkChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this](const juce::FileChooser& fc)
                {
                    const auto dir = fc.getResult();
                    if (dir.isDirectory())
                    {
                        processor_.relinkMissingSamplesFromFolder(dir);
                        syncFromModel();
                    }
                });
        };
        addAndMakeVisible(relinkBtn_);

        infoLabel_.setJustificationType(juce::Justification::centredRight);
        infoLabel_.setColour(juce::Label::textColourId, Palette::goldPrimary);
        infoLabel_.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        addAndMakeVisible(infoLabel_);

        waveformView_.onStartEndChanged = [this](int s, int e)
        {
            processor_.getSampleEditorModel().setStartEnd(s, e);
            syncFromModel();
        };
        waveformView_.onLoopRangeChanged = [this](int ls, int le)
        {
            processor_.getSampleEditorModel().setLoopRange(ls, le);
            syncFromModel();
        };
        addAndMakeVisible(waveformView_);

        configureSlider(zoomSlider_, 1.0, 64.0, 0.1, "x");
        zoomSlider_.onValueChange = [this]() { waveformView_.setZoom(zoomSlider_.getValue()); };
        configureSlider(scrollSlider_, 0.0, 1.0, 0.001, "");
        scrollSlider_.onValueChange = [this]() { waveformView_.setScrollOffset(scrollSlider_.getValue()); };

        addAndMakeVisible(reverseBtn_);
        reverseBtn_.onClick = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setReverse(reverseBtn_.getToggleState());
            syncFromModel();
        };

        addAndMakeVisible(loopBtn_);
        loopBtn_.onClick = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setLoopEnabled(loopBtn_.getToggleState());
            syncFromModel();
        };

        configureSlider(startSlider_, 0, 10000000, 1, " smp");
        startSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setStartEnd(static_cast<int>(startSlider_.getValue()),
                                                          static_cast<int>(endSlider_.getValue()));
            syncFromModel();
        };
        configureSlider(endSlider_, 1, 10000000, 1, " smp");
        endSlider_.onValueChange = startSlider_.onValueChange;

        configureSlider(loopStartSlider_, 0, 10000000, 1, " smp");
        loopStartSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setLoopRange(static_cast<int>(loopStartSlider_.getValue()),
                                                             static_cast<int>(loopEndSlider_.getValue()));
            syncFromModel();
        };
        configureSlider(loopEndSlider_, 2, 10000000, 1, " smp");
        loopEndSlider_.onValueChange = loopStartSlider_.onValueChange;

        configureSlider(gainSlider_, -36.0, 24.0, 0.1, " dB");
        gainSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setGainDb(static_cast<float>(gainSlider_.getValue()));
            waveformView_.setData(processor_.getSampleEditorModel().getSelectedSampleBuffer(),
                                  processor_.getSampleEditorModel().getSelectedZone());
        };

        configureSlider(pitchSlider_, -36, 36, 1, " st");
        pitchSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setPitchAndFine(static_cast<int>(pitchSlider_.getValue()),
                                                                static_cast<float>(fineSlider_.getValue()));
        };
        configureSlider(fineSlider_, -100.0, 100.0, 1.0, " ct");
        fineSlider_.onValueChange = pitchSlider_.onValueChange;

        configureSlider(panSlider_, -1.0, 1.0, 0.01, "");
        panSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setPan(static_cast<float>(panSlider_.getValue()));
        };

        configureSlider(fadeInMsSlider_, 0.0, 2000.0, 1.0, " ms");
        fadeInMsSlider_.onValueChange = [this]()
        {
            if (updatingUi_) return;
            auto buf = processor_.getSampleEditorModel().getSelectedSampleBuffer();
            const double sr = (buf != nullptr) ? buf->sampleRate : 44100.0;
            processor_.getSampleEditorModel().setFades(
                static_cast<int>(fadeInMsSlider_.getValue() * 0.001 * sr),
                static_cast<int>(fadeOutMsSlider_.getValue() * 0.001 * sr));
            waveformView_.setData(buf, processor_.getSampleEditorModel().getSelectedZone());
        };
        configureSlider(fadeOutMsSlider_, 0.0, 2000.0, 1.0, " ms");
        fadeOutMsSlider_.onValueChange = fadeInMsSlider_.onValueChange;

        configureSlider(attackSlider_,  0.001, 5.0, 0.001, " s");
        configureSlider(decaySlider_,   0.002, 5.0, 0.001, " s");
        configureSlider(sustainSlider_, 0.0,   1.0, 0.01,  "");
        configureSlider(releaseSlider_, 0.002, 8.0, 0.001, " s");

        auto adsrChanged = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setAdsr(
                static_cast<float>(attackSlider_.getValue()),
                static_cast<float>(decaySlider_.getValue()),
                static_cast<float>(sustainSlider_.getValue()),
                static_cast<float>(releaseSlider_.getValue()));
        };
        attackSlider_.onValueChange  = adsrChanged;
        decaySlider_.onValueChange   = adsrChanged;
        sustainSlider_.onValueChange = adsrChanged;
        releaseSlider_.onValueChange = adsrChanged;

        addAndMakeVisible(filterEnableBtn_);
        filterTypeBox_.addItemList({ "LP 12dB", "LP 24dB", "HP 12dB", "BP 12dB" }, 1);
        addAndMakeVisible(filterTypeBox_);
        configureSlider(cutoffSlider_, 20.0, 20000.0, 1.0, " Hz");
        cutoffSlider_.setSkewFactorFromMidPoint(1000.0);
        configureSlider(resSlider_, 0.0, 0.95, 0.01, "");

        auto filtChanged = [this]()
        {
            if (updatingUi_) return;
            processor_.getSampleEditorModel().setFilter(
                filterEnableBtn_.getToggleState(),
                static_cast<SvfFilterType>(std::max(0, filterTypeBox_.getSelectedItemIndex())),
                static_cast<float>(cutoffSlider_.getValue()),
                static_cast<float>(resSlider_.getValue()));
        };
        filterEnableBtn_.onClick = filtChanged;
        filterTypeBox_.onChange  = filtChanged;
        cutoffSlider_.onValueChange = filtChanged;
        resSlider_.onValueChange    = filtChanged;

        syncFromModel();
    }

    void SampleEditorView::configureSlider(juce::Slider& s, double minV, double maxV, double step, const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        s.setRange(minV, maxV, step);
        s.setTextValueSuffix(suffix);
        addAndMakeVisible(s);
    }

    void SampleEditorView::syncFromModel()
    {
        updatingUi_ = true;
        auto& model = processor_.getSampleEditorModel();
        const auto& inst = processor_.getActiveInstrument();

        zoneSelector_.clear(juce::dontSendNotification);
        for (int i = 0; i < static_cast<int>(inst.zones.size()); ++i)
        {
            const auto& z = inst.zones[static_cast<size_t>(i)];
            zoneSelector_.addItem("L" + juce::String(z.layerIndex + 1) + ": " + z.displayName, i + 1);
        }

        if (model.getSelectedZoneIndex() >= 0 && model.getSelectedZoneIndex() < zoneSelector_.getNumItems())
            zoneSelector_.setSelectedItemIndex(model.getSelectedZoneIndex(), juce::dontSendNotification);

        const auto* z  = model.getSelectedZone();
        const auto buf = model.getSelectedSampleBuffer();
        waveformView_.setData(buf, z);

        if (z != nullptr && buf != nullptr)
        {
            const int maxS = std::max(1, buf->numSamples - 1);
            startSlider_.setRange(0, maxS - 1, 1);
            endSlider_.setRange(1, maxS, 1);
            loopStartSlider_.setRange(0, maxS - 2, 1);
            loopEndSlider_.setRange(2, maxS, 1);

            startSlider_.setValue(z->startSample, juce::dontSendNotification);
            endSlider_.setValue((z->endSample > 0) ? z->endSample : maxS, juce::dontSendNotification);
            loopStartSlider_.setValue(z->loopStartSample, juce::dontSendNotification);
            loopEndSlider_.setValue((z->loopEndSample > 0) ? z->loopEndSample : maxS, juce::dontSendNotification);

            reverseBtn_.setToggleState(z->reverse, juce::dontSendNotification);
            loopBtn_.setToggleState(z->loopEnabled, juce::dontSendNotification);

            rootNoteSlider_.setValue(z->rootNote, juce::dontSendNotification);
            gainSlider_.setValue(z->gainDb, juce::dontSendNotification);
            pitchSlider_.setValue(z->pitchSemitones, juce::dontSendNotification);
            fineSlider_.setValue(z->fineTuneCents, juce::dontSendNotification);
            panSlider_.setValue(z->pan, juce::dontSendNotification);

            const double sr = std::max(1.0, buf->sampleRate);
            fadeInMsSlider_.setValue((z->fadeInSamples / sr) * 1000.0, juce::dontSendNotification);
            fadeOutMsSlider_.setValue((z->fadeOutSamples / sr) * 1000.0, juce::dontSendNotification);

            attackSlider_.setValue(z->attackSec, juce::dontSendNotification);
            decaySlider_.setValue(z->decaySec, juce::dontSendNotification);
            sustainSlider_.setValue(z->sustainLevel, juce::dontSendNotification);
            releaseSlider_.setValue(z->releaseSec, juce::dontSendNotification);

            filterEnableBtn_.setToggleState(z->filterEnabled, juce::dontSendNotification);
            filterTypeBox_.setSelectedItemIndex(static_cast<int>(z->filterType), juce::dontSendNotification);
            cutoffSlider_.setValue(z->filterCutoffHz, juce::dontSendNotification);
            resSlider_.setValue(z->filterResonance, juce::dontSendNotification);

            infoLabel_.setText("NON-DESTRUCTIVE  |  " + juce::String(buf->sampleRate, 0) + " Hz  |  "
                               + juce::String(buf->numChannels) + "ch  |  Norm: " + juce::String(z->normalizeGainDb, 1) + " dB",
                               juce::dontSendNotification);
        }
        else
        {
            infoLabel_.setText("No sample zone selected", juce::dontSendNotification);
        }

        relinkBtn_.setVisible(!processor_.getMissingSamples().isEmpty());
        updatingUi_ = false;
    }

    void SampleEditorView::paint(juce::Graphics& g)
    {
        g.fillAll(Palette::bgObsidian);

        const auto area = getLocalBounds().reduced(10);
        const int botY = area.getBottom() - 205;
        const auto ctrlRect = juce::Rectangle<float>(static_cast<float>(area.getX()), static_cast<float>(botY),
                                                     static_cast<float>(area.getWidth()), 205.0f);
        g.setColour(Palette::bgPanel);
        g.fillRoundedRectangle(ctrlRect, 8.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(ctrlRect, 8.0f, 1.0f);

        g.setColour(Palette::textSecondary);
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        const int colW = (area.getWidth() - 24) / 3;
        g.drawText("REGION / GAIN / PITCH", area.getX() + 12, botY + 6, colW, 16, juce::Justification::left);
        g.drawText("LOOP / FADES / PAN", area.getX() + 16 + colW, botY + 6, colW, 16, juce::Justification::left);
        g.drawText("ZONE ADSR & FILTER", area.getX() + 20 + colW * 2, botY + 6, colW, 16, juce::Justification::left);
    }

    void SampleEditorView::resized()
    {
        auto area = getLocalBounds().reduced(10);

        const bool compactToolbar = area.getWidth() < 840;
        auto topBar = area.removeFromTop(compactToolbar ? 58 : 28);
        if (compactToolbar)
        {
            auto sampleRow = topBar.removeFromTop(26);
            zoneSelector_.setBounds(sampleRow.removeFromLeft(190));
            sampleRow.removeFromLeft(5);
            auditionBtn_.setBounds(sampleRow.removeFromLeft(82));
            sampleRow.removeFromLeft(5);
            autoRootBtn_.setBounds(sampleRow.removeFromLeft(82));
            sampleRow.removeFromLeft(5);
            rootNoteSlider_.setBounds(sampleRow.removeFromLeft(145));

            topBar.removeFromTop(5);
            auto actionRow = topBar;
            normalizeBtn_.setBounds(actionRow.removeFromLeft(90));
            actionRow.removeFromLeft(5);
            relinkBtn_.setBounds(actionRow.removeFromLeft(112));
            actionRow.removeFromLeft(8);
            infoLabel_.setBounds(actionRow);
        }
        else
        {
            zoneSelector_.setBounds(topBar.removeFromLeft(190));
            topBar.removeFromLeft(5);
            auditionBtn_.setBounds(topBar.removeFromLeft(82));
            topBar.removeFromLeft(5);
            autoRootBtn_.setBounds(topBar.removeFromLeft(82));
            topBar.removeFromLeft(5);
            rootNoteSlider_.setBounds(topBar.removeFromLeft(145));
            topBar.removeFromLeft(5);
            normalizeBtn_.setBounds(topBar.removeFromLeft(90));
            topBar.removeFromLeft(5);
            relinkBtn_.setBounds(topBar.removeFromLeft(112));
            infoLabel_.setBounds(topBar);
        }

        area.removeFromTop(6);
        auto botControls = area.removeFromBottom(205);
        area.removeFromBottom(6);

        auto zoomBar = area.removeFromBottom(22);
        zoomSlider_.setBounds(zoomBar.removeFromLeft(220));
        zoomBar.removeFromLeft(10);
        scrollSlider_.setBounds(zoomBar);

        area.removeFromBottom(4);
        waveformView_.setBounds(area);

        // Controls grid
        auto inner = botControls.reduced(12, 24);
        const int colW = (inner.getWidth() - 20) / 3;

        auto c1 = inner.removeFromLeft(colW);
        inner.removeFromLeft(10);
        auto c2 = inner.removeFromLeft(colW);
        inner.removeFromLeft(10);
        auto c3 = inner;

        const int rh = 24;
        startSlider_.setBounds(c1.removeFromTop(rh));
        endSlider_.setBounds(c1.removeFromTop(rh));
        gainSlider_.setBounds(c1.removeFromTop(rh));
        pitchSlider_.setBounds(c1.removeFromTop(rh));
        fineSlider_.setBounds(c1.removeFromTop(rh));
        reverseBtn_.setBounds(c1.removeFromTop(rh));

        auto lRow = c2.removeFromTop(rh);
        loopBtn_.setBounds(lRow.removeFromLeft(80));
        loopStartSlider_.setBounds(c2.removeFromTop(rh));
        loopEndSlider_.setBounds(c2.removeFromTop(rh));
        fadeInMsSlider_.setBounds(c2.removeFromTop(rh));
        fadeOutMsSlider_.setBounds(c2.removeFromTop(rh));
        panSlider_.setBounds(c2.removeFromTop(rh));

        attackSlider_.setBounds(c3.removeFromTop(rh));
        decaySlider_.setBounds(c3.removeFromTop(rh));
        sustainSlider_.setBounds(c3.removeFromTop(rh));
        releaseSlider_.setBounds(c3.removeFromTop(rh));
        auto fRow = c3.removeFromTop(rh);
        filterEnableBtn_.setBounds(fRow.removeFromLeft(100));
        filterTypeBox_.setBounds(fRow);
        cutoffSlider_.setBounds(c3.removeFromTop(rh));
        resSlider_.setBounds(c3.removeFromTop(rh));
    }
}
