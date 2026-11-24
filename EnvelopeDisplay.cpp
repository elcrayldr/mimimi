#include "UI/Components/EnvelopeDisplay.h"
#include "UI/Theme/ColorPalette.h"
#include <algorithm>
#include <cmath>

namespace cray
{
    void EnvelopeDisplay::setAdsr(float attackSec, float decaySec, float sustain01, float releaseSec)
    {
        attack_  = std::max(0.001f, attackSec);
        decay_   = std::max(0.002f, decaySec);
        sustain_ = juce::jlimit(0.0f, 1.0f, sustain01);
        release_ = std::max(0.002f, releaseSec);
        repaint();
    }

    void EnvelopeDisplay::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(Palette::bgObsidian);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        const auto inner = bounds.reduced(8.0f, 8.0f);
        const float w = inner.getWidth();
        const float h = inner.getHeight();

        // Map durations logarithmically so short snappy urban envelopes remain clearly visible
        const float aW = std::sqrt(std::min(attack_, 4.0f) / 4.0f) * (w * 0.28f) + 6.0f;
        const float dW = std::sqrt(std::min(decay_, 4.0f) / 4.0f) * (w * 0.28f) + 6.0f;
        const float rW = std::sqrt(std::min(release_, 4.0f) / 4.0f) * (w * 0.28f) + 6.0f;
        const float sW = std::max(10.0f, w - (aW + dW + rW));

        const float x0 = inner.getX();
        const float yBottom = inner.getBottom();
        const float yTop    = inner.getY();
        const float ySus    = yBottom - sustain_ * h;

        const float xA = x0 + aW;
        const float xD = xA + dW;
        const float xS = xD + sW;
        const float xR = inner.getRight();

        juce::Path curve;
        curve.startNewSubPath(x0, yBottom);
        curve.quadraticTo(x0 + aW * 0.35f, yTop + h * 0.15f, xA, yTop);
        curve.quadraticTo(xA + dW * 0.30f, ySus, xD, ySus);
        curve.lineTo(xS, ySus);
        curve.quadraticTo(xS + rW * 0.30f, yBottom, xR, yBottom);

        juce::Path fill = curve;
        fill.lineTo(xR, yBottom);
        fill.lineTo(x0, yBottom);
        fill.closeSubPath();

        juce::ColourGradient fillGrad(Palette::goldPrimary.withAlpha(0.28f), 0.0f, yTop,
                                      Palette::goldPrimary.withAlpha(0.02f), 0.0f, yBottom, false);
        g.setGradientFill(fillGrad);
        g.fillPath(fill);

        g.setColour(Palette::goldBright);
        g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Node dots
        g.setColour(Palette::textPrimary);
        g.fillEllipse(xA - 3.0f, yTop - 3.0f, 6.0f, 6.0f);
        g.fillEllipse(xD - 3.0f, ySus - 3.0f, 6.0f, 6.0f);
        g.fillEllipse(xS - 3.0f, ySus - 3.0f, 6.0f, 6.0f);
    }
}
