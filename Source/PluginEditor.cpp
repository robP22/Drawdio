#include "PluginEditor.h"

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), pedalboardCanvas(p)
{
    addAndMakeVisible(pixelCanvas);
    addAndMakeVisible(pedalboardCanvas);

    // --- Color palette buttons ---
    static const struct { const char* label; PixelCanvasComponent::PixelColor color; } palette[5] = {
        { "K", PixelCanvasComponent::PixelColor::BLACK },
        { "B", PixelCanvasComponent::PixelColor::BLUE },
        { "G", PixelCanvasComponent::PixelColor::GREEN },
        { "R", PixelCanvasComponent::PixelColor::RED },
        { "W", PixelCanvasComponent::PixelColor::WHITE }
    };
    for (int c = 0; c < 5; ++c)
    {
        addAndMakeVisible(colorButtons[c]);
        colorButtons[c].setButtonText(palette[c].label);
        colorButtons[c].onClick = [this, c]() { colorSelected(palette[c].color); };
    }
    colorButtons[0].setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    colorButtons[1].setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    colorButtons[2].setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    colorButtons[3].setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    colorButtons[4].setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);

    // --- Clear button ---
    addAndMakeVisible(clearButton);
    clearButton.setButtonText("Clear");
    clearButton.onClick = [this]() {
        pixelCanvas.clearCanvas();
        audioProcessor.setManualRouting({});
        triggerRecompile();
    };

    // --- Undo button ---
    addAndMakeVisible(undoButton);
    undoButton.setButtonText("Undo");
    undoButton.onClick = [this]() {
        pixelCanvas.undo();
        triggerRecompile();
    };

    // --- Reset Routing button ---
    addAndMakeVisible(resetRoutingButton);
    resetRoutingButton.setButtonText("Reset Route");
    resetRoutingButton.onClick = [this]() {
        audioProcessor.setManualRouting({});
        triggerRecompile();
    };

    // --- Canvas callbacks ---
    pixelCanvas.setOnPenDown([this]() {
        audioProcessor.getPenDebouncer().penDown();
        pixelCanvas.pushUndoState();
    });
    pixelCanvas.setOnPenUp([this]() {
        audioProcessor.getPenDebouncer().penUp();
    });
    pixelCanvas.setOnCanvasSnapshot([this](const auto&) {
        triggerRecompile();
        juce::MessageManager::callAsync([this]() { timerCallback(); });
    });

    pixelCanvas.setGridData(audioProcessor.getGridData());

    setSize(1100, 650);
    startTimerHz(20);
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    stopTimer();
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Canvas-area background: dark "plywood" warm tones
    auto canvasArea = bounds;
    canvasArea.removeFromRight(380);
    canvasArea.removeFromBottom(50);
    juce::ColourGradient woodGrad(juce::Colour(0xFF3A2A1A), 0, 0,
                                   juce::Colour(0xFF1E140A), 0,
                                   static_cast<float>(canvasArea.getHeight()), false);
    g.setGradientFill(woodGrad);
    g.fillRect(canvasArea);

    // Bottom controls area background
    auto controlsArea = bounds.removeFromBottom(50);
    g.setColour(juce::Colour(0xFF111111));
    g.fillRect(controlsArea);
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto controlsRow = bounds.removeFromBottom(50);
    auto rightPanel = bounds.removeFromRight(380);

    // Canvas fills left area
    pixelCanvas.setBounds(bounds.reduced(10));

    // Pedalboard fills right panel
    pedalboardCanvas.setBounds(rightPanel);

    // Controls row: color buttons, clear, undo
    auto row = controlsRow.reduced(10);
    int colorW = 40;
    for (int c = 0; c < 5; ++c)
    {
        colorButtons[c].setBounds(row.removeFromLeft(colorW).reduced(2));
        row.removeFromLeft(4);
    }
    row.removeFromLeft(20);
    clearButton.setBounds(row.removeFromLeft(70).reduced(2));
    row.removeFromLeft(10);
    undoButton.setBounds(row.removeFromLeft(70).reduced(2));
    row.removeFromLeft(10);
    resetRoutingButton.setBounds(row.removeFromLeft(100).reduced(2));
}

void DrawdioProcessorEditor::triggerRecompile()
{
    audioProcessor.getMessageQueue().pushSnapshot(pixelCanvas.getGridData().data());
    audioProcessor.setGridData(pixelCanvas.getGridData());
    audioProcessor.getCompilerThread().notify();
}

void DrawdioProcessorEditor::colorSelected(PixelCanvasComponent::PixelColor color)
{
    pixelCanvas.setCurrentColor(color);
}

void DrawdioProcessorEditor::timerCallback()
{
    bool hadNewResult = false;
    auto& compiler = audioProcessor.getCompilerThread();
    if (compiler.hasCompiledResult())
    {
        auto payloadPtr = compiler.getCompiledPayloadPtr();
        if (payloadPtr)
        {
            audioProcessor.getDSPProcessor().loadPedalConfiguration(std::move(payloadPtr));
            hadNewResult = true;
        }
    }

    pedalboardCanvas.syncPedals();

    auto config = audioProcessor.getDSPProcessor().getCurrentConfig();
    if (config)
    {
        for (auto& p : config->parameters)
        {
            int chainPos = static_cast<int>(p.targetDspNodeRegister);
            if (chainPos >= 0 && chainPos < static_cast<int>(config->routingSlotOrder.size()))
            {
                int slotIdx = config->routingSlotOrder[chainPos];
                if (auto* pedal = pedalboardCanvas.getPedal(slotIdx))
                    pedal->setKnobValue(static_cast<int>(p.parameterToken), p.currentValue);
            }
        }

        if (hadNewResult)
            pedalboardCanvas.updateRouting(config->routingSlotOrder);
    }
}
