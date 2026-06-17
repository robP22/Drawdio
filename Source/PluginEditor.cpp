#include "PluginEditor.h"
#include "GridLayout.h"
#include "RenderUtils.h"

#include <cmath>

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

// Background component implementations - flat textures without decorative frames
WoodGrainBackground::WoodGrainBackground(const ResourceManager& resources, const IThemeProvider& theme)
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

PedalboardBackground::PedalboardBackground(const ResourceManager& resources, const IThemeProvider& theme)
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

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(p),
      audioProcessor(p),
      m_woodGrainBackground(m_resourceManager, m_theme),
      m_pedalboardBackground(m_resourceManager, m_theme),
      m_canvasModule(m_resourceManager, m_theme),
      m_pedalboardGrid(p, m_resourceManager, m_theme, m_routingManager)
{
    addAndMakeVisible(m_woodGrainBackground);
    addAndMakeVisible(m_pedalboardBackground);
    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardGrid);
    addAndMakeVisible(m_hamburgerButton);

    auto& pixelCanvas = m_canvasModule.getPixelCanvas();
    pixelCanvas.setGridData(audioProcessor.getGridData());

    // Immediately apply restored knob values to pedals
    {
        auto knobVals = audioProcessor.getKnobValues();
        for (int s = 0; s < PedalSlotCount; ++s)
            if (auto* pedal = m_pedalboardGrid.getPedal(s))
                for (int k = 0; k < 4; ++k)
                    pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * 4 + k)]);
    }

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

    m_canvasModule.setOnClear([this]()
    {
        audioProcessor.getDSPProcessor().scheduleReset();
        audioProcessor.getDSPProcessor().clearParamOffsets();
    });

    m_hamburgerButton.onClick = [this]()
    {
        showHamburgerMenu();
    };

    setSize(1400, 800);
    startTimerHz(20);
    juce::MessageManager::callAsync([self = juce::Component::SafePointer<DrawdioProcessorEditor>(this)]()
    {
        if (self != nullptr)
            self->checkForUpdates();
    });
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    stopTimer();
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void DrawdioProcessorEditor::resized()
{
    const auto fullWindow = getLocalBounds();
    m_woodGrainBackground.setBounds(fullWindow);

    const int pedalW  = juce::roundToInt(fullWindow.getWidth() * Layout_PedalboardWidthRatio);
    const int canvasW = fullWindow.getWidth() - pedalW;

    const auto pedalboardArea = fullWindow.withTrimmedLeft(canvasW);
    const auto canvasArea     = fullWindow.withTrimmedRight(pedalW);

    m_pedalboardBackground.setBounds(pedalboardArea);
    m_canvasModule.setBounds(canvasArea);
    m_pedalboardGrid.setBounds(pedalboardArea);

    const int hamburgerSize = 36;
    const int hamburgerMargin = 8;
    m_hamburgerButton.setBounds(fullWindow.getWidth() - hamburgerSize - hamburgerMargin,
                                hamburgerMargin,
                                hamburgerSize, hamburgerSize);

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
    m_canvasModule.setVerticalOffsets(canvasTopPx, paletteShiftPx, paletteCenterPx);
}

void DrawdioProcessorEditor::triggerRecompile()
{
    const auto& grid = m_canvasModule.getPixelCanvas().getGridData();
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

void DrawdioProcessorEditor::checkForUpdates()
{
    bool needsRepaint = false;

    // Drain released config payloads on the message thread, not the audio thread
    audioProcessor.getDSPProcessor().drainReleaseQueue();
    audioProcessor.getDSPProcessor().tryApplyDeferredConfig();

    // Consume compiled config and sync knob visuals only when revision changes.
    // Skip knobs the user has manually overridden (valid cache entry).
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
                    }
                }
            }
        }
    }

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

    {
        auto config = audioProcessor.getDSPProcessor().getCurrentConfig();
        if (config)
        {
            if (config->routingSlotOrder != m_lastRoutingOrder)
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
    }

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

    menu.addItem("Save Preset", [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.drawdio");
        chooser->launchAsync(2, [this, chooser](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result == juce::File{})
                return;

            juce::File file = result;
            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension(".drawdio");

            juce::MemoryBlock state = audioProcessor.createPresetState();
            juce::FileOutputStream stream(file);
            if (stream.openedOk())
                stream.write(state.getData(), state.getSize());
        });
    });

    menu.addItem("Load Preset", [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Preset",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.drawdio");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result == juce::File{})
                return;

            juce::MemoryBlock data;
            if (result.loadFileAsData(data))
            {
                if (audioProcessor.applyPresetState(data.getData(),
                                                    static_cast<int>(data.getSize())))
                {
                    m_canvasModule.getPixelCanvas().setGridData(
                        audioProcessor.getGridData());
                    triggerRecompile();

                    auto knobVals = audioProcessor.getKnobValues();
                    for (int s = 0; s < PedalSlotCount; ++s)
                        if (auto* pedal = m_pedalboardGrid.getPedal(s))
                            for (int k = 0; k < 4; ++k)
                                pedal->setKnobValue(k, knobVals[static_cast<size_t>(s * 4 + k)]);
                }
            }
        });
    });

    menu.addSeparator();
    menu.addItem("Settings...", false, false, nullptr);

    menu.showMenuAsync(juce::PopupMenu::Options(), [](int) {});
}
