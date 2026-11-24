#pragma once

#include "Library/InstrumentDefinition.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cray
{
    /**
     * Visual Key & Velocity Zone mapper for the Instrument Builder.
     * Displays MIDI keys 0..127 horizontally and Velocity (or Layers) vertically.
     * Clicking selects a zone; dragging left/right edges resizes key ranges in real time.
     */
    class KeyZoneMapView : public juce::Component
    {
    public:
        void setInstrument(InstrumentDefinition* def, int activeLayerFilter = -1, int selectedZoneIdx = -1);

        std::function<void(int zoneIndex)> onZoneSelected;
        std::function<void()>              onZonesModified;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        enum class EdgeDrag { None, MoveLowKey, MoveHighKey, MoveRoot };

        juce::Rectangle<float> getZoneRect(const SampleZoneSpec& z, const juce::Rectangle<float>& gridArea) const;
        int xToMidiNote(float x, const juce::Rectangle<float>& gridArea) const;

        InstrumentDefinition* instrument_      = nullptr;
        int                   layerFilter_     = -1; // -1 = show all layers
        int                   selectedZoneIdx_ = -1;
        EdgeDrag              dragMode_        = EdgeDrag::None;
    };
}
