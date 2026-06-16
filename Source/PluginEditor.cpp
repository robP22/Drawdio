#include "PluginEditor.h"
#include "GridLayout.h"
#include "RenderUtils.h"

#include <cmath>

// Layout constants namespace for consistent spacing across all components
namespace Layout
{
    constexpr float PedalboardWidthRatio = 0.55f;
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
    });

    m_hamburgerButton.onClick = [this]()
    {
        showHamburgerMenu();
    };
    m_hamburgerButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    m_hamburgerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    m_hamburgerButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));

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

    const int pedalW  = juce::roundToInt(fullWindow.getWidth() * Layout::PedalboardWidthRatio);
    const int canvasW = fullWindow.getWidth() - pedalW;

    const auto pedalboardArea = fullWindow.withTrimmedLeft(canvasW);
    const auto canvasArea     = fullWindow.withTrimmedRight(pedalW);

    m_pedalboardBackground.setBounds(pedalboardArea);
    m_canvasModule.setBounds(canvasArea);
    m_pedalboardGrid.setBounds(pedalboardArea);

    const int hamburgerSize = 28;
    const int hamburgerMargin = 8;
    m_hamburgerButton.setBounds(fullWindow.getWidth() - hamburgerSize - hamburgerMargin,
                                hamburgerMargin,
                                hamburgerSize, hamburgerSize);
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
                    if (audioProcessor.getDSPProcessor().isParamOverridden(slotIdx, static_cast<int>(param.parameterToken)))
                        continue;
                    if (auto* pedal = m_pedalboardGrid.getPedal(slotIdx))
                        pedal->setKnobValue(static_cast<int>(param.parameterToken), param.currentValue);
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
        chooser->launchAsync(1, [this, chooser](const juce::FileChooser& fc)
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
                }
            }
        });
    });

    menu.addSeparator();
    menu.addItem("Settings...", false, false, nullptr);

    menu.showMenuAsync(juce::PopupMenu::Options(), [](int) {});
}
