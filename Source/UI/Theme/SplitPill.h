#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <functional>

class SplitPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    SplitPill()
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setRepaintsOnMouseActivity(true);
    }

    void setTexts(const juce::String& left, const juce::String& right)
    {
        m_leftText = left.toUpperCase();
        m_rightText = right.toUpperCase();
        repaint();
    }

    void setLeftText(const juce::String& t) { m_leftText = t.toUpperCase(); repaint(); }
    void setRightText(const juce::String& t) { m_rightText = t.toUpperCase(); repaint(); }

    void setFontScales(float leftRatio, float rightRatio)
    {
        m_leftFontScale = leftRatio;
        m_rightFontScale = rightRatio;
        repaint();
    }

    void setPadScale(float padRatio)
    {
        m_padScale = padRatio;
        repaint();
    }

    void setTooltips(const juce::String& leftTip, const juce::String& rightTip)
    {
        m_leftTip = leftTip;
        m_rightTip = rightTip;
        updateTooltip();
    }

    int getPreferredWidth(float pillHeight) const;

    std::function<void()> onLeftClick;
    std::function<void()> onRightClick;

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

private:
    bool isLeftHalf(int x) const { return x < getWidth() / 2; }
    void updateHover(int x);
    void updateTooltip();

    juce::String m_leftText;
    juce::String m_rightText;
    juce::String m_leftTip;
    juce::String m_rightTip;
    bool m_leftHovered = false;
    bool m_rightHovered = false;
    bool m_leftDown = false;
    bool m_rightDown = false;
    int m_activeSide = 0;
    float m_leftFontScale = 0.55f;
    float m_rightFontScale = 0.60f;
    float m_padScale = 0.25f;
};
