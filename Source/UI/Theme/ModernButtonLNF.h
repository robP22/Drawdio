#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

class ModernButtonLNF : public juce::LookAndFeel_V4
{
public:
    ModernButtonLNF()
    {
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xF2E8F1F5));
        setColour(juce::TextButton::textColourOnId,  juce::Colour(0xF2E8F1F5));
        setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF000000));
    }

    static void drawBody(juce::Graphics& g,
                         const juce::Path& bodyPath,
                         const juce::Rectangle<float>& bounds,
                         juce::Colour baseColour,
                         bool hovered, bool down, bool focused)
    {
        const float shadowOffset = std::min(2.5f, std::max(1.0f, bounds.getHeight() * 0.15f));

        if (!down)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.saveState();
            g.addTransform(juce::AffineTransform::translation(0.0f, shadowOffset));
            g.fillPath(bodyPath);
            g.restoreState();
        }

        if (down)
            baseColour = baseColour.darker(0.12f);

        auto topColor = baseColour.darker(0.85f);
        auto bottomColor = baseColour;
        g.setGradientFill(juce::ColourGradient(
            topColor, bounds.getX(), bounds.getY(),
            bottomColor, bounds.getX(), bounds.getBottom(), false));
        g.fillPath(bodyPath);

        const float highlightAlpha = down ? 0.05f : (hovered ? 0.32f : 0.16f);
        g.setColour(juce::Colours::white.withAlpha(highlightAlpha));
        g.strokePath(bodyPath, juce::PathStrokeType(1.0f));

        auto outlineColor = juce::Colour(0xD610141A);
        if (hovered && !down)
            outlineColor = outlineColor.brighter(0.2f);
        g.setColour(outlineColor);
        g.strokePath(bodyPath, juce::PathStrokeType(1.5f));

        if (focused)
        {
            g.setColour(juce::Colours::cyan.withAlpha(0.4f));
            g.strokePath(bodyPath, juce::PathStrokeType(3.0f));
        }
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float cornerRadius = std::max(2.0f, std::min(16.0f, bounds.getHeight() * 0.5f));

        juce::Path bodyPath;
        bodyPath.addRoundedRectangle(bounds, cornerRadius);

        drawBody(g, bodyPath, bounds, backgroundColour, isMouseOverButton, isButtonDown,
                 button.hasKeyboardFocus(true));
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool isMouseOverButton,
                        bool isButtonDown) override
    {
        juce::ignoreUnused(isMouseOverButton);

        auto font = getTextButtonFont(button, button.getHeight());
        g.setFont(font);

        auto textColor = button.findColour(button.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId);

        if (!button.isEnabled())
            textColor = textColor.withAlpha(0.45f);

        auto textBounds = button.getLocalBounds().toFloat();
        if (isButtonDown)
            textBounds = textBounds.translated(0.0f, 1.0f);

        g.setColour(textColor);
        g.drawText(button.getButtonText(), textBounds, juce::Justification::centred, true);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        const float fontSize = std::max(7.0f, std::min(13.0f, static_cast<float>(buttonHeight) * 0.65f));
        return juce::Font(fontSize, juce::Font::bold);
    }
};
