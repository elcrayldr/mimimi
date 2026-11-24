#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cray
{
    /**
     * Visual ADSR envelope curve display in obsidian/gold styling.
     */
    class EnvelopeDisplay : public juce::Component
    {
    public:
        void setAdsr(float attackSec, float decaySec, float sustain01, float releaseSec);
        void paint(juce::Graphics& g) override;

    private:
        float attack_  = 0.01f;
        float decay_   = 0.25f;
        float sustain_ = 0.75f;
        float release_ = 0.30f;
    };
}
