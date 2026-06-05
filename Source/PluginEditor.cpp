#include "PluginEditor.h"
#include "RenderUtils.h"

#include <cmath>
#include <utility>

namespace
{
juce::String colorName(PixelCanvasComponent::PixelColor color)
{
    switch (color)
    {
        case PixelCanvasComponent::PixelColor::White: return "WHITE";
        case PixelCanvasComponent::PixelColor::Red:   return "RED";
        case PixelCanvasComponent::PixelColor::Green: return "GREEN";
        case PixelCanvasComponent::PixelColor::Blue:  return "BLUE";
        case PixelCanvasComponent::PixelColor::Black:
        default:                                      return "BLACK";
    }
}

// Layout constants
constexpr int WindowWidth = 1400;
constexpr int WindowHeight = 800;
constexpr int BlobCount = 5;
constexpr float BlobSpacing = 8.0f;
constexpr float BlobMaxSize = 72.0f;
constexpr float BlobPadding = 20.0f;
constexpr int ButtonHeight = 16;
constexpr int ButtonSpacing = 5;
constexpr int ButtonAreaHeight = 80;
}

// Background component implementations - flat textures without decorative frames
WoodGrainBackground::WoodGrainBackground(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme)
{
    setInterceptsMouseClicks(false, false);
}

void WoodGrainBackground::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& woodTexture = m_resources.getTexture(ResourceManager::TextureId::WorkspaceWood);
    if (woodTexture.isValid())
    {
        g.drawImage(woodTexture, bounds.getX(), bounds.getY(), 
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, woodTexture.getWidth(), woodTexture.getHeight());
    }
}

PedalboardBackground::PedalboardBackground(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme)
{
    setInterceptsMouseClicks(false, false);
}

void PedalboardBackground::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& texture = m_resources.getTexture(ResourceManager::TextureId::PedalboardSprite);
    if (texture.isValid())
    {
        g.drawImage(texture, bounds.getX(), bounds.getY(),
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, texture.getWidth(), texture.getHeight());
    }
}

ColorPalette::ColorPalette(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme),
      m_blobs {{
          { PixelCanvasComponent::PixelColor::Red,   {} },
          { PixelCanvasComponent::PixelColor::Green, {} },
          { PixelCanvasComponent::PixelColor::Blue,  {} },
          { PixelCanvasComponent::PixelColor::White, {} },
          { PixelCanvasComponent::PixelColor::Black, {} }
      }}
{
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_clearButton);
    styleButton(m_undoButton, juce::Colour(0xFF4A90D9));
    styleButton(m_clearButton, juce::Colour(0xFFE74C3C));
}

void ColorPalette::paint(juce::Graphics& g)
{
    // Draw colorpalettebody.png sprite with black alpha transparency
    auto bounds = getLocalBounds().toFloat();
    const auto& bodyTexture = m_resources.getTexture(ResourceManager::TextureId::ColorPaletteBody);
    if (bodyTexture.isValid())
    {
        g.drawImage(bodyTexture,
                   bounds.getX(), bounds.getY(),
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, bodyTexture.getWidth(), bodyTexture.getHeight());
    }

    // DEBUG: Draw colored border
    g.setColour(juce::Colours::red.withAlpha(0.5f));
    g.drawRect(getLocalBounds(), 2);

    // Draw blob shadows, colors, selection on top
    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const auto& blob = m_blobs[static_cast<size_t>(i)];
        const auto blobBounds = blob.bounds;
        const auto paintColour = m_theme.canvasPixelColour(static_cast<uint8_t>(blob.color));
        const bool selected = blob.color == m_selectedColor;
        const bool hovered = i == m_hoveredBlob;

        if (selected)
        {
            g.setColour(m_theme.paletteSelectionFill(paintColour));
            g.fillEllipse(blobBounds.expanded(7.0f));
            g.setColour(m_theme.paletteSelectionOutline());
            g.drawEllipse(blobBounds.expanded(5.0f), 2.0f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillEllipse(blobBounds.translated(0.0f, selected ? 3.0f : 4.0f));

        auto body = blobBounds.translated(0.0f, selected ? -3.0f : 0.0f);
        g.setColour(paintColour.darker(0.25f));
        g.fillEllipse(body);

        g.setColour(paintColour.brighter(0.08f));
        g.fillEllipse(body.reduced(body.getWidth() * 0.07f, body.getHeight() * 0.12f));

        g.setColour(paintColour.brighter(0.45f).withAlpha(blob.color == PixelCanvasComponent::PixelColor::Black ? 0.18f : 0.42f));
        g.fillEllipse(body.withSizeKeepingCentre(body.getWidth() * 0.42f, body.getHeight() * 0.22f)
                          .translated(-body.getWidth() * 0.12f, -body.getHeight() * 0.18f));

        if (hovered)
        {
            g.setColour(m_theme.paletteHoverOutline());
            g.drawEllipse(body.expanded(2.0f), 1.2f);
        }
    }
}

void ColorPalette::resized()
{
    auto area = getLocalBounds();
    auto blobsArea = area.withTrimmedTop(BlobPadding).withTrimmedBottom(BlobPadding);

    // Position blobs using constants
    const auto blobSize = juce::jmin(blobsArea.getHeight() - 10.0f, BlobMaxSize);
    const float totalWidth = blobSize * BlobCount + BlobSpacing * (BlobCount - 1);
    float startX = blobsArea.getCentreX() - totalWidth / 2.0f;
    float blobY = blobsArea.getCentreY() - blobSize / 2.0f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(startX + static_cast<float>(i) * (blobSize + BlobSpacing),
                                           blobY,
                                           blobSize,
                                           blobSize);
        m_blobs[static_cast<size_t>(i)].bounds = slot;
    }

    // Position buttons on right side of palette, below blobs area
    auto buttonsArea = area.withTrimmedLeft(10).withTrimmedRight(10);
    const auto totalH = static_cast<float>(ButtonHeight * 2 + ButtonSpacing);
    auto buttonBounds = buttonsArea.withSizeKeepingCentre(buttonsArea.getWidth(), totalH);
    m_undoButton.setBounds(buttonBounds.removeFromTop(ButtonHeight).toType<int>());
    buttonBounds.removeFromTop(ButtonSpacing);
    m_clearButton.setBounds(buttonBounds.removeFromTop(ButtonHeight).toType<int>());
}

void ColorPalette::mouseDown(const juce::MouseEvent& event)
{
    const int index = hitTestBlob(event.position);
    if (index < 0)
        return;

    setSelectedColor(m_blobs[static_cast<size_t>(index)].color);

    if (m_onColorSelected)
        m_onColorSelected(m_selectedColor);
}

void ColorPalette::mouseMove(const juce::MouseEvent& event)
{
    const int hit = hitTestBlob(event.position);
    if (hit != m_hoveredBlob)
    {
        m_hoveredBlob = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ColorPalette::mouseExit(const juce::MouseEvent&)
{
    m_hoveredBlob = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void ColorPalette::setSelectedColor(PixelCanvasComponent::PixelColor color)
{
    m_selectedColor = color;
    repaint();
}

int ColorPalette::hitTestBlob(juce::Point<float> position) const
{
    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
        if (m_blobs[static_cast<size_t>(i)].bounds.expanded(6.0f).contains(position))
            return i;

    return -1;
}

CanvasTools::CanvasTools(const ThemeManager& theme)
    : m_theme(theme)
{
    styleButton(m_undoButton, m_theme.undoButtonAccent());
    styleButton(m_clearButton, m_theme.clearButtonAccent());

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

void CanvasTools::paint(juce::Graphics&)
{
    // EMPTY - no custom rendering
}

void CanvasTools::resized()
{
    auto area = getLocalBounds().withTrimmedTop(20).withTrimmedBottom(20);
    const auto buttonH = 16;
    const auto gap = 5;
    const auto totalH = buttonH * 2 + gap;
    auto buttonArea = area.withSizeKeepingCentre(area.getWidth() - 20, static_cast<float>(totalH));
    m_undoButton.setBounds(buttonArea.removeFromTop(buttonH));
    buttonArea.removeFromTop(gap);
    m_clearButton.setBounds(buttonArea.removeFromTop(buttonH));
}

void CanvasTools::styleButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF25292C));
    button.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.2f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

void ColorPalette::styleButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF25292C));
    button.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.2f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

CanvasModule::CanvasModule(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme),
      m_pixelCanvas(theme),
      m_palette(resources, theme),
      m_tools(theme)
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);
    addAndMakeVisible(m_tools);

    m_palette.setOnColorSelected([this](auto color)
    {
        m_pixelCanvas.setCurrentColor(color);
    });

    m_palette.setOnUndo([this]()
    {
        m_pixelCanvas.undo();
    });

    m_palette.setOnClear([this]()
    {
        if (m_onClear)
            m_onClear();

        m_pixelCanvas.clearCanvas();
    });

    m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Red);
}

void CanvasModule::paint(juce::Graphics& g)
{
    // DEBUG: Draw border
    g.setColour(juce::Colours::yellow.withAlpha(0.5f));
    g.drawRect(getLocalBounds(), 2);
}

void CanvasModule::resized()
{
    auto area = getLocalBounds().reduced(22, 20).translated(0, 85);
    auto bottom = area.removeFromBottom(380);
    area.removeFromBottom(14);

    const auto square = juce::jmin(area.getWidth(), area.getHeight()) + 140;
    auto canvasArea = area.withSizeKeepingCentre(square, square).translated(0, -5);
    m_pixelCanvas.setBounds(canvasArea.reduced(8));

    // Position palette and tools side by side (tools 50px on right)
    auto controls = bottom;
    auto toolsArea = controls.removeFromRight(50);

    m_palette.setBounds(controls);
    m_tools.setBounds(toolsArea);
}

void CanvasModule::refreshStatus()
{
    // Status display removed - no-op
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(p),
      audioProcessor(p),
      m_woodGrainBackground(m_resourceManager, m_theme),
      m_pedalboardBackground(m_resourceManager, m_theme),
      m_canvasModule(m_resourceManager, m_theme),
      m_pedalboardGrid(p, m_resourceManager, m_theme, m_routingManager)
{
    // Add background layers first, then content
    addAndMakeVisible(m_woodGrainBackground);
    addAndMakeVisible(m_pedalboardBackground);
    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardGrid);

    // Wood grain is bottom-most layer, pedalboard on top (alpha shows wood grain through)

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
        m_canvasModule.refreshStatus();
        checkForUpdates();  // Direct check instead of async message
    });

    m_canvasModule.setOnClear([this]()
    {
        m_routingManager.clearManualRouting();
        m_pedalboardGrid.updateRouting({});
        audioProcessor.setManualRouting({});
    });

    setSize(1400, 800);

    // Start with an update check
    juce::MessageManager::callAsync([this]() { checkForUpdates(); });
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    stopTimer();
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    // DEBUG: Draw main border first
    g.setColour(juce::Colours::green.withAlpha(0.5f));
    g.drawRect(getLocalBounds(), 2);

    g.fillAll(m_theme.editorBackground());

    // DEBUG: Draw grid lines on top (foreground)
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    auto bounds = getLocalBounds();
    for (int x = 0; x < bounds.getWidth(); x += 50)
    {
        g.drawLine(static_cast<float>(x), 0, static_cast<float>(x), static_cast<float>(bounds.getHeight()));
    }
    for (int y = 0; y < bounds.getHeight(); y += 50)
    {
        g.drawLine(0, static_cast<float>(y), static_cast<float>(bounds.getWidth()), static_cast<float>(y));
    }
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto content = juce::Rectangle<int>(50, 0, bounds.getWidth() - 50, bounds.getHeight());
    // Give more width to pedalboard (right side), less to canvas (left side)
    const auto pedalW = juce::jlimit(680, 780, content.getWidth() - 400) - 40;
    auto pedalArea = content.removeFromRight(pedalW);

    // Pedalboard grid with internal padding (top and bottom)
    auto gridArea = pedalArea.withTrimmedTop(30).withTrimmedBottom(30)
                        .withSizeKeepingCentre(pedalArea.getWidth() - 60, pedalArea.getHeight() - 40);

    // Wood grain fills entire window as bottom layer
    m_woodGrainBackground.setBounds(bounds);
    // Pedalboard sprite on right side only
    m_pedalboardBackground.setBounds(pedalArea);

    m_canvasModule.setBounds(content);
    m_pedalboardGrid.setBounds(gridArea);
}

void DrawdioProcessorEditor::triggerRecompile()
{
    const auto& grid = m_canvasModule.getPixelCanvas().getGridData();
    audioProcessor.getMessageQueue().pushSnapshot(grid.data());
    audioProcessor.setGridData(grid);
    audioProcessor.getCompilerThread().notify();
}

void DrawdioProcessorEditor::checkForUpdates()
{
    // Check if UI needs an update (called on message thread)
    if (!audioProcessor.consumeUINotification())
        return;  // No updates needed

    const auto previousRevision = m_seenConfigRevision;
    audioProcessor.consumeCompiledResultIfAvailable();
    m_seenConfigRevision = audioProcessor.getConfigRevision();
    const bool configChanged = m_seenConfigRevision != previousRevision;

    m_pedalboardGrid.syncPedals();

    auto config = audioProcessor.getDSPProcessor().getCurrentConfig();
    if (config)
    {
        for (auto& param : config->parameters)
        {
            const int chainPos = static_cast<int>(param.targetDspNodeRegister);
            if (chainPos >= 0 && chainPos < static_cast<int>(config->routingSlotOrder.size()))
            {
                const int slotIdx = config->routingSlotOrder[static_cast<size_t>(chainPos)];
                if (auto* pedal = m_pedalboardGrid.getPedal(slotIdx))
                    pedal->setKnobValue(static_cast<int>(param.parameterToken), param.currentValue);
            }
        }

        if (configChanged || config->routingSlotOrder != m_lastRoutingOrder)
        {
            m_lastRoutingOrder = config->routingSlotOrder;
            m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
        }
    }
    else if (!m_lastRoutingOrder.empty())
    {
        m_lastRoutingOrder.clear();
        m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
    }

    m_canvasModule.refreshStatus();

    // Trigger repaint only if changes were made
    repaint();
}

void DrawdioProcessorEditor::timerCallback()
{
    checkForUpdates();
}
