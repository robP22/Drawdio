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
        }
    }
    else if (!m_lastRoutingOrder.empty())
    {
        m_lastRoutingOrder.clear();
        m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
    }
}

void DrawdioProcessorEditor::checkForUpdates()
{
    // Drain released config payloads on the message thread, not the audio thread
    audioProcessor.getDSPProcessor().drainReleaseQueue();
    audioProcessor.getDSPProcessor().tryApplyDeferredConfig();
    audioProcessor.consumeCompiledResultIfAvailable();

    // Always sync knob values from the current config so pedals
    // reflect the latest compiled/manual values even without a recompile.
    {
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
        }
    }

    if (!audioProcessor.consumeUINotification())
    {
        refreshRoutingFromConfig();
        return;
    }

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

    repaint();
}

void DrawdioProcessorEditor::timerCallback()
{
    checkForUpdates();
}
