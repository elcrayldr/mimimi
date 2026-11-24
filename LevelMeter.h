#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    /**
     * Stereo peak LED meter with smooth decay and clip indicator.
     */
    class LevelMeter : public juce::Component
    {
    public:
        void setLevels(float leftPeak, float rightPeak, bool clipped);
        void paint(juce::Graphics& g) override;

    private:
        float displayL_ = 0.0f;
        float displayR_ = 0.0f;
        bool  clipped_  = false;
    };
}
