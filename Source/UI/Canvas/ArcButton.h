#pragma once
#include <JuceHeader.h>
#include <functional>

class ArcButton : public juce::Component
{
public:
    ArcButton();

    void setArc(float cx, float cy, float innerR, float outerR,
                float startAngle, float endAngle);
    void setArc(float cx, float cy, float innerR, float outerR,
                float innerStart, float innerEnd,
                float outerStart, float outerEnd);
    void setToggleState(bool on, juce::NotificationType n = juce::dontSendNotification);
    bool getToggleState() const { return m_toggleOn; }
    void setClickingTogglesState(bool v) { m_toggleable = v; }

    void setAccentColour(juce::Colour c) { m_accent = c; repaint(); }

    using IconDrawer = std::function<void(juce::Graphics&, juce::Rectangle<float>)>;
    void setDrawIcon(IconDrawer fn) { m_drawIcon = std::move(fn); repaint(); }

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    juce::Path m_arcPath;
    juce::Colour m_accent = juce::Colour(0xFF4A90D9);
    IconDrawer m_drawIcon;
    bool m_toggleOn = false;
    bool m_toggleable = false;
    bool m_hovered = false;
    bool m_pressed = false;
    float m_centreX = 0.0f, m_centreY = 0.0f;
    float m_innerR = 0.0f, m_outerR = 0.0f;
    float m_startAngle = 0.0f, m_endAngle = 0.0f;
};
