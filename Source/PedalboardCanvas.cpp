#include "PedalboardCanvas.h"
#include "PluginProcessor.h"

PedalboardCanvas::PedalboardCanvas(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
    // Load texture immediately
    loadTexture();
}

void PedalboardCanvas::loadTexture()
{
    // Try development path first
    auto texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/pedalboard_bg.png");

    if (!texturePath.existsAsFile())
    {
        // Try plugin bundle path
        auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
        texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/pedalboard_bg.png");
    }

    if (texturePath.existsAsFile())
    {
        m_pedalboardImage = juce::ImageCache::getFromFile(texturePath);
    }
}

void PedalboardCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (m_pedalboardImage.isValid())
    {
        // Don't rescale every paint - use the pre-scaled image
        g.drawImage(m_pedalboardImage, bounds);
    }
    else
    {
        // Fallback if image not loaded
        g.fillAll(juce::Colour(0xFF1A1A1A));
    }
}

void PedalboardCanvas::resized()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    // Rescale image to fit bounds
    if (m_pedalboardImage.isValid())
    {
        m_pedalboardImage = m_pedalboardImage.rescaled(bounds.getWidth(),
                                                        bounds.getHeight(),
                                                        juce::Graphics::highResamplingQuality);
    }
}
