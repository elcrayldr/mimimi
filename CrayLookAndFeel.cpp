#include "UI/Theme/CrayLookAndFeel.h"
#include "UI/Theme/ColorPalette.h"

namespace cray
{
    CrayLookAndFeel::CrayLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, Palette::bgObsidian);
        setColour(juce::Label::textColourId, Palette::textPrimary);
        setColour(juce::TextEditor::backgroundColourId, Palette::bgGraphite);
        setColour(juce::TextEditor::textColourId, Palette::textPrimary);
        setColour(juce::TextEditor::outlineColourId, Palette::borderSubtle);
        setColour(juce::TextEditor::focusedOutlineColourId, Palette::goldPrimary);
        setColour(juce::ComboBox::backgroundColourId, Palette::bgGraphite);
        setColour(juce::ComboBox::textColourId, Palette::textPrimary);
        setColour(juce::ComboBox::outlineColourId, Palette::borderSubtle);
        setColour(juce::PopupMenu::backgroundColourId, Palette::bgPanel);
        setColour(juce::PopupMenu::textColourId, Palette::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::goldDim);
        setColour(juce::PopupMenu::highlightedTextColourId, Palette::textPrimary);
        setColour(juce::ScrollBar::thumbColourId, Palette::borderBright);
        setColour(juce::ListBox::backgroundColourId, Palette::bgGraphite);
        setColour(juce::ListBox::textColourId, Palette::textPrimary);
    }

    void CrayLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
    {
        const auto bounds  = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                    static_cast<float>(width), static_cast<float>(height)).reduced(4.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  center = bounds.getCentre();
        const float angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer track arc
        const float arcRadius = radius - 2.0f;
        juce::Path trackPath;
        trackPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(Palette::bgObsidian);
        g.strokePath(trackPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active gold value arc
        if (slider.isEnabled())
        {
            juce::Path valPath;
            const bool bipolar = slider.getProperties().getWithDefault("bipolar", false);
            const float startA = bipolar ? (rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)) : rotaryStartAngle;
            valPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f,
                                  juce::jmin(startA, angle), juce::jmax(startA, angle), true);

            g.setColour(Palette::goldGlow);
            g.strokePath(valPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(Palette::goldPrimary);
            g.strokePath(valPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Machined knob body
        const float bodyRadius = juce::jmax(6.0f, arcRadius - 5.5f);
        const auto bodyRect = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(center);
        juce::ColourGradient grad(Palette::bgElevated, center.x, bodyRect.getY(),
                                  Palette::bgObsidian, center.x, bodyRect.getBottom(), false);
        g.setGradientFill(grad);
        g.fillEllipse(bodyRect);

        g.setColour(Palette::borderBright);
        g.drawEllipse(bodyRect, 1.1f);

        // Gold indicator line
        juce::Path pointer;
        const float pLen = bodyRadius * 0.68f;
        pointer.addRoundedRectangle(-1.3f, -bodyRadius + 2.0f, 2.6f, pLen, 1.0f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(center.x, center.y));
        g.setColour(slider.isEnabled() ? Palette::goldBright : Palette::textMuted);
        g.fillPath(pointer);
    }

    void CrayLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height));
        if (style == juce::Slider::LinearVertical)
        {
            const float trackW = 4.0f;
            const auto track = juce::Rectangle<float>(bounds.getCentreX() - trackW * 0.5f, bounds.getY() + 4.0f,
                                                      trackW, bounds.getHeight() - 8.0f);
            g.setColour(Palette::bgObsidian);
            g.fillRoundedRectangle(track, 2.0f);

            const auto fill = track.withTop(sliderPos);
            g.setColour(slider.isEnabled() ? Palette::goldPrimary : Palette::textMuted);
            g.fillRoundedRectangle(fill, 2.0f);

            g.setColour(Palette::goldBright);
            g.fillRoundedRectangle(bounds.getCentreX() - 7.0f, sliderPos - 3.0f, 14.0f, 6.0f, 2.0f);
        }
        else
        {
            const float trackH = 4.0f;
            const auto track = juce::Rectangle<float>(bounds.getX() + 4.0f, bounds.getCentreY() - trackH * 0.5f,
                                                      bounds.getWidth() - 8.0f, trackH);
            g.setColour(Palette::bgObsidian);
            g.fillRoundedRectangle(track, 2.0f);

            const auto fill = track.withRight(sliderPos);
            g.setColour(slider.isEnabled() ? Palette::goldPrimary : Palette::textMuted);
            g.fillRoundedRectangle(fill, 2.0f);

            g.setColour(Palette::goldBright);
            g.fillEllipse(sliderPos - 5.5f, bounds.getCentreY() - 5.5f, 11.0f, 11.0f);
        }
    }

    void CrayLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& /*backgroundColour*/,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool toggled = button.getToggleState();

        juce::Colour bg = toggled ? Palette::goldPrimary.withAlpha(0.22f) : Palette::bgCard;
        if (button.getProperties().getWithDefault("accent", false))
            bg = toggled ? Palette::goldBright : Palette::goldPrimary;

        if (shouldDrawButtonAsDown)
            bg = bg.darker(0.18f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter(0.10f);

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(toggled ? Palette::goldPrimary : Palette::borderSubtle);
        g.drawRoundedRectangle(bounds, 5.0f, toggled ? 1.4f : 1.0f);
    }

    void CrayLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const float boxSize = 14.0f;
        const auto box = juce::Rectangle<float>(2.0f, bounds.getCentreY() - boxSize * 0.5f, boxSize, boxSize);

        g.setColour(button.getToggleState() ? Palette::goldPrimary : Palette::bgObsidian);
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(shouldDrawButtonAsHighlighted ? Palette::goldBright : Palette::borderBright);
        g.drawRoundedRectangle(box, 3.0f, 1.0f);

        if (button.getToggleState())
        {
            g.setColour(Palette::bgObsidian);
            g.fillRoundedRectangle(box.reduced(3.5f), 1.5f);
        }

        g.setColour(button.isEnabled() ? Palette::textPrimary : Palette::textMuted);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        g.drawText(button.getButtonText(),
                   juce::Rectangle<float>(box.getRight() + 6.0f, 0.0f, bounds.getWidth() - box.getRight() - 6.0f, bounds.getHeight()),
                   juce::Justification::centredLeft, true);
    }

    void CrayLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                       juce::ComboBox& box)
    {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        g.setColour(isButtonDown ? Palette::bgElevated : Palette::bgGraphite);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(box.hasKeyboardFocus(false) ? Palette::goldPrimary : Palette::borderSubtle);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Down chevron
        const float cx = static_cast<float>(width) - 13.0f;
        const float cy = static_cast<float>(height) * 0.5f;
        juce::Path p;
        p.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour(Palette::goldPrimary);
        g.fillPath(p);
    }

    juce::Font CrayLookAndFeel::getComboBoxFont(juce::ComboBox&)
    {
        return juce::Font(juce::FontOptions(12.0f, juce::Font::plain));
    }

    juce::Font CrayLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
    {
        return juce::Font(juce::FontOptions(juce::jmin(13.0f, static_cast<float>(buttonHeight) * 0.54f), juce::Font::bold));
    }
}
