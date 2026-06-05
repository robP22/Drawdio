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
constexpr int ButtonWidth = 20;
constexpr int ButtonHeight = 14;
constexpr int ButtonSpacing = 5;
constexpr int ButtonAreaHeight = 80;
}

// Layout constants namespace for consistent spacing across all components
namespace Layout
{
    // CanvasModule layout
    constexpr int CanvasOuterMargin = 22;
    constexpr int CanvasVerticalMargin = 20;
    constexpr int PaletteHeight = 300;
    constexpr int CanvasPadding = 8;
    constexpr int CanvasTopMargin = 24;

    // ColorPalette layout
    constexpr int PaletteLeftMargin = 30;
    constexpr int PaletteRightMargin = 30;
    constexpr int PaletteTopMargin = 20;
    constexpr int PaletteBottomMargin = 20;
    constexpr float PaletteBlobShift = 10.0f;
    constexpr int PaletteButtonRightPadding = 25;

    // Editor layout
    constexpr int EditorContentLeftMargin = 50;
    constexpr int EditorPedalAreaMinWidth = 680;
    constexpr int EditorPedalAreaMaxWidth = 780;
    constexpr int EditorPedalAreaExtraDeduction = 40;
    constexpr int EditorGridBottomTrim = 30;
    constexpr int EditorGridWidthDeduction = 60;
    constexpr int EditorGridHeightDeduction = 20;
}

namespace GridLayout
{
    constexpr int RowCount = 2;
    constexpr int ColCount = 3;
    constexpr int GridTopPadding = 30;
    constexpr int GridSidePadding = 8;
    constexpr int PedalWidthMin = 180;
    constexpr int PedalWidthMax = 220;
    constexpr int PedalHeightMin = 240;
    constexpr int PedalHeightMax = 280;
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
    auto area = getLocalBounds()
        .removeFromLeft(Layout::PaletteLeftMargin)
        .removeFromRight(Layout::PaletteRightMargin)
        .removeFromTop(Layout::PaletteTopMargin)
        .removeFromBottom(Layout::PaletteBottomMargin);

    // Calculate blob size to use available height minus some padding
    const auto blobSize = juce::jmin(area.getHeight() - 20.0f, BlobMaxSize);
    
    // Position blobs horizontally, spread across the available width
    // Leave space on right for buttons (button area width = 60)
    const float availableForBlobs = area.getWidth() - 70.0f;  // 60 for buttons + 10 padding
    const float totalBlobWidth = blobSize * BlobCount + BlobSpacing * (BlobCount - 1);
    
    // Center blobs in the available space (left portion)
    const float startX = area.getX() + (availableForBlobs - totalBlobWidth) / 2.0f;
    const float blobY = area.getCentreY() - blobSize / 2.0f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(startX + static_cast<float>(i) * (blobSize + BlobSpacing),
                                           blobY,
                                           blobSize,
                                           blobSize);
        m_blobs[static_cast<size_t>(i)].bounds = slot;
    }

    // Position buttons on the right side, vertically centered
    const auto totalH = ButtonHeight * 2 + ButtonSpacing;
    const int buttonW = ButtonWidth * 2;
    const int buttonY = area.getCentreY() - totalH / 2;
    const int rightX = area.getRight() - 10;  // 10px from right edge
    m_undoButton.setBounds(rightX - buttonW, buttonY, buttonW, ButtonHeight);
    m_clearButton.setBounds(rightX - buttonW, buttonY + ButtonHeight + ButtonSpacing, buttonW, ButtonHeight);
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
    // Buttons moved to ColorPalette - this is now empty
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
      m_palette(resources, theme)
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);

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
    auto area = getLocalBounds().reduced(Layout::CanvasOuterMargin, Layout::CanvasVerticalMargin);

    auto paletteArea = area.removeFromBottom(Layout::PaletteHeight);

    // Canvas gets the remaining area
    auto canvasArea = area;

    // Make canvas a fixed square size and center it
    constexpr int CanvasSize = 400;
    auto centeredCanvas = canvasArea.withSizeKeepingCentre(CanvasSize, CanvasSize);
    m_pixelCanvas.setBounds(centeredCanvas);

    m_palette.setBounds(paletteArea);
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

    auto content = bounds.removeFromLeft(bounds.getWidth() - Layout::EditorContentLeftMargin);
    const auto pedalW = juce::jlimit(
        Layout::EditorPedalAreaMinWidth,
        Layout::EditorPedalAreaMaxWidth,
        content.getWidth() - 400) - Layout::EditorPedalAreaExtraDeduction;
    auto pedalArea = content.removeFromRight(pedalW);

    auto gridArea = pedalArea.withTrimmedLeft(Layout::EditorGridWidthDeduction / 2)
                                .withTrimmedRight(Layout::EditorGridWidthDeduction / 2)
                                .withTrimmedBottom(Layout::EditorGridBottomTrim);

    m_woodGrainBackground.setBounds(bounds);
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
