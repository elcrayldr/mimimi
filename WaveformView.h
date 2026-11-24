#pragma once

#include "Engine/SampleData.h"
#include "Library/InstrumentDefinition.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cray
{
    /**
     * Interactive non-destructive waveform display:
     * - Zoom & horizontal scroll
     * - Draggable Start / End / Loop Start / Loop End markers
     * - Region selection for Normalize or inspection
     * - Visual Fade-In / Fade-Out curves and Gain/Reverse preview
     */
    class WaveformView : public juce::Component
    {
    public:
        void setData(std::shared_ptr<const SampleBuffer> buffer, const SampleZoneSpec* zone);
        void setZoom(double zoomFactor);
        double getZoom() const noexcept { return zoomFactor_; }
        void setScrollOffset(double norm0To1);

        int getSelectionStartSample() const noexcept { return selStart_; }
        int getSelectionEndSample() const noexcept { return selEnd_; }

        std::function<void(int startSample, int endSample)>         onStartEndChanged;
        std::function<void(int loopStartSample, int loopEndSample)> onLoopRangeChanged;
        std::function<void(int selStart, int selEnd)>               onSelectionChanged;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    private:
        enum class DragTarget
        {
            None = 0,
            StartMarker,
            EndMarker,
            LoopStartMarker,
            LoopEndMarker,
            SelectRegion
        };

        int sampleToPixelX(int sampleIdx) const;
        int pixelXToSample(float x) const;
        void getVisibleSampleRange(int& outStart, int& outEnd) const;

        std::shared_ptr<const SampleBuffer> buffer_;
        SampleZoneSpec                      zoneCopy_;
        bool                                hasZone_ = false;

        double zoomFactor_     = 1.0; // 1.0 = full sample
        double viewStartNorm_  = 0.0; // [0..1)
        int    selStart_       = -1;
        int    selEnd_         = -1;
        DragTarget dragTarget_ = DragTarget::None;
        int    dragAnchorSample_ = 0;
    };
}
