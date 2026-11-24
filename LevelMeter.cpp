#include "UI/Components/LevelMeter.h"
#include "UI/Theme/ColorPalette.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void LevelMeter::setLevels(float leftPeak, float rightPeak, bool clipped)
    {
        displayL_ = std::max(leftPeak, displayL_ * 0.78f);
        displayR_ = std::max(rightPeak, displayR_ * 0.78f);
        clipped_  = clipped;
        repaint();
    }

    void LevelMeter::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(bounds, 3.0f);

        auto drawBar = [&](juce::Rectangle<float> r, float lvl)
        {
            const float norm = juce::jlimit(0.0f, 1.0f, std::pow(lvl, 0.55f));
            const float fillW = r.getWidth() * norm;
            if (fillW > 1.0f)
            {
                juce::ColourGradient grad(Palette::ledGreen, r.getX(), 0.0f,
                                          Palette::ledRed, r.getRight(), 0.0f, false);
                grad.addColour(0.68, Palette::goldBright);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(r.withWidth(fillW), 2.0f);
            }
        };

        const auto inner = bounds.reduced(3.0f);
        const float h = (inner.getHeight() - 2.0f) * 0.5f;
        drawBar(inner.withHeight(h), displayL_);
        drawBar(inner.withY(inner.getY() + h + 2.0f).withHeight(h), displayR_);

        if (clipped_)
        {
            g.setColour(Palette::ledRed);
            g.fillEllipse(bounds.getRight() - 7.0f, bounds.getY() + 2.0f, 5.0f, 5.0f);
        }
    }
}
