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
constexpr float BlobSpacing = 10.5f;
constexpr float BlobMaxSize = 72.0f;
constexpr float BlobPadding = 20.0f;
constexpr int ButtonWidth = 38;
constexpr int ButtonHeight = 38;
constexpr int ButtonSpacing = 6;
constexpr int ButtonAreaHeight = 80;
}

// Layout constants namespace for consistent spacing across all components
namespace Layout
{
    // ---- Proportional split ratios (Layer 1 surfaces) ----
    // PedalboardWidthRatio: fraction of the full editor width given to the pedalboard region.
    // CanvasWidthRatio is the complement (1.0 - PedalboardWidthRatio).
    constexpr float PedalboardWidthRatio = 0.55f;

    // PaletteHeightRatio: fraction of the canvas module height reserved for the colour palette.
    constexpr float PaletteHeightRatio = 0.26f;

    // ---- Fixed structural margins (Layer 1 → Layer 2 insets) ----
    // These are intentional insets that detach child content from the raw structural
    // surface boundary.  They are NOT compensatory corrections.
    constexpr int CanvasOuterMargin   = 22;   // horizontal inset inside CanvasModule
    constexpr int CanvasVerticalMargin = 20;  // vertical inset inside CanvasModule

    // ---- ColorPalette internal margins ----
    constexpr int PaletteLeftMargin   = 30;
    constexpr int PaletteRightMargin  = 30;
    constexpr int PaletteTopMargin    = 20;
    constexpr int PaletteBottomMargin = 35;
    constexpr float PaletteBlobShift  = 10.0f;
    constexpr int PaletteButtonRightPadding = 8;
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
        g.drawImage(woodTexture, bounds, juce::RectanglePlacement::stretchToFit);
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
    
    // DEBUG: Draw border
    //g.setColour(juce::Colours::blue);
    //g.drawRect(bounds, 2);
    
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
    auto bounds = getLocalBounds().toFloat();
    const auto& texture = m_resources.getTexture(ResourceManager::TextureId::ColorPaletteBody);

    if (texture.isValid())
        g.drawImage(texture, bounds.getX(), bounds.getY(),
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, texture.getWidth(), texture.getHeight());

    //g.setColour(juce::Colours::red);
    //g.drawRect(bounds, 2);

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const auto& blob = m_blobs[static_cast<size_t>(i)];
        const bool isSelected = blob.color == m_selectedColor;
        const bool isHovered  = i == m_hoveredBlob;

        auto colour = PixelCanvasComponent::colourForPixel(blob.color);
        if (isHovered) colour = colour.brighter(0.12f);

        const auto drawBounds = isSelected ? blob.bounds.expanded(2.0f) : blob.bounds;

        if (isSelected)
        {
            for (int g2 = 4; g2 >= 1; --g2)
            {
                g.setColour(colour.withAlpha(0.04f));
                g.fillEllipse(drawBounds.expanded(static_cast<float>(g2) * 2.0f));
            }
        }

        juce::ColourGradient gradient(colour.brighter(0.04f),
                                      drawBounds.getCentreX(), drawBounds.getCentreY(),
                                      colour.darker(0.18f),
                                      drawBounds.getRight(), drawBounds.getBottom(),
                                      true);
        gradient.addColour(0.6, colour);  // flat zone from centre out to 60% radius
        g.setGradientFill(gradient);
        g.fillEllipse(drawBounds);

        // Outer rim darkening — 1px at perimeter, darker blob colour, very low contrast
        g.setColour(colour.darker(0.30f).withAlpha(0.22f));
        g.drawEllipse(drawBounds, 1.0f);

        // Inner highlight ring — slightly brighter, low opacity, 1.5px inside perimeter
        g.setColour(colour.brighter(0.25f).withAlpha(0.18f));
        g.drawEllipse(drawBounds.reduced(1.5f), 1.0f);
    }
}

void ColorPalette::resized()
{
    // Use withTrimmed* so every margin is applied to the same starting rectangle,
    // not chained on the strip returned by the previous removeFrom* call.
    auto area = getLocalBounds()
        .withTrimmedLeft(Layout::PaletteLeftMargin)
        .withTrimmedRight(Layout::PaletteRightMargin)
        .withTrimmedTop(Layout::PaletteTopMargin)
        .withTrimmedBottom(Layout::PaletteBottomMargin);

    const auto blobSize = juce::jmin(area.getHeight() - 10.0f, BlobMaxSize);
    float startX = area.getX() + blobSize * 0.25f + 1.0f;
    float blobY = area.getCentreY() - blobSize / 2.0f - 1.0f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(startX + static_cast<float>(i) * (blobSize + BlobSpacing), blobY, blobSize, blobSize);
        m_blobs[static_cast<size_t>(i)].bounds = slot;
    }

    const auto totalH = ButtonHeight * 2 + ButtonSpacing;
    const int buttonW = ButtonWidth * 2;
    const int buttonY = area.getCentreY() - totalH / 2 + 2 - 1;
    const int rightX = area.getRight() - Layout::PaletteButtonRightPadding - buttonW - 5;
    m_undoButton.setBounds(rightX, buttonY, buttonW, ButtonHeight);
    m_clearButton.setBounds(rightX, buttonY + ButtonHeight + ButtonSpacing, buttonW, ButtonHeight);
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

void CanvasModule::paint(juce::Graphics&)
{
    // COMMENTED OUT FOR DEBUG - canvas module rendering disabled
}

void CanvasModule::resized()
{
    auto full = getLocalBounds();
    auto area = full.reduced(Layout::CanvasOuterMargin, Layout::CanvasVerticalMargin);

    // Palette anchored to the bottom of the full (unreduced) bounds so its
    // bottom edge aligns with the window bottom.
    const int paletteH = juce::roundToInt(full.getHeight() * Layout::PaletteHeightRatio);
    auto paletteArea = full.removeFromBottom(paletteH);

    // Canvas component is a square (height-constrained), right-edge aligned
    // with the canvas module right edge (= pedalboard left boundary).
    auto canvasArea = full;
    canvasArea.setBottom(paletteArea.getY());
    const int squareSize = canvasArea.getHeight();
    const auto pixelCanvasBounds = canvasArea.withSizeKeepingCentre(squareSize, squareSize)
                                              .withRightX(canvasArea.getRight());
    m_pixelCanvas.setBounds(pixelCanvasBounds);
    m_palette.setBounds(paletteArea.withTrimmedLeft(pixelCanvasBounds.getX()).withTrimmedRight(3).withTrimmedTop(2));
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
    // DEBUG: Fill entire window with WHITE to identify black rectangle source
    g.fillAll(juce::Colours::white);
}

void DrawdioProcessorEditor::resized()
{
    // ── Layer 0: wood grain background fills the entire window ──────────────
    // No deductions. This is the root visual surface.
    const auto fullWindow = getLocalBounds();
    m_woodGrainBackground.setBounds(fullWindow);

    // ── Layer 1: major structural surfaces ──────────────────────────────────
    // Split the full window left/right using PedalboardWidthRatio.
    // The pedalboard owns the right portion; the canvas module owns the left.
    const int pedalW  = juce::roundToInt(fullWindow.getWidth() * Layout::PedalboardWidthRatio);
    const int canvasW = fullWindow.getWidth() - pedalW;

    const auto pedalboardArea = fullWindow.withTrimmedLeft(canvasW);  // right portion
    const auto canvasArea     = fullWindow.withTrimmedRight(pedalW);  // left portion

    m_pedalboardBackground.setBounds(pedalboardArea);
    m_canvasModule.setBounds(canvasArea);

    // ── Layer 1 → Layer 2: pedalboard grid ──────────────────────────────────
    // PedalboardGrid sits inside the pedalboard region. Its own resized()
    // applies GridSidePadding internally to position the pedal slots.
    m_pedalboardGrid.setBounds(pedalboardArea);
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
