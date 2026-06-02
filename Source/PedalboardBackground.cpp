#include "PedalboardBackground.h"
#include "PluginProcessor.h"

PedalboardBackground::PedalboardBackground(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
    loadTexture();
}

void PedalboardBackground::loadTexture()
{
    juce::File texturePath;

    auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
    texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/pedalboard_bg.png");
    if (!texturePath.existsAsFile())
    {
        texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/pedalboard_bg.png");
    }

    if (texturePath.existsAsFile())
    {
        m_backgroundImage = juce::ImageCache::getFromFile(texturePath);
    }
}

void PedalboardBackground::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (m_backgroundImage.isValid())
    {
        g.drawImage(m_backgroundImage, bounds);
    }
    else
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
    }
}

void PedalboardBackground::resized()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    if (m_backgroundImage.isValid())
    {
        m_backgroundImage = m_backgroundImage.rescaled(bounds.getWidth(),
                                                        bounds.getHeight(),
                                                        juce::Graphics::highResamplingQuality);
        repaint();
    }
}
