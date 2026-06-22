#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "GridLayout.h"

#include <cmath>

class WoodGrainBackground : public juce::Component
{
public:
    WoodGrainBackground(const ResourceManager& resources, const IThemeProvider& theme)
        : m_resources(resources), m_theme(theme) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto& woodTexture = m_resources.getTexture(ResourceManager::TextureId::WorkspaceWood);
        if (woodTexture.isValid())
            g.drawImage(woodTexture, bounds, juce::RectanglePlacement::stretchToFit);
    }
private:
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
};

class PedalboardBackground : public juce::Component
{
public:
    PedalboardBackground(const ResourceManager& resources, const IThemeProvider& theme)
        : m_resources(resources), m_theme(theme) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto& texture = m_resources.getTexture(ResourceManager::TextureId::PedalboardSprite);
        if (texture.isValid())
            g.drawImage(texture, bounds.getX(), bounds.getY(),
                       bounds.getWidth(), bounds.getHeight(),
                       0, 0, texture.getWidth(), texture.getHeight());
    }
private:
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
};

// Layout constants namespace for consistent spacing across all components
namespace
{
constexpr float Layout_PedalboardWidthRatio = 0.55f;

float topOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() == 255)
                return static_cast<float>(y) / static_cast<float>(img.getHeight());
    return 0.0f;
}

float bottomOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    for (int y = img.getHeight() - 1; y >= 0; --y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() == 255)
                return static_cast<float>(img.getHeight() - 1 - y) / static_cast<float>(img.getHeight());
    return 0.0f;
}
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(p),
      audioProcessor(p),
      m_woodGrainBackground(std::make_unique<WoodGrainBackground>(m_resourceManager, m_theme)),
      m_pedalboardBackground(std::make_unique<PedalboardBackground>(m_resourceManager, m_theme)),
      m_pixelCanvas(m_resourceManager, m_theme),
      m_palette(m_resourceManager, m_theme),
      m_pedalboardGrid(p, m_resourceManager, m_theme, m_routingManager),
      m_bottomBar(this, p, m_resourceManager)
{
    addAndMakeVisible(m_woodGrainBackground.get());
    addAndMakeVisible(m_pedalboardBackground.get());
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);
    addAndMakeVisible(m_pedalboardGrid);
    addAndMakeVisible(m_bottomBar);

    m_bottomBar.getAutomationDisplay().onBarCountChanged = [this](int bars) {
        m_automationPlayer.setBarCount(bars);
    };

    m_palette.setOnColorSelected([this](uint8_t color) {
        m_pixelCanvas.setCurrentColor(static_cast<PixelCanvasComponent::PixelColor>(color));
    });
    m_palette.setOnUndo([this]() { m_pixelCanvas.undo(); });
    m_palette.setOnRedo([this]() { m_pixelCanvas.redo(); });
    m_palette.setOnClear([this]() {
        audioProcessor.getDSPProcessor().scheduleReset();
        audioProcessor.getDSPProcessor().clearParamOffsets();
        m_pixelCanvas.clearCanvas();
    });
    m_palette.setOnFill([this](bool active) { m_pixelCanvas.setFillMode(active); });
    m_palette.setOnBrushSize([this](float radius) { m_pixelCanvas.setBrushRadius(radius); });
    m_palette.setOnPartyMode([this](bool on) { m_pixelCanvas.setPartyModeEnabled(on); });
    m_palette.setOnEraser([this](bool on) {
        if (on)
            m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Transparent);
    });

    m_pixelCanvas.setOnFillModeChanged([this](bool active) { m_palette.setFillButtonState(active); });
    m_pixelCanvas.setOnColorChanged([this](PixelCanvasComponent::PixelColor color) {
        m_palette.setSelectedColor(static_cast<uint8_t>(color));
    });
    m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Red);

    m_pixelCanvas.setGridData(audioProcessor.getGridData());

    // Immediately apply restored knob values to pedals
    {
        auto knobVals = audioProcessor.getKnobValues();
        for (int s = 0; s < PedalSlotCount; ++s)
            if (auto* pedal = m_pedalboardGrid.getPedal(s))
                for (int k = 0; k < 4; ++k)
                    pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * 4 + k)]);
    }

    // Restore bar count from processor
    {
        int bars = audioProcessor.getBarCount();
        m_bottomBar.getAutomationDisplay().setBarCount(bars);
        m_bottomBar.updateBarsButton(bars);
        m_automationPlayer.setBarCount(bars);
    }

    m_pixelCanvas.setOnPenDown([this]()
    {
        audioProcessor.getPenDebouncer().penDown();
    });
    m_pixelCanvas.setOnPenUp([this]()
    {
        audioProcessor.getPenDebouncer().penUp();
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
            self->checkForUpdates();
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
    m_woodGrainBackground->setBounds(topArea);

    const int pedalW  = juce::roundToInt(topArea.getWidth() * Layout_PedalboardWidthRatio);
    const int canvasW = topArea.getWidth() - pedalW;

    const auto pedalboardArea = topArea.withTrimmedLeft(canvasW);
    const auto canvasArea     = topArea.withTrimmedRight(pedalW);

    m_pedalboardBackground->setBounds(pedalboardArea);
    m_pedalboardGrid.setBounds(pedalboardArea);

    // Compute vertical alignment offsets from sprite edges
    const auto& pedalImg = m_resourceManager.getImage(ResourceManager::ImageId::PedalboardSprite);
    const auto& paletteImg = m_resourceManager.getImage(ResourceManager::ImageId::ColorPaletteBody);
    int pedalH = pedalboardArea.getHeight();
    float pedalTopR = topOpaqueRatio(pedalImg);
    float pedalBotR = bottomOpaqueRatio(pedalImg);
    float paletteTopR = topOpaqueRatio(paletteImg);
    float paletteBotR = bottomOpaqueRatio(paletteImg);
    int paletteH = juce::roundToInt(canvasArea.getHeight() * GridLayout::PaletteHeightRatio);
    int canvasTopPx = juce::roundToInt(pedalH * pedalTopR);
    int paletteShiftPx = juce::roundToInt(pedalH * pedalBotR) - juce::roundToInt(paletteH * paletteBotR);
    int paletteCenterPx = juce::roundToInt(0.5f * paletteH * (paletteTopR - paletteBotR));

    // Position pixel canvas and palette
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
    const auto& grid = m_pixelCanvas.getGridData();
    audioProcessor.getMessageQueue().pushSnapshot(grid.data());
    audioProcessor.setGridData(grid);
    audioProcessor.getCompilerThread().notify();
}

void DrawdioProcessorEditor::refreshRoutingFromConfig()
{
    auto config = audioProcessor.getDSPProcessor().getCurrentConfig();
    if (config)
    {
        if (config->routingSlotOrder != m_lastRoutingOrder)
        {
            m_lastRoutingOrder = config->routingSlotOrder;
            m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
            m_needsRepaint = true;
        }
    }
    else if (!m_lastRoutingOrder.empty())
    {
        m_lastRoutingOrder.clear();
        m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
        m_needsRepaint = true;
    }
}

void DrawdioProcessorEditor::syncCompiledKnobs(bool& needsRepaint)
{
    uint32_t revBefore = audioProcessor.getConfigRevision();
    audioProcessor.consumeCompiledResultIfAvailable();
    if (audioProcessor.getConfigRevision() != revBefore)
    {
        needsRepaint = true;
        auto& syncData = audioProcessor.getLastConfigSync();
        for (auto& param : syncData.parameters)
        {
            const int chainPos = static_cast<int>(param.targetDspNodeRegister);
            if (chainPos >= 0 && chainPos < static_cast<int>(syncData.routingSlotOrder.size()))
            {
                const int slotIdx = syncData.routingSlotOrder[static_cast<size_t>(chainPos)];
                const int token = static_cast<int>(param.parameterToken);
                if (audioProcessor.getDSPProcessor().isParamOverridden(slotIdx, token))
                {
                    float display = audioProcessor.getDSPProcessor().getKnobDisplayValue(
                        slotIdx, token, param.currentValue);
                    if (auto* pedal = m_pedalboardGrid.getPedal(slotIdx))
                        pedal->setKnobValue(token, display);
                    audioProcessor.getDSPProcessor().storeParameterValue(slotIdx, token, display);
                }
                else if (auto* pedal = m_pedalboardGrid.getPedal(slotIdx))
                {
                    pedal->setKnobValue(token, param.currentValue);
                    audioProcessor.getDSPProcessor().storeParameterValue(slotIdx, token, param.currentValue);
                }
            }
        }
    }
}

void DrawdioProcessorEditor::syncAutomation()
{
    std::vector<DspModuleType> slots(PedalSlotCount);
    for (int i = 0; i < PedalSlotCount; ++i)
        slots[i] = audioProcessor.getPedalSlot(i);
    auto envelope = m_automationCompiler.compile(
        m_pixelCanvas.getGridData(), slots);
    m_automationPlayer.setEnvelope(envelope);
    m_bottomBar.getAutomationDisplay().setEnvelope(envelope);

    float bpm = audioProcessor.getPlayHeadBpm();
    double ppq = audioProcessor.getPlayHeadPpq();
    bool playing = audioProcessor.isPlayHeadPlaying();
    if (!playing)
        audioProcessor.getDSPProcessor().resetPedalPeaks();
    m_automationPlayer.tick(ppq, bpm, playing);
    audioProcessor.getDSPProcessor().setAutomationValue(m_automationPlayer.getValue());
    m_bottomBar.getAutomationDisplay().setPlayheadTime(m_automationPlayer.getPlayheadTime());
    syncKnobAutomation();
}

void DrawdioProcessorEditor::syncKnobAutomation()
{
    float autoVal = m_automationPlayer.getValue();
    auto knobVals = audioProcessor.getKnobValues();
    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        auto* pedal = m_pedalboardGrid.getPedal(slot);
        if (!pedal) continue;
        for (int k = 0; k < 4; ++k)
        {
            if (!audioProcessor.getDSPProcessor().isKnobLinked(slot, k))
                continue;
            size_t idx = static_cast<size_t>(slot * 4 + k);
            float strength = audioProcessor.getDSPProcessor().getKnobLinkStrength(slot, k);
            float display = std::max(0.0f, std::min(1.0f, knobVals[idx] + autoVal * strength));
            pedal->setKnobValue(k, display);
        }
    }
}

void DrawdioProcessorEditor::checkForUpdates()
{
    bool needsRepaint = false;

    audioProcessor.getDSPProcessor().drainReleaseQueue();
    audioProcessor.getDSPProcessor().tryApplyDeferredConfig();

    syncCompiledKnobs(needsRepaint);
    syncAutomation();

    if (!audioProcessor.consumeUINotification())
    {
        refreshRoutingFromConfig();
        if (needsRepaint || m_needsRepaint)
        {
            m_needsRepaint = false;
            repaint();
        }
        return;
    }

    needsRepaint = true;
    m_seenConfigRevision = audioProcessor.getConfigRevision();

    m_pedalboardGrid.syncPedals();
    m_bottomBar.syncPedalNames();
    refreshRoutingFromConfig();

    if (needsRepaint || m_needsRepaint)
    {
        m_needsRepaint = false;
        repaint();
    }
}

void DrawdioProcessorEditor::timerCallback()
{
    checkForUpdates();
}

void DrawdioProcessorEditor::showHamburgerMenu()
{
    juce::PopupMenu menu;
    menu.addItem("Settings...", false, false, nullptr);
    menu.showMenuAsync(juce::PopupMenu::Options(), [](int) {});
}

void DrawdioProcessorEditor::savePreset()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.drawdio");
    chooser->launchAsync(2, [this, chooser](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == juce::File{}) return;
        juce::File file = result;
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension(".drawdio");
        juce::MemoryBlock state = audioProcessor.createPresetState();
        juce::FileOutputStream stream(file);
        if (stream.openedOk())
            stream.write(state.getData(), state.getSize());
    });
}

void DrawdioProcessorEditor::loadPreset()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.drawdio");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == juce::File{}) return;
        juce::MemoryBlock data;
        if (result.loadFileAsData(data))
        {
            if (audioProcessor.applyPresetState(data.getData(), static_cast<int>(data.getSize())))
            {
    m_pixelCanvas.setGridData(audioProcessor.getGridData());
    m_pixelCanvas.applyUndoData(audioProcessor.getUndoData());
                triggerRecompile();
                auto knobVals = audioProcessor.getKnobValues();
                for (int s = 0; s < PedalSlotCount; ++s)
                    if (auto* pedal = m_pedalboardGrid.getPedal(s))
                        for (int k = 0; k < 4; ++k)
                            pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * 4 + k)]);
            }
        }
    });
}
