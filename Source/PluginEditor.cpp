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
}

WorkspaceBackground::WorkspaceBackground(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme)
{
    setInterceptsMouseClicks(false, false);
}

void WorkspaceBackground::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (m_resources.getTexture(ResourceManager::TextureId::WorkspaceWood).isValid())
        RenderUtils::drawImageScaled(g, m_resources.getTexture(ResourceManager::TextureId::WorkspaceWood), bounds);
    else
        g.fillAll(m_theme.workspaceFallback());

    juce::ColourGradient light(juce::Colours::white.withAlpha(0.13f), 0.0f, 0.0f,
                               juce::Colours::transparentWhite,
                               bounds.getWidth() * 0.66f,
                               bounds.getHeight() * 0.72f, true);
    g.setGradientFill(light);
    g.fillAll();

    juce::ColourGradient vignette(juce::Colours::transparentBlack,
                                  bounds.getCentreX(),
                                  bounds.getCentreY(),
                                  m_theme.workspaceVignette(),
                                  0.0f,
                                  bounds.getHeight() * 0.68f,
                                  true);
    g.setGradientFill(vignette);
    g.fillAll();
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
}

void ColorPalette::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(bounds.reduced(2.0f).translated(0.0f, 2.0f), 7.0f);
    RenderUtils::drawTextureClippedToRoundedRect(g,
                                                 m_resources.getTexture(ResourceManager::TextureId::PalettePaint),
                                                 bounds.reduced(2.0f),
                                                 7.0f,
                                                 1.0f);

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
        g.fillEllipse(blobBounds.translated(0.0f, selected ? 5.0f : 7.0f));

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
    auto area = getLocalBounds().reduced(8, 6).toFloat();
    const auto slotW = area.getWidth() / static_cast<float>(m_blobs.size());
    const auto blobSize = juce::jmin(52.0f, area.getHeight() - 4.0f);

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(area.getX() + static_cast<float>(i) * slotW,
                                           area.getY(),
                                           slotW,
                                           area.getHeight());
        m_blobs[static_cast<size_t>(i)].bounds = slot.withSizeKeepingCentre(blobSize, blobSize * 0.78f);
    }
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
    styleButton(m_drawButton, m_theme.drawButtonAccent());
    styleButton(m_undoButton, m_theme.undoButtonAccent());
    styleButton(m_clearButton, m_theme.clearButtonAccent());

    m_drawButton.setClickingTogglesState(true);
    m_drawButton.setToggleState(true, juce::dontSendNotification);
    m_drawButton.setEnabled(false);

    addAndMakeVisible(m_drawButton);
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

void CanvasTools::paint(juce::Graphics& g)
{
    RenderUtils::drawInsetPanel(g, getLocalBounds().toFloat().reduced(1.0f), 7.0f);
}

void CanvasTools::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    const auto buttonH = juce::jmax(24, (area.getHeight() - 8) / 3);
    m_drawButton.setBounds(area.removeFromTop(buttonH));
    area.removeFromTop(4);
    m_undoButton.setBounds(area.removeFromTop(buttonH));
    area.removeFromTop(4);
    m_clearButton.setBounds(area.removeFromTop(buttonH));
}

void CanvasTools::styleButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF25292C));
    button.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.2f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

CanvasStatusDisplay::CanvasStatusDisplay(const ThemeManager& theme)
    : m_theme(theme)
{
}

void CanvasStatusDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    RenderUtils::drawInsetPanel(g, bounds, 7.0f);

    auto content = bounds.reduced(12.0f, 8.0f).toNearestInt();
    auto chip = content.removeFromLeft(34).reduced(0, 7).toFloat();
    g.setColour(m_theme.canvasPixelColour(static_cast<uint8_t>(m_selectedColor)));
    g.fillEllipse(chip);
    g.setColour(juce::Colours::white.withAlpha(0.34f));
    g.drawEllipse(chip, 1.2f);

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(juce::Colours::whitesmoke.withAlpha(0.88f));
    g.drawText(colorName(m_selectedColor), content.removeFromTop(22), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.72f));
    g.drawText(juce::String(m_changedCellCount) + " / " + juce::String(TotalCells),
               content,
               juce::Justification::centredLeft);
}

void CanvasStatusDisplay::setSelectedColor(PixelCanvasComponent::PixelColor color)
{
    m_selectedColor = color;
    repaint();
}

void CanvasStatusDisplay::setChangedCellCount(int count)
{
    if (m_changedCellCount == count)
        return;

    m_changedCellCount = count;
    repaint();
}

CanvasModule::CanvasModule(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme),
      m_pixelCanvas(theme),
      m_palette(resources, theme),
      m_tools(theme),
      m_status(theme)
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);
    addAndMakeVisible(m_tools);
    addAndMakeVisible(m_status);

    m_palette.setOnColorSelected([this](auto color)
    {
        m_pixelCanvas.setCurrentColor(color);
        m_status.setSelectedColor(color);
    });

    m_tools.setOnUndo([this]()
    {
        m_pixelCanvas.undo();
        refreshStatus();
    });

    m_tools.setOnClear([this]()
    {
        if (m_onClear)
            m_onClear();

        m_pixelCanvas.clearCanvas();
        refreshStatus();
    });

    m_status.setSelectedColor(m_pixelCanvas.getCurrentColor());
    refreshStatus();
}

void CanvasModule::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 9.0f), 14.0f);

    RenderUtils::fillVerticalGloss(g, bounds, m_theme.panelTop(), m_theme.panelBottom(), 8.0f);
    g.setColour(m_theme.panelEdge());
    g.drawRoundedRectangle(bounds.reduced(1.0f), 12.0f, 2.0f);

    auto canvasPocket = m_pixelCanvas.getBounds().toFloat().expanded(10.0f);
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.fillRoundedRectangle(canvasPocket, 9.0f);
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawRoundedRectangle(canvasPocket.reduced(1.0f), 8.0f, 1.0f);
}

void CanvasModule::resized()
{
    auto area = getLocalBounds().reduced(22, 20);
    auto bottom = area.removeFromBottom(82);
    area.removeFromBottom(14);

    const auto square = juce::jmin(area.getWidth(), area.getHeight());
    auto canvasArea = area.withSizeKeepingCentre(square, square);
    m_pixelCanvas.setBounds(canvasArea.reduced(8));

    auto controls = bottom;
    auto toolsArea = controls.removeFromRight(112);
    controls.removeFromRight(10);
    auto statusArea = controls.removeFromRight(162);
    controls.removeFromRight(10);

    m_palette.setBounds(controls);
    m_status.setBounds(statusArea);
    m_tools.setBounds(toolsArea);
}

void CanvasModule::refreshStatus()
{
    m_status.setChangedCellCount(m_pixelCanvas.getChangedCellCount());
}

LevelMeter::LevelMeter(juce::String label, const ThemeManager& theme)
    : m_theme(theme),
      m_label(std::move(label))
{
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    RenderUtils::drawInsetPanel(g, bounds, 6.0f);

    auto meter = bounds.reduced(8.0f, 8.0f);
    auto labelArea = meter.removeFromLeft(26.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.82f));
    g.drawText(m_label, labelArea.toNearestInt(), juce::Justification::centred);

    auto bar = meter.reduced(2.0f, 5.0f);
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.fillRoundedRectangle(bar, 3.0f);

    auto lit = bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, m_level));
    juce::ColourGradient gradient(juce::Colour(0xFF36D987), lit.getX(), lit.getY(),
                                  juce::Colour(0xFFEBD45E), lit.getRight(), lit.getY(), false);
    gradient.addColour(0.82, juce::Colour(0xFFEBD45E));
    gradient.addColour(1.0, juce::Colour(0xFFE94D44));
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(lit, 3.0f);

    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawRoundedRectangle(bar, 3.0f, 1.0f);
}

void LevelMeter::setLevel(float level)
{
    level = juce::jlimit(0.0f, 1.0f, level);
    if (std::abs(level - m_level) < 0.002f)
        return;

    m_level = level;
    repaint();
}

BottomControlBar::BottomControlBar(const ThemeManager& theme)
    : m_theme(theme),
      m_inputMeter("IN", theme),
      m_outputMeter("OUT", theme)
{
    addAndMakeVisible(m_inputMeter);
    addAndMakeVisible(m_outputMeter);
    addAndMakeVisible(m_dryWetSlider);
    addAndMakeVisible(m_oversamplingSelector);
    addAndMakeVisible(m_qualitySelector);

    m_dryWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_dryWetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    m_dryWetSlider.setRange(0.0, 100.0, 1.0);
    m_dryWetSlider.setValue(50.0, juce::dontSendNotification);
    m_dryWetSlider.setTextValueSuffix("%");
    m_dryWetSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE6ECEF));
    m_dryWetSlider.setColour(juce::Slider::trackColourId, m_theme.drawButtonAccent());
    m_dryWetSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF0E1012));

    m_oversamplingSelector.addItem("1x", 1);
    m_oversamplingSelector.addItem("2x", 2);
    m_oversamplingSelector.addItem("4x", 3);
    m_oversamplingSelector.setSelectedId(1, juce::dontSendNotification);

    m_qualitySelector.addItem("Eco", 1);
    m_qualitySelector.addItem("Studio", 2);
    m_qualitySelector.addItem("Ultra", 3);
    m_qualitySelector.setSelectedId(2, juce::dontSendNotification);

    for (auto* combo : { &m_oversamplingSelector, &m_qualitySelector })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1D2225));
        combo->setColour(juce::ComboBox::textColourId, juce::Colours::whitesmoke);
        combo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF4A555B));
        combo->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFB8C1C5));
    }
}

void BottomControlBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.fillRect(bounds);

    juce::ColourGradient gradient(juce::Colour(0xFF24282B), bounds.getX(), bounds.getY(),
                                  juce::Colour(0xFF0C0E10), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds.reduced(8.0f, 5.0f), 8.0f);

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds.reduced(9.0f, 6.0f), 7.0f, 1.0f);

    auto labels = getLocalBounds().reduced(24, 8);
    labels.removeFromLeft(414);
    auto dryLabel = labels.removeFromLeft(66);
    g.setColour(juce::Colours::lightgrey.withAlpha(0.76f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("DRY/WET", dryLabel, juce::Justification::centredLeft);

    labels.removeFromLeft(240);
    g.drawText("OS", labels.removeFromLeft(28), juce::Justification::centredLeft);
    labels.removeFromLeft(120);
    g.drawText("QUALITY", labels.removeFromLeft(58), juce::Justification::centredLeft);
}

void BottomControlBar::resized()
{
    auto area = getLocalBounds().reduced(24, 17);
    const int meterW = 190;
    m_inputMeter.setBounds(area.removeFromLeft(meterW));
    area.removeFromLeft(14);
    m_outputMeter.setBounds(area.removeFromLeft(meterW));
    area.removeFromLeft(28);

    area.removeFromLeft(70);
    m_dryWetSlider.setBounds(area.removeFromLeft(260).reduced(0, 4));
    area.removeFromLeft(34);

    area.removeFromLeft(30);
    m_oversamplingSelector.setBounds(area.removeFromLeft(116).reduced(0, 7));
    area.removeFromLeft(34);
    area.removeFromLeft(60);
    m_qualitySelector.setBounds(area.removeFromLeft(130).reduced(0, 7));
}

void BottomControlBar::setMeterLevels(float inputLevel, float outputLevel)
{
    m_inputMeter.setLevel(inputLevel);
    m_outputMeter.setLevel(outputLevel);
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      m_workspaceBackground(m_resourceManager, m_theme),
      m_canvasModule(m_resourceManager, m_theme),
      m_pedalboardCanvas(p, m_resourceManager, m_theme, m_routingManager),
      m_bottomControlBar(m_theme)
{
    addAndMakeVisible(m_workspaceBackground);
    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardCanvas);
    addAndMakeVisible(m_bottomControlBar);
    m_workspaceBackground.toBack();

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
        juce::MessageManager::callAsync([this]() { timerCallback(); });
    });

    m_canvasModule.setOnClear([this]()
    {
        m_routingManager.clearManualRouting();
        m_pedalboardCanvas.updateRouting({});
        audioProcessor.setManualRouting({});
    });

    setSize(1400, 800);
    startTimerHz(30);
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    stopTimer();
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(m_theme.editorBackground());
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    m_workspaceBackground.setBounds(bounds);

    auto bottom = bounds.removeFromBottom(78);
    m_bottomControlBar.setBounds(bottom);

    auto content = bounds.reduced(18, 16);
    const auto gap = 18;
    const auto pedalW = juce::jlimit(560, 620, content.getWidth() - 760);
    auto pedalArea = content.removeFromRight(pedalW);
    content.removeFromRight(gap);

    m_canvasModule.setBounds(content);
    m_pedalboardCanvas.setBounds(pedalArea);
}

void DrawdioProcessorEditor::triggerRecompile()
{
    const auto& grid = m_canvasModule.getPixelCanvas().getGridData();
    audioProcessor.getMessageQueue().pushSnapshot(grid.data());
    audioProcessor.setGridData(grid);
    audioProcessor.getCompilerThread().notify();
}

void DrawdioProcessorEditor::timerCallback()
{
    const auto previousRevision = m_seenConfigRevision;
    audioProcessor.consumeCompiledResultIfAvailable();
    m_seenConfigRevision = audioProcessor.getConfigRevision();
    const bool configChanged = m_seenConfigRevision != previousRevision;

    m_pedalboardCanvas.syncPedals();

    auto config = audioProcessor.getDSPProcessor().getCurrentConfig();
    if (config)
    {
        for (auto& param : config->parameters)
        {
            const int chainPos = static_cast<int>(param.targetDspNodeRegister);
            if (chainPos >= 0 && chainPos < static_cast<int>(config->routingSlotOrder.size()))
            {
                const int slotIdx = config->routingSlotOrder[static_cast<size_t>(chainPos)];
                if (auto* pedal = m_pedalboardCanvas.getPedal(slotIdx))
                    pedal->setKnobValue(static_cast<int>(param.parameterToken), param.currentValue);
            }
        }

        if (configChanged || config->routingSlotOrder != m_lastRoutingOrder)
        {
            m_lastRoutingOrder = config->routingSlotOrder;
            m_pedalboardCanvas.updateRouting(m_lastRoutingOrder);
        }
    }
    else if (!m_lastRoutingOrder.empty())
    {
        m_lastRoutingOrder.clear();
        m_pedalboardCanvas.updateRouting(m_lastRoutingOrder);
    }

    m_bottomControlBar.setMeterLevels(audioProcessor.getInputMeterLevel(),
                                      audioProcessor.getOutputMeterLevel());
    m_canvasModule.refreshStatus();
}
