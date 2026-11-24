#include "UI/Components/KeyZoneMapView.h"
#include "UI/Theme/ColorPalette.h"
#include "Library/InstrumentBuilder.h"
#include <algorithm>

namespace cray
{
    namespace
    {
        bool isBlackKey(int note)
        {
            const int n = note % 12;
            return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
        }
    }

    void KeyZoneMapView::setInstrument(InstrumentDefinition* def, int activeLayerFilter, int selectedZoneIdx)
    {
        instrument_      = def;
        layerFilter_     = activeLayerFilter;
        selectedZoneIdx_ = selectedZoneIdx;
        repaint();
    }

    int KeyZoneMapView::xToMidiNote(float x, const juce::Rectangle<float>& gridArea) const
    {
        const float norm = juce::jlimit(0.0f, 1.0f, (x - gridArea.getX()) / std::max(1.0f, gridArea.getWidth()));
        return std::clamp(static_cast<int>(std::floor(norm * 128.0f)), 0, 127);
    }

    juce::Rectangle<float> KeyZoneMapView::getZoneRect(const SampleZoneSpec& z, const juce::Rectangle<float>& gridArea) const
    {
        const float keyW = gridArea.getWidth() / 128.0f;
        const float x0   = gridArea.getX() + static_cast<float>(z.lowKey) * keyW;
        const float x1   = gridArea.getX() + static_cast<float>(z.highKey + 1) * keyW;

        const float yTop = gridArea.getBottom() - (static_cast<float>(z.highVelocity) / 127.0f) * gridArea.getHeight();
        const float yBot = gridArea.getBottom() - (static_cast<float>(std::max(0, z.lowVelocity - 1)) / 127.0f) * gridArea.getHeight();

        return juce::Rectangle<float>(x0, yTop, std::max(4.0f, x1 - x0), std::max(8.0f, yBot - yTop));
    }

    void KeyZoneMapView::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto area = bounds.reduced(4.0f);
        const auto pianoStrip = area.removeFromBottom(24.0f);
        const auto gridArea   = area;

        // Grid background octave lines
        const float keyW = gridArea.getWidth() / 128.0f;
        for (int n = 0; n < 128; ++n)
        {
            const float x = gridArea.getX() + static_cast<float>(n) * keyW;
            if (n % 12 == 0)
            {
                g.setColour(Palette::borderSubtle.withAlpha(0.65f));
                g.drawVerticalLine(static_cast<int>(x), gridArea.getY(), gridArea.getBottom());
            }

            // Piano strip at bottom
            const bool blk = isBlackKey(n);
            g.setColour(blk ? Palette::bgObsidian : Palette::bgCard);
            g.fillRect(x, pianoStrip.getY(), std::max(1.0f, keyW - 0.5f), blk ? pianoStrip.getHeight() * 0.65f : pianoStrip.getHeight());
            if (n % 12 == 0 && keyW > 3.5f)
            {
                g.setColour(Palette::textMuted);
                g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
                g.drawText("C" + juce::String((n / 12) - 1),
                           static_cast<int>(x), static_cast<int>(pianoStrip.getBottom() - 11), 22, 11,
                           juce::Justification::left);
            }
        }

        if (instrument_ == nullptr)
            return;

        static const juce::Colour layerColors[4] = {
            Palette::goldPrimary,
            Palette::ledCyan,
            Palette::ledGreen,
            Palette::ledAmber
        };

        for (int i = 0; i < static_cast<int>(instrument_->zones.size()); ++i)
        {
            const auto& z = instrument_->zones[static_cast<size_t>(i)];
            if (layerFilter_ >= 0 && z.layerIndex != layerFilter_)
                continue;

            const auto r = getZoneRect(z, gridArea);
            const auto baseCol = layerColors[static_cast<size_t>(std::max(0, z.layerIndex) % 4)];
            const bool selected = (i == selectedZoneIdx_);

            g.setColour(baseCol.withAlpha(selected ? 0.42f : 0.22f));
            g.fillRoundedRectangle(r.reduced(0.5f), 3.0f);

            g.setColour(selected ? Palette::textPrimary : baseCol);
            g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, selected ? 1.8f : 1.0f);

            // Root note marker
            const float rootX = gridArea.getX() + (static_cast<float>(z.rootNote) + 0.5f) * keyW;
            g.setColour(Palette::goldBright);
            g.fillEllipse(rootX - 3.0f, r.getBottom() - 8.0f, 6.0f, 6.0f);

            if (r.getWidth() > 28.0f)
            {
                g.setColour(Palette::textPrimary);
                g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
                const auto label = z.displayName + " (" + InstrumentBuilder::midiNoteToName(z.rootNote) + ")";
                g.drawText(label, r.reduced(4.0f, 2.0f), juce::Justification::topLeft, true);
            }
        }
    }

    void KeyZoneMapView::mouseDown(const juce::MouseEvent& e)
    {
        if (instrument_ == nullptr)
            return;

        auto area = getLocalBounds().toFloat().reduced(4.0f);
        area.removeFromBottom(24.0f);

        for (int i = static_cast<int>(instrument_->zones.size()) - 1; i >= 0; --i)
        {
            const auto& z = instrument_->zones[static_cast<size_t>(i)];
            if (layerFilter_ >= 0 && z.layerIndex != layerFilter_)
                continue;

            const auto r = getZoneRect(z, area);
            if (r.contains(e.position))
            {
                selectedZoneIdx_ = i;
                if (std::abs(e.position.x - r.getX()) < 7.0f)
                    dragMode_ = EdgeDrag::MoveLowKey;
                else if (std::abs(e.position.x - r.getRight()) < 7.0f)
                    dragMode_ = EdgeDrag::MoveHighKey;
                else if (e.mods.isAltDown())
                    dragMode_ = EdgeDrag::MoveRoot;
                else
                    dragMode_ = EdgeDrag::None;

                if (onZoneSelected)
                    onZoneSelected(i);
                repaint();
                return;
            }
        }
    }

    void KeyZoneMapView::mouseDrag(const juce::MouseEvent& e)
    {
        if (instrument_ == nullptr || dragMode_ == EdgeDrag::None)
            return;
        if (selectedZoneIdx_ < 0 || selectedZoneIdx_ >= static_cast<int>(instrument_->zones.size()))
            return;

        auto area = getLocalBounds().toFloat().reduced(4.0f);
        area.removeFromBottom(24.0f);
        const int note = xToMidiNote(e.position.x, area);
        auto& z = instrument_->zones[static_cast<size_t>(selectedZoneIdx_)];

        if (dragMode_ == EdgeDrag::MoveLowKey)
            z.lowKey = std::clamp(note, 0, z.highKey);
        else if (dragMode_ == EdgeDrag::MoveHighKey)
            z.highKey = std::clamp(note, z.lowKey, 127);
        else if (dragMode_ == EdgeDrag::MoveRoot)
            z.rootNote = note;

        repaint();
    }

    void KeyZoneMapView::mouseUp(const juce::MouseEvent&)
    {
        if (dragMode_ != EdgeDrag::None && onZonesModified)
            onZonesModified();
        dragMode_ = EdgeDrag::None;
    }
}
