#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Core/EditorDesignMetrics.h"
#include "UI/EditorLayout.h"
#include "Resources/FontManager.h"

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

juce::File getImagesDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userPicturesDirectory)
                   .getChildFile("Drawdio");
    if (!dir.isDirectory() && !dir.createDirectory().wasOk())
    {
        dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                  .getChildFile("Drawdio");
        dir.createDirectory();
    }
    else
    {
        dir.createDirectory();
    }
    return dir;
}
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(p),
      audioProcessor(p),
      m_processorBridge(p),
      m_scaledAssets(m_resourceManager),
      m_themeImpl(),
      m_theme(m_themeImpl),
      m_woodGrainBackground(m_scaledAssets),
      m_pedalboardBackground(m_scaledAssets),
      m_pixelCanvas(m_resourceManager, m_scaledAssets, m_theme),
      m_palette(m_resourceManager, m_scaledAssets, m_theme),
      m_pedalboardGrid(m_processorBridge.getUiSnapshot(), m_resourceManager, m_scaledAssets, m_theme,
                       m_routingManager,
                       PedalboardGrid::Actions{
                           [this](int slot, DspModuleType type) { m_processorBridge.setPedalSlot(slot, type); },
                           [this](int slot, int knob, float start, float value)
                           { m_processorBridge.setKnobParameter(slot, knob, start, value); },
                           [this](int slot, int knob, bool linked)
                           { m_processorBridge.setKnobLink(slot, knob, linked); },
                           [this](int slot, int knob, float rMin, float rMax)
                           { m_processorBridge.setKnobLinkRange(slot, knob, rMin, rMax); },
                           [this](const std::vector<uint8_t>& routing)
                           { m_processorBridge.setManualRouting(routing); }
                       }),
      m_bottomBar(m_scaledAssets,
                  BottomControlBar::Actions{
                      [this](float gain) { m_processorBridge.setInputGain(gain); },
                      [this](float gain) { m_processorBridge.setOutputGain(gain); },
                      [this](int slot, float gain) { m_processorBridge.setPedalGain(slot, gain); },
                      [this](int bars) { m_processorBridge.setBarCount(bars); },
                      [this](int section) { m_processorBridge.setSectionStart(section); }
                  }),
      m_pedalboardHeader(),
      m_syncController(m_processorBridge, m_pedalboardGrid, m_bottomBar, m_pedalboardHeader,
                       m_automationPlayer, m_automationCompiler, m_pixelCanvas, m_palette)
{
    FontManager::initialise();
    m_processorBridge.onStateChanged = [this]() { m_syncController.requestSync(); };
    addAndMakeVisible(m_woodGrainBackground);
    addAndMakeVisible(m_pedalboardBackground);
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);
    addAndMakeVisible(m_pedalboardGrid);
    addAndMakeVisible(m_pedalboardHeader);
    addAndMakeVisible(m_bottomBar);
    m_bottomBar.setViewState(m_processorBridge.getUiSnapshot());

    m_pedalboardHeader.onModeToggle = [this]() {
        bool next = !m_processorBridge.getUiSnapshot().manualMode;
        m_processorBridge.setManualMode(next);
        m_pedalboardHeader.updateModeButton(next);
        if (next)
            enterManualMode();
        else
            exitManualMode();
    };
    m_pedalboardHeader.onReset = [this]() { resetPedalboardState(); };

    m_bottomBar.onPresetSave = [this]() { savePreset(); };
    m_bottomBar.onPresetImport = [this]() { importPreset(); };
    m_bottomBar.onImageImport = [this]() { importImage(); };
    m_bottomBar.onImageExport = [this]() { exportImage(); };

    m_bottomBar.getAutomationDisplay().onBarCountChanged = [this](int bars) {
        m_automationPlayer.setBarCount(bars);
    };

    m_bottomBar.getAutomationDisplay().onSectionChanged = [this](int start) {
        m_processorBridge.setSectionStart(start);
        m_automationPlayer.setSectionStartBar(start);
    };

    m_bottomBar.getAutomationDisplay().onEnvelopeEdit = [this](int slice, float value) {
        if (!m_processorBridge.isManualMode())
            return;
        m_processorBridge.setManualEnvelopeSlice(slice, value);
        AutomationEnvelope env;
        for (int i = 0; i < EnvelopeSliceCount; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(EnvelopeSliceCount - 1);
            auto stored = m_processorBridge.getManualEnvelopeSlice(i);
            env.addPoint(t, stored);
        }
        m_automationPlayer.setEnvelope(env);
        m_bottomBar.getAutomationDisplay().setEnvelope(env);
    };

    m_palette.setOnColorSelected([this](uint8_t color) {
        m_pixelCanvas.setCurrentColor(static_cast<PixelCanvasComponent::PixelColor>(color));
        auto session = m_processorBridge.getEditorSessionState();
        session.selectedColour = color;
        m_processorBridge.setEditorSessionState(session);
    });
    m_palette.setOnUndo([this]() { m_pixelCanvas.undo(); });
    m_palette.setOnRedo([this]() { m_pixelCanvas.redo(); });
    m_palette.setOnClear([this]() {
        m_processorBridge.scheduleReset();
        m_processorBridge.clearParameterOffsets();
        m_pixelCanvas.clearCanvas();
    });
    m_palette.setOnFill([this](bool active) { m_pixelCanvas.setFillMode(active); });
    m_palette.setOnBrushSize([this](float radius) {
        m_pixelCanvas.setBrushRadius(radius);
        auto session = m_processorBridge.getEditorSessionState();
        session.brushSizeIndex = m_palette.getBrushSizeIndex();
        m_processorBridge.setEditorSessionState(session);
    });
    m_palette.setOnReboundMode([this](bool on) { m_pixelCanvas.setReboundModeEnabled(on); });
    m_palette.setOnEraser([this](bool on) {
        if (on)
            m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Transparent);
    });

    m_pixelCanvas.setOnColorChanged([this](PixelCanvasComponent::PixelColor color) {
        m_palette.setSelectedColor(static_cast<uint8_t>(color));
    });
    const auto initialSession = m_processorBridge.getEditorSessionState();
    m_palette.setSelectedColor(initialSession.selectedColour);
    m_pixelCanvas.setCurrentColor(static_cast<PixelCanvasComponent::PixelColor>(initialSession.selectedColour));
    m_palette.setBrushSizeIndex(initialSession.brushSizeIndex);
    m_pixelCanvas.setBrushRadius(m_palette.getBrushRadius());

    m_pixelCanvas.setGridData(m_processorBridge.getGridData(), false);
    const auto persistedUndo = m_processorBridge.getUndoData();
    if (!persistedUndo.empty())
        m_pixelCanvas.applyUndoData(persistedUndo);

    {
        auto knobVals = m_processorBridge.getKnobValues();
        for (int s = 0; s < PedalSlotCount; ++s)
            if (auto* pedal = m_pedalboardGrid.getPedal(s))
                for (int k = 0; k < KnobsPerPedal; ++k)
                    pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * KnobsPerPedal + k)]);
    }

    {
        int bars = m_processorBridge.getBarCount();
        m_bottomBar.getAutomationDisplay().setBarCount(bars);
        m_bottomBar.updateBarsButton(bars);
        m_automationPlayer.setBarCount(bars);
    }

    {
        int start = m_processorBridge.getSectionStart();
        m_bottomBar.getAutomationDisplay().setSectionStart(start);
        m_automationPlayer.setSectionStartBar(start);
    }

    {
        bool manual = m_processorBridge.isManualMode();
        m_pedalboardHeader.updateModeButton(manual);
    }

    m_pixelCanvas.setOnPenDown([this]()
    {
        m_processorBridge.notifyPenDown();
    });
    m_pixelCanvas.setOnPenUp([this]()
    {
        m_processorBridge.notifyPenUp();
    });
    m_pixelCanvas.setOnCanvasSnapshot([this](const auto&)
    {
        triggerRecompile();
    });


    setResizable(true, true);
    setResizeLimits(EditorDesignMetrics::DesignResolution::MinimumWidth,
                    EditorDesignMetrics::DesignResolution::MinimumHeight,
                    EditorDesignMetrics::DesignResolution::MaximumWidth,
                    EditorDesignMetrics::DesignResolution::MaximumHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(EditorDesignMetrics::DesignResolution::Width)
                                         / static_cast<double>(EditorDesignMetrics::DesignResolution::Height));

    setSize(EditorDesignMetrics::DesignResolution::Width, EditorDesignMetrics::DesignResolution::Height);
    startTimerHz(60);

    juce::MessageManager::callAsync([self = juce::Component::SafePointer<DrawdioProcessorEditor>(this)]()
    {
        if (self != nullptr)
            self->m_syncController.tick();
    });
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    stopTimer();
    m_processorBridge.onStateChanged = {};
    m_processorBridge.detach();
    m_processorBridge.storeUndoData(m_pixelCanvas.captureUndoData());
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void DrawdioProcessorEditor::resized()
{
    m_scaledAssets.setResizeActive(true);
    m_lastResizeTimeMs = juce::Time::getMillisecondCounterHiRes();
    const auto layout = EditorLayout::calculate(getLocalBounds());
    m_bottomBar.setBounds(layout.bottomBar);
    m_woodGrainBackground.setBounds(layout.topArea);

    m_pedalboardHeader.setBounds(layout.header);
    const float spriteTopY = layout.pedalboardArea.getHeight() * EditorLayout::topOpaqueRatio(
        m_resourceManager.getTexture(IResourceProvider::TextureId::PedalboardSprite));
    m_pedalboardHeader.setButtonCenterY(spriteTopY * 0.5f);

    m_pedalboardBackground.setBounds(layout.pedalboardArea);
    m_pedalboardGrid.setBounds(layout.pedalboardArea);
    m_pixelCanvas.setBounds(layout.pixelCanvas);
    m_palette.setBounds(layout.palette);
    m_palette.setImageCenterX(layout.pixelCanvas.getCentreX() - layout.palette.getX());
}

void DrawdioProcessorEditor::triggerRecompile()
{
    m_syncController.setAutoEnvelopeDirty();
    m_processorBridge.submitCanvasSnapshot(m_pixelCanvas.getGridData(), m_pixelCanvas.getDirtyRows());
    m_pixelCanvas.clearDirtyRows();
}

void DrawdioProcessorEditor::timerCallback()
{
    if (m_scaledAssets.isResizeActive()
        && juce::Time::getMillisecondCounterHiRes() - m_lastResizeTimeMs >= 100.0)
    {
        m_scaledAssets.setResizeActive(false);
        m_pixelCanvas.refreshAfterResize();
        m_pedalboardGrid.refreshAfterResize();
        m_palette.repaint();
        m_pedalboardHeader.repaint();
        m_bottomBar.repaint();
        m_pedalboardBackground.repaint();
        m_woodGrainBackground.repaint();
        repaint();
    }

    const bool active = m_processorBridge.isPlayHeadPlaying();
    const bool refreshNow = active || (++m_refreshTick >= 3);
    if (!refreshNow)
        return;
    m_refreshTick = 0;

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

void DrawdioProcessorEditor::enterManualMode()
{
    auto seed = m_processorBridge.getManualRouting();
    if (seed.empty())
        seed = m_processorBridge.getLastConfigSync().routingSlotOrder;
    if (!seed.empty())
        m_processorBridge.setManualRouting(seed);
    m_syncController.clearRoutingCache();
}

void DrawdioProcessorEditor::exitManualMode()
{
    m_processorBridge.clearParameterOffsets();
    m_syncController.clearRoutingCache();
}

void DrawdioProcessorEditor::resetPedalboardState()
{
    for (int slot = 0; slot < PedalSlotCount; ++slot)
        for (int k = 0; k < KnobsPerPedal; ++k)
            m_processorBridge.setKnobLink(slot, k, false);
    for (int slot = 0; slot < PedalSlotCount; ++slot)
        m_processorBridge.setPedalSlot(slot, DspModuleType::BYPASS);
    m_processorBridge.clearParameterOffsets();
    m_processorBridge.resetParameterDefaults();
    m_processorBridge.setManualRouting({});
    m_syncController.clearRoutingCache();
}

void DrawdioProcessorEditor::savePreset()
{
    juce::Component::SafePointer<DrawdioProcessorEditor> safeThis(this);
    m_presetChooser = std::make_unique<juce::FileChooser>(
        "Save Drawdio Preset",
        getPresetsDir().getChildFile("Untitled.drawdio"),
        "*.drawdio");
    m_presetChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                     | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            auto& editor = *safeThis;
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension(".drawdio");

            juce::MemoryBlock state;
            editor.m_processorBridge.getPresetInformation(state);

            if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
            {
                stream->write(state.getData(), state.getSize());
                stream->flush();
            }
        });
}

void DrawdioProcessorEditor::importPreset()
{
    juce::Component::SafePointer<DrawdioProcessorEditor> safeThis(this);
    m_presetChooser = std::make_unique<juce::FileChooser>(
        "Import Drawdio Preset",
        getPresetsDir(),
        "*.drawdio");
    m_presetChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            auto& editor = *safeThis;
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            juce::MemoryBlock state;
            if (!file.loadFileAsData(state))
                return;

            if (!editor.m_processorBridge.setPresetInformation(state.getData(), static_cast<int>(state.getSize())))
                return;

            editor.m_pixelCanvas.setGridData(editor.m_processorBridge.getGridData());
            editor.m_syncController.setAutoEnvelopeDirty();
            editor.m_syncController.clearRoutingCache();

            auto knobVals = editor.m_processorBridge.getKnobValues();
            for (int s = 0; s < PedalSlotCount; ++s)
                if (auto* pedal = editor.m_pedalboardGrid.getPedal(s))
                    for (int k = 0; k < KnobsPerPedal; ++k)
                        pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * KnobsPerPedal + k)]);

            editor.m_pedalboardGrid.syncPedals();
            editor.m_pedalboardGrid.repaint();
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
    juce::Component::SafePointer<DrawdioProcessorEditor> safeThis(this);
    m_imageChooser = std::make_unique<juce::FileChooser>(
        "Import Image to Canvas",
        getImagesDir(),
        "*.png;*.jpg;*.jpeg;*.bmp;*.gif");
    m_imageChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            auto& editor = *safeThis;
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            auto image = juce::ImageFileFormat::loadFrom(file);
            if (!image.isValid())
                return;

            auto scaled = image.rescaled(GridSize, GridSize, juce::Graphics::highResamplingQuality);

            auto grid = std::make_unique<std::array<uint8_t, TotalCells>>();
            ditherImageToGrid(scaled, *grid);

            editor.m_processorBridge.submitCanvasSnapshot(*grid);
            editor.m_pixelCanvas.setGridData(*grid);
            editor.m_syncController.setAutoEnvelopeDirty();
            editor.m_syncController.clearRoutingCache();
            editor.m_pedalboardGrid.syncPedals();
            editor.m_pedalboardGrid.repaint();
        });
}

void DrawdioProcessorEditor::exportImage()
{
    juce::Component::SafePointer<DrawdioProcessorEditor> safeThis(this);
    m_imageChooser = std::make_unique<juce::FileChooser>(
        "Export Canvas Image",
        getImagesDir().getChildFile("DrawdioCanvas.png"),
        "*.png");
    m_imageChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;
            auto& editor = *safeThis;
            auto file = fc.getResult();
            if (file == juce::File{})
                return;

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension(".png");

            const auto& grid = editor.m_processorBridge.getGridData();
            juce::Image img(juce::Image::ARGB, GridSize, GridSize, true);
            for (int y = 0; y < GridSize; ++y)
                for (int x = 0; x < GridSize; ++x)
                {
                    const uint8_t v = grid[static_cast<size_t>(y) * GridSize + x];
                    juce::Colour c = (v == 0) ? juce::Colours::transparentBlack
                                              : editor.m_themeImpl.canvasPixelColour(v);
                    img.setPixelAt(x, y, c);
                }

            if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
            {
                juce::PNGImageFormat png;
                png.writeImageToStream(img, *stream);
                stream->flush();
            }
        });
}
