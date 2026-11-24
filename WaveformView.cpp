#include "UI/Components/WaveformView.h"
#include "UI/Theme/ColorPalette.h"
#include "DSP/Utilities/DspMath.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void WaveformView::setData(std::shared_ptr<const SampleBuffer> buffer, const SampleZoneSpec* zone)
    {
        buffer_ = std::move(buffer);
        if (zone != nullptr)
        {
            zoneCopy_ = *zone;
            hasZone_  = true;
        }
        else
        {
            hasZone_ = false;
        }
        repaint();
    }

    void WaveformView::setZoom(double zoomFactor)
    {
        zoomFactor_ = juce::jlimit(1.0, 64.0, zoomFactor);
        const double maxStart = std::max(0.0, 1.0 - (1.0 / zoomFactor_));
        viewStartNorm_ = juce::jlimit(0.0, maxStart, viewStartNorm_);
        repaint();
    }

    void WaveformView::setScrollOffset(double norm0To1)
    {
        const double maxStart = std::max(0.0, 1.0 - (1.0 / zoomFactor_));
        viewStartNorm_ = juce::jlimit(0.0, maxStart, norm0To1 * maxStart);
        repaint();
    }

    void WaveformView::getVisibleSampleRange(int& outStart, int& outEnd) const
    {
        if (buffer_ == nullptr || buffer_->numSamples <= 1)
        {
            outStart = 0;
            outEnd   = 1;
            return;
        }

        const int total = buffer_->numSamples;
        const double visibleFrac = 1.0 / zoomFactor_;
        outStart = std::clamp(static_cast<int>(std::floor(viewStartNorm_ * total)), 0, total - 2);
        outEnd   = std::clamp(static_cast<int>(std::ceil((viewStartNorm_ + visibleFrac) * total)), outStart + 1, total - 1);
    }

    int WaveformView::sampleToPixelX(int sampleIdx) const
    {
        int visStart = 0, visEnd = 1;
        getVisibleSampleRange(visStart, visEnd);
        const float span = static_cast<float>(std::max(1, visEnd - visStart));
        const float norm = static_cast<float>(sampleIdx - visStart) / span;
        return static_cast<int>(std::round(norm * static_cast<float>(getWidth())));
    }

    int WaveformView::pixelXToSample(float x) const
    {
        if (buffer_ == nullptr || buffer_->numSamples <= 1)
            return 0;

        int visStart = 0, visEnd = 1;
        getVisibleSampleRange(visStart, visEnd);
        const float norm = juce::jlimit(0.0f, 1.0f, x / static_cast<float>(std::max(1, getWidth())));
        return std::clamp(visStart + static_cast<int>(std::round(norm * static_cast<float>(visEnd - visStart))),
                          0, buffer_->numSamples - 1);
    }

    void WaveformView::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        const float midY = bounds.getCentreY();
        g.setColour(Palette::borderSubtle.withAlpha(0.45f));
        g.drawHorizontalLine(static_cast<int>(midY), bounds.getX() + 4.0f, bounds.getRight() - 4.0f);

        if (buffer_ == nullptr || buffer_->numSamples < 2)
        {
            g.setColour(Palette::textMuted);
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::plain)));
            g.drawText("Drop or select a sample zone to inspect & edit waveform", bounds, juce::Justification::centred);
            return;
        }

        int visStart = 0, visEnd = buffer_->numSamples - 1;
        getVisibleSampleRange(visStart, visEnd);
        const int visLen = std::max(1, visEnd - visStart);

        // Selected region highlight
        if (selStart_ >= 0 && selEnd_ > selStart_)
        {
            const int sx = sampleToPixelX(selStart_);
            const int ex = sampleToPixelX(selEnd_);
            g.setColour(Palette::ledCyan.withAlpha(0.14f));
            g.fillRect(sx, 2, std::max(2, ex - sx), getHeight() - 4);
        }

        // Loop region highlight
        if (hasZone_ && zoneCopy_.loopEnabled)
        {
            const int lx0 = sampleToPixelX(zoneCopy_.loopStartSample);
            const int lx1 = sampleToPixelX(zoneCopy_.loopEndSample);
            g.setColour(Palette::goldPrimary.withAlpha(0.10f));
            g.fillRect(lx0, 2, std::max(2, lx1 - lx0), getHeight() - 4);
        }

        // Draw waveform peaks per pixel column
        const int w = std::max(1, getWidth());
        const float halfH = (bounds.getHeight() - 16.0f) * 0.5f;
        const float gainPreview = hasZone_
            ? juce::jlimit(0.15f, 3.0f, DspMath::dbToGain(zoneCopy_.gainDb + zoneCopy_.normalizeGainDb))
            : 1.0f;

        const float* ch0 = buffer_->audio.getReadPointer(0);
        const float* ch1 = (buffer_->numChannels > 1) ? buffer_->audio.getReadPointer(1) : ch0;

        juce::Path wavePath;
        for (int px = 0; px < w; ++px)
        {
            const int s0 = visStart + static_cast<int>((static_cast<double>(px) / w) * visLen);
            const int s1 = std::min(buffer_->numSamples - 1,
                                    visStart + static_cast<int>((static_cast<double>(px + 1) / w) * visLen));

            float minV = 0.0f, maxV = 0.0f;
            for (int s = s0; s <= s1; ++s)
            {
                const int readIdx = (hasZone_ && zoneCopy_.reverse) ? (buffer_->numSamples - 1 - s) : s;
                const float v = 0.5f * (ch0[readIdx] + ch1[readIdx]) * gainPreview;
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }
            minV = juce::jlimit(-1.0f, 1.0f, minV);
            maxV = juce::jlimit(-1.0f, 1.0f, maxV);

            const float yTop = midY - maxV * halfH;
            const float yBot = midY - minV * halfH;
            g.setColour(Palette::goldPrimary.withAlpha(0.82f));
            g.drawVerticalLine(px, yTop, std::max(yTop + 1.0f, yBot));
        }

        if (hasZone_)
        {
            const int effEnd = (zoneCopy_.endSample > 0) ? zoneCopy_.endSample : (buffer_->numSamples - 1);
            const int sx = sampleToPixelX(zoneCopy_.startSample);
            const int ex = sampleToPixelX(effEnd);

            // Dim trimmed-out areas
            g.setColour(Palette::bgObsidian.withAlpha(0.68f));
            if (sx > 0) g.fillRect(0, 0, sx, getHeight());
            if (ex < getWidth()) g.fillRect(ex, 0, getWidth() - ex, getHeight());

            // Fade In / Out curve overlay
            if (zoneCopy_.fadeInSamples > 0)
            {
                const int fx = sampleToPixelX(zoneCopy_.startSample + zoneCopy_.fadeInSamples);
                g.setColour(Palette::textPrimary.withAlpha(0.6f));
                g.drawLine(static_cast<float>(sx), bounds.getBottom() - 4.0f,
                           static_cast<float>(fx), bounds.getY() + 6.0f, 1.5f);
            }
            if (zoneCopy_.fadeOutSamples > 0)
            {
                const int fx = sampleToPixelX(std::max(zoneCopy_.startSample, effEnd - zoneCopy_.fadeOutSamples));
                g.setColour(Palette::textPrimary.withAlpha(0.6f));
                g.drawLine(static_cast<float>(fx), bounds.getY() + 6.0f,
                           static_cast<float>(ex), bounds.getBottom() - 4.0f, 1.5f);
            }

            // Start & End handles
            auto drawFlag = [&](int px, juce::Colour col, const juce::String& tag, bool topFlag)
            {
                g.setColour(col);
                g.drawVerticalLine(px, 2.0f, static_cast<float>(getHeight() - 2));
                const float flagY = topFlag ? 3.0f : static_cast<float>(getHeight() - 19);
                g.fillRoundedRectangle(static_cast<float>(px - 9), flagY, 18.0f, 16.0f, 3.0f);
                g.setColour(Palette::bgObsidian);
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText(tag, px - 9, static_cast<int>(flagY), 18, 16, juce::Justification::centred);
            };

            drawFlag(sx, Palette::ledGreen, "S", true);
            drawFlag(ex, Palette::ledRed,   "E", true);

            if (zoneCopy_.loopEnabled)
            {
                drawFlag(sampleToPixelX(zoneCopy_.loopStartSample), Palette::goldBright, "LS", false);
                drawFlag(sampleToPixelX(zoneCopy_.loopEndSample),   Palette::goldBright, "LE", false);
            }
        }
    }

    void WaveformView::mouseDown(const juce::MouseEvent& e)
    {
        if (buffer_ == nullptr || !hasZone_)
            return;

        const float x = e.position.x;
        const int effEnd = (zoneCopy_.endSample > 0) ? zoneCopy_.endSample : (buffer_->numSamples - 1);

        if (std::abs(x - sampleToPixelX(zoneCopy_.startSample)) < 11.0f)
            dragTarget_ = DragTarget::StartMarker;
        else if (std::abs(x - sampleToPixelX(effEnd)) < 11.0f)
            dragTarget_ = DragTarget::EndMarker;
        else if (zoneCopy_.loopEnabled && std::abs(x - sampleToPixelX(zoneCopy_.loopStartSample)) < 11.0f)
            dragTarget_ = DragTarget::LoopStartMarker;
        else if (zoneCopy_.loopEnabled && std::abs(x - sampleToPixelX(zoneCopy_.loopEndSample)) < 11.0f)
            dragTarget_ = DragTarget::LoopEndMarker;
        else
        {
            dragTarget_ = DragTarget::SelectRegion;
            dragAnchorSample_ = pixelXToSample(x);
            selStart_ = dragAnchorSample_;
            selEnd_   = dragAnchorSample_;
            repaint();
        }
    }

    void WaveformView::mouseDrag(const juce::MouseEvent& e)
    {
        if (buffer_ == nullptr || !hasZone_)
            return;

        const int s = pixelXToSample(e.position.x);
        const int effEnd = (zoneCopy_.endSample > 0) ? zoneCopy_.endSample : (buffer_->numSamples - 1);

        switch (dragTarget_)
        {
            case DragTarget::StartMarker:
                zoneCopy_.startSample = std::clamp(s, 0, effEnd - 2);
                if (onStartEndChanged) onStartEndChanged(zoneCopy_.startSample, effEnd);
                break;

            case DragTarget::EndMarker:
                zoneCopy_.endSample = std::clamp(s, zoneCopy_.startSample + 2, buffer_->numSamples - 1);
                if (onStartEndChanged) onStartEndChanged(zoneCopy_.startSample, zoneCopy_.endSample);
                break;

            case DragTarget::LoopStartMarker:
                zoneCopy_.loopStartSample = std::clamp(s, zoneCopy_.startSample, std::max(zoneCopy_.startSample, zoneCopy_.loopEndSample - 4));
                if (onLoopRangeChanged) onLoopRangeChanged(zoneCopy_.loopStartSample, zoneCopy_.loopEndSample);
                break;

            case DragTarget::LoopEndMarker:
                zoneCopy_.loopEndSample = std::clamp(s, zoneCopy_.loopStartSample + 4, effEnd);
                if (onLoopRangeChanged) onLoopRangeChanged(zoneCopy_.loopStartSample, zoneCopy_.loopEndSample);
                break;

            case DragTarget::SelectRegion:
                selStart_ = std::min(dragAnchorSample_, s);
                selEnd_   = std::max(dragAnchorSample_, s);
                if (onSelectionChanged) onSelectionChanged(selStart_, selEnd_);
                break;

            case DragTarget::None:
                break;
        }
        repaint();
    }

    void WaveformView::mouseUp(const juce::MouseEvent&)
    {
        dragTarget_ = DragTarget::None;
    }

    void WaveformView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (buffer_ == nullptr)
            return;

        if (std::abs(wheel.deltaY) > 0.001f)
        {
            const double factor = (wheel.deltaY > 0.0f) ? 1.25 : 0.80;
            const double cursorNorm = juce::jlimit(0.0, 1.0, static_cast<double>(e.position.x) / std::max(1, getWidth()));
            const double anchorFrac = viewStartNorm_ + cursorNorm / zoomFactor_;
            zoomFactor_ = juce::jlimit(1.0, 64.0, zoomFactor_ * factor);
            const double maxStart = std::max(0.0, 1.0 - 1.0 / zoomFactor_);
            viewStartNorm_ = juce::jlimit(0.0, maxStart, anchorFrac - cursorNorm / zoomFactor_);
            repaint();
        }
    }
}
