#include "PedalboardBackground.h"
#include "PluginProcessor.h"

PedalboardBackground::PedalboardBackground(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
    loadTexture();

    for (int i = 0; i < 6; ++i)
    {
        auto type = audioProcessor.getPedalSlot(i);
        m_pedals[i] = std::make_unique<PedalComponent>(processor, i, type);
        addAndMakeVisible(*m_pedals[i]);
    }
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

    if (m_pedals[0])
    {
        const int cols = 2;
        const int rows = 3;
        const int gap = 12;
        const auto cellW = (bounds.getWidth() - gap * (cols + 1)) / cols;
        const auto cellH = (bounds.getHeight() - gap * (rows + 1)) / rows;

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const int idx = row * cols + col;
                const auto x = bounds.getX() + gap + col * (cellW + gap);
                const auto y = bounds.getY() + gap + row * (cellH + gap);
                m_pedals[idx]->setBounds(static_cast<int>(x), static_cast<int>(y),
                                         static_cast<int>(cellW), static_cast<int>(cellH));
            }
        }
    }
}

void PedalboardBackground::syncFromProcessor()
{
    for (int i = 0; i < 6; ++i)
    {
        if (m_pedals[i])
            m_pedals[i]->syncFromProcessor();
    }
}

PedalComponent* PedalboardBackground::getPedalComponent(int slot) const
{
    if (slot >= 0 && slot < 6)
        return m_pedals[slot].get();
    return nullptr;
}
