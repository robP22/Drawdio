#include "RenderUtils.h"

namespace RenderUtils
{
void fillVerticalGloss(juce::Graphics& g,
                       juce::Rectangle<float> bounds,
                       juce::Colour top,
                       juce::Colour bottom,
                       float radius)
{
    juce::ColourGradient gradient(top, bounds.getX(), bounds.getY(),
                                  bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, radius);
}

void drawInsetPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 4.0f), radius);

    fillVerticalGloss(g, bounds, juce::Colour(0xFF252A2D), juce::Colour(0xFF111315), radius);
    drawHighlight(g, bounds.reduced(1.0f), juce::jmax(0.0f, radius - 1.0f), 0.12f);
    drawOutline(g, bounds, radius, juce::Colours::black.withAlpha(0.65f), 2.0f);
}

void drawSoftShadow(juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float alpha)
{
    g.setColour(juce::Colours::black.withAlpha(alpha));
    g.fillRoundedRectangle(bounds, radius);
}

void drawOutline(juce::Graphics& g,
                 juce::Rectangle<float> bounds,
                 float radius,
                 juce::Colour colour,
                 float thickness)
{
    g.setColour(colour);
    g.drawRoundedRectangle(bounds, radius, thickness);
}

void drawHighlight(juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float alpha)
{
    drawOutline(g, bounds, radius, juce::Colours::white.withAlpha(alpha), 1.0f);
}

void drawImageScaled(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds)
{
    if (image.isValid() && !bounds.isEmpty())
        g.drawImage(image,
                    bounds.getX(), bounds.getY(),
                    bounds.getWidth(), bounds.getHeight(),
                    0, 0,
                    image.getWidth(), image.getHeight(),
                    false);
}

void drawTextureClippedToRoundedRect(juce::Graphics& g,
                                     const juce::Image& image,
                                     juce::Rectangle<float> bounds,
                                     float radius,
                                     float opacity)
{
    if (!image.isValid() || bounds.isEmpty() || opacity <= 0.0f)
        return;

    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(bounds, radius);
    g.reduceClipRegion(clip, juce::AffineTransform());
    g.setOpacity(juce::jlimit(0.0f, 1.0f, opacity));
    g.drawImage(image,
                bounds.getX(), bounds.getY(),
                bounds.getWidth(), bounds.getHeight(),
                0, 0,
                image.getWidth(), image.getHeight(),
                false);
}

void drawSprite(juce::Graphics& g,
                const ResourceManager& resources,
                ResourceManager::SpriteId spriteId,
                juce::Rectangle<float> destination,
                float opacity)
{
    if (destination.isEmpty() || opacity <= 0.0f)
        return;

    const auto& frame = resources.getSpriteFrame(spriteId);
    const auto& sheet = resources.getSpriteSheet(frame.sheetId);
    if (!sheet.image.isValid() || frame.source.isEmpty())
        return;

    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(juce::jlimit(0.0f, 1.0f, opacity));
    g.drawImage(sheet.image,
                destination.getX(), destination.getY(),
                destination.getWidth(), destination.getHeight(),
                frame.source.getX(), frame.source.getY(),
                frame.source.getWidth(), frame.source.getHeight(),
                false);
}

void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(thickness,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}
}
