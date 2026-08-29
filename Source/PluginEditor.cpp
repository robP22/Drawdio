#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "GridLayout.h"
#include "UI/EditorLayout.h"

#include <array>
#include <limits>
#include <vector>

namespace
{
juce::File getPresetsDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("Drawdio")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}
}

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

    m_bottomBar.onPresetSave = [this]() { savePreset(); };
    m_bottomBar.onPresetLoad = [this]() { loadPreset(); };
    m_bottomBar.onPresetImport = [this]() { importImage(); };

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
    m_palette.setOnReboundMode([this](bool on) { m_pixelCanvas.setReboundModeEnabled(on); });
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
                for (int k = 0; k < KnobsPerPedal; ++k)
                    pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * KnobsPerPedal + k)]);
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

    int paletteH = juce::roundToInt(canvasArea.getHeight() * GridLayout::PaletteHeightRatio);

    auto paletteBounds = canvasArea.withTrimmedTop(canvasArea.getHeight() - paletteH);
    auto pxCanvasBounds = canvasArea.withTrimmedBottom(paletteH);

    const int squareSize = pxCanvasBounds.getHeight();
    const auto pixelCanvasBounds = pxCanvasBounds.withSizeKeepingCentre(squareSize, squareSize);
    m_pixelCanvas.setBounds(pixelCanvasBounds);
    m_palette.setBounds(paletteBounds);
    m_palette.setCanvasCenterY(pixelCanvasBounds.getCentreY());
    m_palette.setImageCenterX(pixelCanvasBounds.getCentreX() - paletteBounds.getX());
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
    m_bottomBar.tick();
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
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Save Drawdio Preset",
        getPresetsDir().getChildFile("Untitled.drawdio"),
        "*.drawdio");
    m_fileChooser->launchAsync(juce::FileBrowserComponent::saveMode,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension(".drawdio");

            juce::MemoryBlock state;
            audioProcessor.getStateInformation(state);

            if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
            {
                stream->write(state.getData(), state.getSize());
                stream->flush();
            }
        });
}

void DrawdioProcessorEditor::loadPreset()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Load Drawdio Preset",
        getPresetsDir(),
        "*");
    m_fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            juce::MemoryBlock state;
            if (!file.loadFileAsData(state))
                return;

            audioProcessor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

            m_pixelCanvas.setGridData(audioProcessor.getGridData());
            m_syncController.setAutoEnvelopeDirty();
            m_syncController.clearRoutingCache();

            auto knobVals = audioProcessor.getKnobValues();
            for (int s = 0; s < PedalSlotCount; ++s)
                if (auto* pedal = m_pedalboardGrid.getPedal(s))
                    for (int k = 0; k < KnobsPerPedal; ++k)
                        pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * KnobsPerPedal + k)]);

            m_pedalboardGrid.syncPedals();
            m_pedalboardGrid.repaint();
        });
}

namespace
{
struct PaletteEntry { uint8_t value; juce::Colour colour; };

const std::array<PaletteEntry, 12>& drawdioPalette()
{
    static const std::array<PaletteEntry, 12> palette {{
        { 1, juce::Colour(0xFF2F73D8) },  // Blue
        { 2, juce::Colour(0xFF2BBE65) },  // Green
        { 3, juce::Colour(0xFFE54235) },  // Red
        { 4, juce::Colour(0xFFE8E5DC) },  // White
        { 5, juce::Colour(0xFF121212) },  // Black
        { 6, juce::Colour(0xFFFFD700) },  // Yellow
        { 7, juce::Colour(0xFF8B4513) },  // Brown
        { 8, juce::Colour(0xFF800080) },  // Purple
        { 9, juce::Colour(0xFF808080) },  // Grey
        { 10, juce::Colour(0xFFFF69B4) }, // Pink
        { 11, juce::Colour(0xFFE67E22) }, // Orange
        { 12, juce::Colour(0xFF8E44AD) }  // Violet
    }};
    return palette;
}

const PaletteEntry& findNearestPaletteEntry(float r, float g, float b)
{
    const auto& palette = drawdioPalette();
    float bestDist = std::numeric_limits<float>::max();
    const PaletteEntry* best = &palette[0];
    for (const auto& entry : palette)
    {
        const float dr = r - static_cast<float>(entry.colour.getRed());
        const float dg = g - static_cast<float>(entry.colour.getGreen());
        const float db = b - static_cast<float>(entry.colour.getBlue());
        const float dist = 2.0f * dr * dr + 4.0f * dg * dg + 3.0f * db * db;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &entry;
        }
    }
    return *best;
}

void ditherImageToGrid(const juce::Image& source, std::array<uint8_t, TotalCells>& grid)
{
    std::vector<float> buf(static_cast<size_t>(TotalCells) * 3, 0.0f);
    std::vector<uint8_t> opaque(static_cast<size_t>(TotalCells), 0);

    for (int y = 0; y < GridSize; ++y)
        for (int x = 0; x < GridSize; ++x)
        {
            const auto c = source.getPixelAt(x, y);
            if (c.getAlpha() < 128) continue;
            const size_t cellIdx = static_cast<size_t>(y) * GridSize + x;
            auto* cell = &buf[cellIdx * 3];
            cell[0] = static_cast<float>(c.getRed());
            cell[1] = static_cast<float>(c.getGreen());
            cell[2] = static_cast<float>(c.getBlue());
            opaque[cellIdx] = 1;
        }

    auto addError = [&buf](int x, int y, float er, float eg, float eb, float factor)
    {
        if (x < 0 || x >= GridSize || y < 0 || y >= GridSize) return;
        auto* cell = &buf[(static_cast<size_t>(y) * GridSize + x) * 3];
        cell[0] += er * factor;
        cell[1] += eg * factor;
        cell[2] += eb * factor;
    };

    constexpr float kForward = 7.0f / 16.0f;
    constexpr float kBackDown = 3.0f / 16.0f;
    constexpr float kDown = 5.0f / 16.0f;
    constexpr float kAheadDown = 1.0f / 16.0f;

    for (int y = 0; y < GridSize; ++y)
    {
        const bool leftToRight = (y % 2 == 0);
        for (int i = 0; i < GridSize; ++i)
        {
            const int x = leftToRight ? i : (GridSize - 1 - i);
            const int dx = leftToRight ? 1 : -1;
            const size_t cellIdx = static_cast<size_t>(y) * GridSize + x;

            if (!opaque[cellIdx])
            {
                grid[cellIdx] = 0;
                continue;
            }

            auto* cell = &buf[cellIdx * 3];
            const float r = juce::jlimit(0.0f, 255.0f, cell[0]);
            const float g = juce::jlimit(0.0f, 255.0f, cell[1]);
            const float b = juce::jlimit(0.0f, 255.0f, cell[2]);

            const auto& entry = findNearestPaletteEntry(r, g, b);
            grid[cellIdx] = entry.value;

            const float er = r - static_cast<float>(entry.colour.getRed());
            const float eg = g - static_cast<float>(entry.colour.getGreen());
            const float eb = b - static_cast<float>(entry.colour.getBlue());

            addError(x + dx, y,     er, eg, eb, kForward);
            addError(x - dx, y + 1, er, eg, eb, kBackDown);
            addError(x,      y + 1, er, eg, eb, kDown);
            addError(x + dx, y + 1, er, eg, eb, kAheadDown);
        }
    }
}
}

void DrawdioProcessorEditor::importImage()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Import Image to Canvas",
        getPresetsDir(),
        "*");
    m_fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            auto image = juce::ImageFileFormat::loadFrom(file);
            if (!image.isValid())
                return;

            auto scaled = image.rescaled(GridSize, GridSize, juce::Graphics::highResamplingQuality);

            std::array<uint8_t, TotalCells> grid{};
            ditherImageToGrid(scaled, grid);

            audioProcessor.submitCanvasSnapshot(grid);
            m_pixelCanvas.setGridData(grid);
            m_syncController.setAutoEnvelopeDirty();
        });
}
