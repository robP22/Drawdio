#include "PedalboardCanvas.h"
#include "PluginProcessor.h"

PedalboardCanvas::PedalboardCanvas(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
}

void PedalboardCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (m_pedalboardImage.isValid())
    {
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
    rebuildPedalboardImage();
}

void PedalboardCanvas::rebuildPedalboardImage()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    // Try to load texture from plugin bundle
    auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
    auto texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/pedalboard_bg.png");

    if (!texturePath.existsAsFile())
    {
        // Fallback for development builds
        texturePath = juce::File::getCurrentWorkingDirectory().getChildFile("Assets/Textures/pedalboard_bg.png");
    }

    if (texturePath.existsAsFile())
    {
        m_pedalboardImage = juce::ImageCache::getFromFile(texturePath);
        if (m_pedalboardImage.isValid())
        {
            // Resize texture to fit the component bounds
            m_pedalboardImage = m_pedalboardImage.rescaled(bounds.getWidth(),
                                                          bounds.getHeight(),
                                                          juce::Graphics::highResamplingQuality);
            return;
        }
    }

    // No fallback - just leave image invalid and paint() will draw dark background
    m_pedalboardImage = juce::Image();
}
