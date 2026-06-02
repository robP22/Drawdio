#include "PedalboardCanvas.h"
#include "PluginProcessor.h"

PedalboardCanvas::PedalboardCanvas(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
}

void PedalboardCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 9.0f), 14.0f);

    juce::ColourGradient frameGradient(juce::Colour(0xFF5A3822), bounds.getX(), bounds.getY(),
                                       juce::Colour(0xFF1D120B), bounds.getX(), bounds.getBottom(), false);
    frameGradient.addColour(0.48, juce::Colour(0xFF3B2415));
    g.setGradientFill(frameGradient);
    g.fillRoundedRectangle(bounds, 14.0f);

    juce::Random grainRandom(0xB04D);
    for (int i = 0; i < 180; ++i)
    {
        const float y = bounds.getY() + grainRandom.nextFloat() * bounds.getHeight();
        g.setColour((i % 4 == 0 ? juce::Colour(0xFF8B5835) : juce::Colour(0xFF130C08)).withAlpha(0.06f));
        g.drawLine(bounds.getX() + 8.0f,
                   y,
                   bounds.getRight() - 8.0f,
                   y + grainRandom.nextFloat() * 8.0f - 4.0f,
                   0.8f + grainRandom.nextFloat());
    }

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 13.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds, 14.0f, 2.0f);

    if (m_feltImage.isValid())
        g.drawImageAt(m_feltImage, m_feltBounds.getX(), m_feltBounds.getY());
    else
        g.fillRoundedRectangle(m_feltBounds.toFloat(), 9.0f);

    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawRoundedRectangle(m_feltBounds.toFloat().expanded(2.0f), 10.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(m_feltBounds.toFloat().reduced(1.0f), 8.0f, 1.0f);
}

void PedalboardCanvas::resized()
{
    auto bounds = getLocalBounds();
    m_boardBounds = bounds.reduced(2);
    m_feltBounds = m_boardBounds.reduced(21, 19);
    rebuildFeltImage();
}

void PedalboardCanvas::rebuildFeltImage()
{
    if (m_feltBounds.isEmpty())
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
        m_feltImage = juce::ImageCache::getFromFile(texturePath);
        if (m_feltImage.isValid())
        {
            // Resize texture to fit the felt bounds
            m_feltImage = m_feltImage.rescaled(m_feltBounds.getWidth(),
                                               m_feltBounds.getHeight(),
                                               juce::Graphics::highResamplingQuality);
            return;
        }
    }

    // Fallback: Procedural felt texture
    m_feltImage = juce::Image(juce::Image::RGB,
                              m_feltBounds.getWidth(),
                              m_feltBounds.getHeight(),
                              false);
    juce::Graphics felt(m_feltImage);
    juce::ColourGradient feltGradient(juce::Colour(0xFF24292A), 0.0f, 0.0f,
                                      juce::Colour(0xFF070909), 0.0f,
                                      static_cast<float>(m_feltBounds.getHeight()),
                                      false);
    feltGradient.addColour(0.45, juce::Colour(0xFF151A1A));
    felt.setGradientFill(feltGradient);
    felt.fillAll();

    juce::Random random(0xF317);
    for (int i = 0; i < 3800; ++i)
    {
        const int x = random.nextInt(m_feltBounds.getWidth());
        const int y = random.nextInt(m_feltBounds.getHeight());
        felt.setColour((i % 3 == 0 ? juce::Colours::white : juce::Colours::black)
                           .withAlpha(0.018f + random.nextFloat() * 0.028f));
        felt.fillRect(x, y, 1, 1);
    }

    for (int y = 0; y < m_feltBounds.getHeight(); y += 4)
    {
        felt.setColour(juce::Colours::white.withAlpha(0.012f));
        felt.drawHorizontalLine(y, 0.0f, static_cast<float>(m_feltBounds.getWidth()));
    }
}
