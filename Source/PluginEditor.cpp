#include "PluginEditor.h"

PaletteTools::PaletteTools()
{
    loadTexture();

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
    auto texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/palette_body.png");
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
    }
    else
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
    }
}

void PaletteTools::resized()
{
    if (m_texture.isValid())
    {
        // Texture has 5 color slots on left, buttons on right
        // Assuming texture is ~600px wide with ~350px for colors, ~250px for buttons
        const int textureW = m_texture.getWidth();
        const int colorAreaW = textureW * 350 / 600;
        const int buttonAreaW = textureW - colorAreaW;

        auto bounds = getLocalBounds();
        m_texture = m_texture.rescaled(bounds.getWidth(), bounds.getHeight(),
                                       juce::Graphics::highResamplingQuality);

        // Buttons go in right portion
        auto buttonArea = bounds.removeFromRight(buttonAreaW * bounds.getWidth() / textureW);
        buttonArea = buttonArea.reduced(10, 20);

        const auto buttonH = buttonArea.getHeight() / 2 - 10;
        m_undoButton.setBounds(buttonArea.removeFromTop(buttonH));
        buttonArea.removeFromTop(20);
        m_clearButton.setBounds(buttonArea.removeFromTop(buttonH));
    }
    else
    {
        auto area = getLocalBounds().reduced(10, 20);
        const auto buttonH = (area.getHeight() - 30) / 2;
        m_undoButton.setBounds(area.removeFromTop(buttonH));
        area.removeFromTop(30);
        m_clearButton.setBounds(area.removeFromTop(buttonH));
    }
}

CanvasModule::CanvasModule()
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_paletteTools);

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
    auto area = getLocalBounds().reduced(12, 10);

    // Palette tools on the right side, canvas takes the rest
    auto paletteW = area.getWidth() / 4;  // Palette is about 1/4 of width
    m_paletteTools.setBounds(area.removeFromRight(paletteW));
    area.removeFromRight(10);

    // Canvas fills remaining space, but keep it square-ish
    const auto square = juce::jmin(area.getWidth(), area.getHeight());
    auto canvasArea = area.withSizeKeepingCentre(square, square);
    m_pixelCanvas.setBounds(canvasArea.reduced(4));
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      m_pedalboardCanvas(p)
{
    // Load wood texture
    auto texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/wood_texture_generic.png");
    if (texturePath.existsAsFile())
    {
        m_woodBackground = juce::ImageCache::getFromFile(texturePath);
    }

    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardCanvas);

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
    if (m_woodBackground.isValid())
    {
        g.drawImage(m_woodBackground, bounds);
    }
    else
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
    }
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Resize wood background if needed
    if (m_woodBackground.isValid())
    {
        m_woodBackground = m_woodBackground.rescaled(bounds.getWidth(),
                                                     bounds.getHeight(),
                                                     juce::Graphics::highResamplingQuality);
    }

    auto content = bounds.reduced(18, 16);
    const auto gap = 18;
    const auto pedalW = juce::jlimit(560, 620, content.getWidth() - 760);
    auto pedalArea = content.removeFromRight(pedalW);
    content.removeFromRight(gap);

    m_canvasModule.setBounds(content);
    m_pedalboardCanvas.setBounds(pedalArea);
}

void DrawdioProcessorEditor::triggerRecompile()
{
    const auto& grid = m_canvasModule.getPixelCanvas().getGridData();
    audioProcessor.getMessageQueue().pushSnapshot(grid.data());
    audioProcessor.setGridData(grid);
    audioProcessor.getCompilerThread().notify();
}
