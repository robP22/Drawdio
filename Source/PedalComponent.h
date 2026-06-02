#pragma once

#include <JuceHeader.h>

class DrawdioProcessor;

class PedalComponent : public juce::Component
{
public:
    PedalComponent(DrawdioProcessor& processor, int slotIndex, int spriteFrameX, int spriteFrameY);
    ~PedalComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Knob control - drawing controlled only
    void setKnobValue(int knobIdx, float value);  // value 0.0 to 1.0
    float getKnobValue(int knobIdx) const;

    // Sprite region
    static constexpr int spriteCols = 2;
    static constexpr int spriteRows = 3;
    static int spriteWidth() { return 1536 / spriteCols; }  // 768
    static int spriteHeight() { return 1024 / spriteRows; }  // ~341

private:
    // Knob look-and-feel with 8-lobe scalloped design
    class PedalKnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                             float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                             juce::Slider& slider) override;

        static PedalKnobLookAndFeel& getInstance();
    };

    void loadSpriteSheet();
    void initKnobs();

    DrawdioProcessor& audioProcessor;
    int m_slotIndex;
    int m_spriteFrameX;  // 0 or 1
    int m_spriteFrameY;  // 0, 1, or 2

    juce::Image m_pedalImage;
    juce::Slider m_knobs[4];  // 4 knobs per pedal
    float m_knobValues[4] = {0.5f, 0.5f, 0.5f, 0.5f};  // Start at center
};