#pragma once
#include <JuceHeader.h>
#include <functional>
#include "UI/Theme/HeaderPill.h"

class PedalboardHeader : public juce::Component
{
public:
    PedalboardHeader();

    void resized() override;

    void updateModeButton(bool manual);
    void setButtonCenterY(float centerY) { m_buttonCenterY = centerY; resized(); }
    std::function<void()> onReset;
    std::function<void()> onModeToggle;

private:
    HeaderPill m_resetPill;
    HeaderPill m_modePill;
    float m_buttonCenterY = 0.0f;
};
