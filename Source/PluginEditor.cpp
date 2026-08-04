#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "GridLayout.h"
#include "UI/EditorLayout.h"
#include "UI/PresetFileController.h"

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(p),
      audioProcessor(p),
      m_theme(m_themeImpl),
      m_woodGrainBackground(m_resourceManager),
      m_pedalboardBackground(m_resourceManager),
      m_pixelCanvas(m_resourceManager, m_theme),
      m_palette(m_resourceManager, m_theme),
      m_pedalboardGrid(p, m_resourceManager, m_theme, m_routingManager),
      m_bottomBar(p, m_resourceManager),
      m_syncController(p, m_pedalboardGrid, m_bottomBar,
                       m_automationPlayer, m_automationCompiler, m_pixelCanvas)
{
    addAndMakeVisible(m_woodGrainBackground);
    addAndMakeVisible(m_pedalboardBackground);
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);
    addAndMakeVisible(m_pedalboardGrid);
    addAndMakeVisible(m_bottomBar);

    m_bottomBar.onManualModeToggled = [this](bool manual) {
        if (manual)
            enterManualMode();
        else
            exitManualMode();
    };

    m_bottomBar.getAutomationDisplay().onBarCountChanged = [this](int bars) {
        m_automationPlayer.setBarCount(bars);
    };

    m_bottomBar.getAutomationDisplay().onSectionChanged = [this](int start) {
        audioProcessor.setSectionStart(start);
        m_automationPlayer.setSectionStartBar(start);
    };

    m_palette.setOnColorSelected([this](uint8_t color) {
        m_pixelCanvas.setCurrentColor(static_cast<PixelCanvasComponent::PixelColor>(color));
    });
    m_palette.setOnUndo([this]() { m_pixelCanvas.undo(); });
    m_palette.setOnRedo([this]() { m_pixelCanvas.redo(); });
    m_palette.setOnClear([this]() {
        audioProcessor.scheduleReset();
        audioProcessor.clearParamOffsets();
        m_pixelCanvas.clearCanvas();
    });
    m_palette.setOnFill([this](bool active) { m_pixelCanvas.setFillMode(active); });
    m_palette.setOnBrushSize([this](float radius) { m_pixelCanvas.setBrushRadius(radius); });
    m_palette.setOnPartyMode([this](bool on) { m_pixelCanvas.setPartyModeEnabled(on); });
    m_palette.setOnEraser([this](bool on) {
        if (on)
            m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Transparent);
    });

    m_pixelCanvas.setOnColorChanged([this](PixelCanvasComponent::PixelColor color) {
        m_palette.setSelectedColor(static_cast<uint8_t>(color));
    });
    m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Red);

    m_pixelCanvas.setGridData(audioProcessor.getGridData());

    {
        auto knobVals = audioProcessor.getKnobValues();
        for (int s = 0; s < PedalSlotCount; ++s)
            if (auto* pedal = m_pedalboardGrid.getPedal(s))
                for (int k = 0; k < 4; ++k)
                    pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * 4 + k)]);
    }

    {
        int bars = audioProcessor.getBarCount();
        m_bottomBar.getAutomationDisplay().setBarCount(bars);
        m_bottomBar.updateBarsButton(bars);
        m_automationPlayer.setBarCount(bars);
    }

    {
        int start = audioProcessor.getSectionStart();
        m_bottomBar.getAutomationDisplay().setSectionStart(start);
        m_automationPlayer.setSectionStartBar(start);
    }

    {
        bool manual = audioProcessor.isManualMode();
        m_bottomBar.updateManualButton(manual);
    }

    m_pixelCanvas.setOnPenDown([this]()
    {
        audioProcessor.notifyPenDown();
    });
    m_pixelCanvas.setOnPenUp([this]()
    {
        audioProcessor.notifyPenUp();
    });
    m_pixelCanvas.setOnCanvasSnapshot([this](const auto&)
    {
        triggerRecompile();
    });


    setSize(GridLayout::DesignResolution::Width, GridLayout::DesignResolution::Height);
    startTimerHz(20);
    juce::MessageManager::callAsync([self = juce::Component::SafePointer<DrawdioProcessorEditor>(this)]()
    {
        if (self != nullptr)
            self->m_syncController.tick();
    });
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    audioProcessor.storeUndoData(m_pixelCanvas.captureUndoData());
    stopTimer();
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void DrawdioProcessorEditor::resized()
{
    auto fullWindow = getLocalBounds();
    const int bottomBarH = juce::roundToInt(fullWindow.getHeight() * GridLayout::BottomBar::HeightRatio);
    auto topArea = fullWindow.removeFromTop(fullWindow.getHeight() - bottomBarH);
    m_bottomBar.setBounds(fullWindow);
    m_woodGrainBackground.setBounds(topArea);

    const int pedalW  = juce::roundToInt(topArea.getWidth() * EditorLayout::PedalboardWidthRatio);
    const int canvasW = topArea.getWidth() - pedalW;

    const auto pedalboardArea = topArea.withTrimmedLeft(canvasW);
    const auto canvasArea     = topArea.withTrimmedRight(pedalW);

    m_pedalboardBackground.setBounds(pedalboardArea);
    m_pedalboardGrid.setBounds(pedalboardArea);

    const auto& pedalImg = m_resourceManager.getImage(ResourceManager::ImageId::PedalboardSprite);
    const auto& paletteImg = m_resourceManager.getImage(ResourceManager::ImageId::ColorPaletteBody);
    int pedalH = pedalboardArea.getHeight();
    float pedalTopR = EditorLayout::topOpaqueRatio(pedalImg);
    float pedalBotR = EditorLayout::bottomOpaqueRatio(pedalImg);
    float paletteTopR = EditorLayout::topOpaqueRatio(paletteImg);
    float paletteBotR = EditorLayout::bottomOpaqueRatio(paletteImg);
    int paletteH = juce::roundToInt(canvasArea.getHeight() * GridLayout::PaletteHeightRatio);
    int canvasTopPx = juce::roundToInt(pedalH * pedalTopR);
    int paletteShiftPx = juce::roundToInt(pedalH * pedalBotR) - juce::roundToInt(paletteH * paletteBotR);
    int paletteCenterPx = juce::roundToInt(0.5f * paletteH * (paletteTopR - paletteBotR));

    auto paletteBounds = canvasArea.withTrimmedTop(canvasArea.getHeight() - paletteH);
    auto pxCanvasBounds = canvasArea.withTrimmedBottom(paletteH);

    const int squareSize = pxCanvasBounds.getHeight();
    const auto pixelCanvasBounds = pxCanvasBounds.withSizeKeepingCentre(squareSize, squareSize);
    m_pixelCanvas.setBounds(pixelCanvasBounds);
    m_pixelCanvas.setCanvasTopOffset(canvasTopPx);
    m_palette.setBounds(paletteBounds);
    m_palette.setImageBottomShift(static_cast<float>(paletteShiftPx));
    m_palette.setContentCenterOffset(static_cast<float>(paletteCenterPx));

    const float canvasScale = GridLayout::CanvasScaleRatio;
    const float scaledCanvasW = pixelCanvasBounds.getWidth() * canvasScale;
    const float canvasCX = pixelCanvasBounds.getX()
        + (pixelCanvasBounds.getWidth() - scaledCanvasW) * 0.5f
        + pixelCanvasBounds.getWidth() * GridLayout::CanvasCenterXShiftRatio
        + scaledCanvasW * 0.5f;
    m_palette.setImageCenterX(canvasCX - paletteBounds.getX());
}

void DrawdioProcessorEditor::triggerRecompile()
{
    m_syncController.setAutoEnvelopeDirty();
    audioProcessor.submitCanvasSnapshot(m_pixelCanvas.getGridData());
}

void DrawdioProcessorEditor::timerCallback()
{
    const bool neededBefore = m_syncController.needsRepaint();
    m_syncController.tick();
    if (m_syncController.needsRepaint() || neededBefore)
    {
        m_syncController.clearRepaintFlag();
        repaint();
    }
    int w = m_pedalboardGrid.getWidth();
    if (w > 0 && m_lastPedalboardWidth == 0)
        m_pedalboardGrid.rebuildCableCache();
    m_lastPedalboardWidth = w;

}

void DrawdioProcessorEditor::clearManualState()
{
    m_routingManager.clearManualRouting();
    m_pedalboardGrid.clearEdges();
    m_pedalboardGrid.clearInputOutputCables();
    m_pedalboardGrid.rebuildCableCache();
}

void DrawdioProcessorEditor::enterManualMode()
{
    clearManualState();
    m_syncController.clearRoutingCache();
    audioProcessor.setManualRouting({});
}

void DrawdioProcessorEditor::exitManualMode()
{
    clearManualState();
    m_syncController.clearRoutingCache();
    if (auto* config = audioProcessor.getCurrentConfig())
        m_pedalboardGrid.updateRouting(config->routingSlotOrder);
}

void DrawdioProcessorEditor::savePreset()
{
    PresetFileController::savePreset([this]() {
        return audioProcessor.createPresetState();
    });
}

void DrawdioProcessorEditor::loadPreset()
{
    PresetFileController::loadPreset(
        [this](const void* data, int size) {
            return audioProcessor.applyPresetState(data, size);
        },
        [this]() {
            m_pixelCanvas.setGridData(audioProcessor.getGridData());
            triggerRecompile();
            auto knobVals = audioProcessor.getKnobValues();
            for (int s = 0; s < PedalSlotCount; ++s)
                if (auto* pedal = m_pedalboardGrid.getPedal(s))
                    for (int k = 0; k < 4; ++k)
                        pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * 4 + k)]);
        });
}
