#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include "PluginProcessor.h"
#include "PixelCanvasComponent.h"
#include "PedalboardCanvas.h"

class DrawdioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    DrawdioProcessorEditor(DrawdioProcessor&);
    ~DrawdioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void triggerRecompile();
    void colorSelected(PixelCanvasComponent::PixelColor color);
    void timerCallback() override;

    DrawdioProcessor& audioProcessor;
    PixelCanvasComponent pixelCanvas;
    PedalboardCanvas pedalboardCanvas;

    juce::TextButton colorButtons[5];
    juce::TextButton clearButton;
    juce::TextButton undoButton;
    juce::TextButton resetRoutingButton;
};
