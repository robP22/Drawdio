#include "PluginEditor.h"

PaletteTools::PaletteTools()
{
    loadTexture();
    repaint();

    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_clearButton);

    m_undoButton.onClick = [this]()
    {
        if (m_onUndo)
            m_onUndo();
    };

    m_clearButton.onClick = [this]()
    {
        if (m_onClear)
            m_onClear();
    };
}

void PaletteTools::loadTexture()
{
    juce::File texturePath;

    // Try plugin bundle path first
    auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
    texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/palette_body.png");
    if (!texturePath.existsAsFile())
    {
        texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/palette_body.png");
    }

    if (texturePath.existsAsFile())
    {
        m_texture = juce::ImageCache::getFromFile(texturePath);
    }
}

void PaletteTools::paint(juce::Graphics& g)
{
    if (m_texture.isValid())
    {
        g.drawImage(m_texture, getLocalBounds().toFloat());

        // Draw hover highlight on selected color slot
        if (m_hoveredColor >= 0 && m_hoveredColor < 5)
        {
            auto& slot = m_colorSlots[static_cast<size_t>(m_hoveredColor)];
            if (slot.isEmpty())
                return;

            // Subtle highlight
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawRoundedRectangle(slot.reduced(2.0f), 6.0f, 2.0f);

            // Selection indicator
            if (static_cast<int>(m_selectedColor) == m_hoveredColor)
            {
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.drawRoundedRectangle(slot.expanded(-2.0f), 8.0f, 3.0f);
            }
        }
    }
    else
    {
        // Fallback if texture not loaded
        g.fillAll(juce::Colour(0xFF1A1A1A));

        // Draw color slots
        for (int i = 0; i < 5; ++i)
        {
            if (!m_colorSlots[static_cast<size_t>(i)].isEmpty())
            {
                auto color = static_cast<PixelCanvasComponent::PixelColor>(i);
                g.setColour(PixelCanvasComponent::colourForPixel(color));
                g.fillRoundedRectangle(m_colorSlots[static_cast<size_t>(i)], 8.0f);
            }
        }
    }
}

void PaletteTools::resized()
{
    auto bounds = getLocalBounds();

    if (!bounds.isEmpty())
    {
        m_texture = m_texture.rescaled(bounds.getWidth(), bounds.getHeight(),
                                      juce::Graphics::highResamplingQuality);

        const float colorAreaWidth = bounds.getWidth() * 0.6f;
        const int slotCount = 5;
        const float slotW = colorAreaWidth / slotCount;
        const float slotH = bounds.getHeight() * 0.7f;
        const float yOffset = (bounds.getHeight() - slotH) / 2.0f;

        for (int i = 0; i < slotCount; ++i)
        {
            m_colorSlots[static_cast<size_t>(i)] = juce::Rectangle<float>(
                static_cast<float>(i) * slotW + slotW * 0.1f,
                yOffset,
                slotW * 0.8f,
                slotH
            );
        }

        auto buttonArea = bounds.withX(bounds.getX() + colorAreaWidth);
        buttonArea = buttonArea.reduced(15, 25);

        const auto buttonH = buttonArea.getHeight() / 2 - 15;
        m_undoButton.setBounds(buttonArea.removeFromTop(buttonH).toNearestInt());
        buttonArea.removeFromTop(30);
        m_clearButton.setBounds(buttonArea.removeFromTop(buttonH).toNearestInt());

        repaint();
    }
}

void PaletteTools::mouseDown(const juce::MouseEvent& event)
{
    const int colorIdx = hitTestColor(event.position);
    if (colorIdx >= 0)
    {
        m_selectedColor = static_cast<PixelCanvasComponent::PixelColor>(colorIdx);
        repaint();

        if (m_onColorSelected)
            m_onColorSelected(m_selectedColor);
    }
}

void PaletteTools::mouseMove(const juce::MouseEvent& event)
{
    const int hit = hitTestColor(event.position);
    if (hit != m_hoveredColor)
    {
        m_hoveredColor = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void PaletteTools::mouseExit(const juce::MouseEvent&)
{
    m_hoveredColor = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

int PaletteTools::hitTestColor(juce::Point<float> pos) const
{
    for (int i = 0; i < 5; ++i)
    {
        if (m_colorSlots[static_cast<size_t>(i)].expanded(6.0f).contains(pos))
            return i;
    }
    return -1;
}

CanvasModule::CanvasModule()
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_paletteTools);

    m_paletteTools.setOnColorSelected([this](auto color)
    {
        m_pixelCanvas.setCurrentColor(color);
    });

    m_paletteTools.setOnUndo([this]()
    {
        m_pixelCanvas.undo();
    });

    m_paletteTools.setOnClear([this]()
    {
        if (m_onClear)
            m_onClear();

        m_pixelCanvas.clearCanvas();
    });
}

void CanvasModule::paint(juce::Graphics& g)
{
    // Transparent - show wood background underneath
}

void CanvasModule::resized()
{
    auto area = getLocalBounds().reduced(10, 10);

    // Canvas on top, taking most of the height
    const int canvasHeight = area.getHeight() * 3 / 4;
    m_pixelCanvas.setBounds(area.removeFromTop(canvasHeight));

    // Palette/tools below, centered and constrained to canvas width
    const int paletteHeight = area.getHeight();
    const int paletteWidth = juce::jmin(area.getWidth(), m_pixelCanvas.getWidth());
    const int paletteX = area.getX() + (area.getWidth() - paletteWidth) / 2;
    m_paletteTools.setBounds(paletteX, area.getY(), paletteWidth, paletteHeight);
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      m_pedalboardBackground(p)
{
    juce::File texturePath;

    // Try plugin bundle path first
    auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
    texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/wood_texture_generic.png");
    if (!texturePath.existsAsFile())
    {
        texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/wood_texture_generic.png");
    }

    if (texturePath.existsAsFile())
    {
        m_woodBackground = juce::ImageCache::getFromFile(texturePath);
    }

    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardBackground);

    auto& pixelCanvas = m_canvasModule.getPixelCanvas();
    pixelCanvas.setGridData(audioProcessor.getGridData());
    pixelCanvas.setOnPenDown([this]()
    {
        audioProcessor.getPenDebouncer().penDown();
    });
    pixelCanvas.setOnPenUp([this]()
    {
        audioProcessor.getPenDebouncer().penUp();
    });
    pixelCanvas.setOnCanvasSnapshot([this](const auto&)
    {
        triggerRecompile();
    });

    setSize(1400, 800);
}

DrawdioProcessorEditor::~DrawdioProcessorEditor() = default;

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fill background first in case image has transparent alpha
    g.fillAll(juce::Colour(0xFF2A2A2A));

    if (m_woodBackground.isValid())
    {
        g.drawImage(m_woodBackground, bounds);
    }
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Rescale wood background to fill the component
    if (m_woodBackground.isValid())
    {
        m_woodBackground = m_woodBackground.rescaled(bounds.getWidth(),
                                                     bounds.getHeight(),
                                                     juce::Graphics::highResamplingQuality);
        repaint();
    }

    auto content = bounds.reduced(18, 16);
    const auto gap = 18;
    const auto totalWidth = content.getWidth();

    // Balance left/right: left is 75px wider
    const int leftBias = 75;
    const int leftW = (totalWidth - gap) / 2 + leftBias;
    const int rightW = totalWidth - leftW;

    auto leftArea = content.removeFromLeft(leftW);
    content.removeFromRight(gap);
    auto rightArea = content;  // Remaining is right side

    m_canvasModule.setBounds(leftArea);
    m_canvasModule.resized();
    m_pedalboardBackground.setBounds(rightArea);
    m_pedalboardBackground.resized();
}

void DrawdioProcessorEditor::triggerRecompile()
{
    const auto& grid = m_canvasModule.getPixelCanvas().getGridData();
    audioProcessor.getMessageQueue().pushSnapshot(grid.data());
    audioProcessor.setGridData(grid);
    audioProcessor.getCompilerThread().notify();
}
