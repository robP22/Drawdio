#include "PedalComponent.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"
#include "PedalDefinition.h"

namespace
{
juce::Colour skinColourForSlot(int slot)
{
    static constexpr uint32_t colours[] {
        0xFF3B5A74, 0xFF6E3E49, 0xFF4D6846,
        0xFF6B603A, 0xFF584E75, 0xFF5E6266
    };
    return juce::Colour(colours[static_cast<size_t>(slot % 6)]);
}
}

PedalComponent::PedalComponent(DrawdioProcessor& processor,
                               int slotIndex,
                               DspModuleType initialType,
                               const ResourceManager& resources,
                               const ThemeManager& theme,
                               PedalSkinManager::PedalSkin skin)
    : audioProcessor(processor),
      m_resources(resources),
      m_theme(theme),
      m_slotIndex(slotIndex),
      m_currentType(initialType),
      m_skin(skin),
      m_definition(&PedalDefinitions::get(initialType))
{
    // Initialize knob values from definition defaults
    for (int i = 0; i < kKnobCount; ++i)
    {
        m_knobValues[i] = m_definition->parameters[static_cast<size_t>(i)].defaultValue;
    }
}

PedalComponent::~PedalComponent()
{
}

void PedalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& texture = m_resources.getTexture(ResourceManager::TextureId::PedalEnclosure);
    
    if (texture.isValid())
    {
        g.drawImage(texture, bounds.getX(), bounds.getY(), 
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, texture.getWidth(), texture.getHeight());
    }
    
    // Draw all 4 knobs in 2x2 grid layout
    for (int i = 0; i < kKnobCount; ++i)
    {
        drawKnob(g, i, m_knobValues[i]);
    }
}

void PedalComponent::setSkin(PedalSkinManager::PedalSkin skin)
{
    if (m_skin != skin)
    {
        m_skin = skin;
        repaint();
    }
}

void PedalComponent::resized()
{
    updateKnobBounds();
}

void PedalComponent::mouseDown(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().toFloat().reduced(5.0f, 9.0f);
    bounds.removeFromTop(6.0f);
    auto labelArea = bounds.withTrimmedTop(bounds.getHeight() * 0.67f).reduced(18.0f, 10.0f);

    if (labelArea.contains(event.position))
        showTypePopup();
}

void PedalComponent::mouseMove(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().toFloat().reduced(5.0f, 9.0f);
    bounds.removeFromTop(6.0f);
    auto labelArea = bounds.withTrimmedTop(bounds.getHeight() * 0.67f).reduced(18.0f, 10.0f);
    setMouseCursor(labelArea.contains(event.position)
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void PedalComponent::showTypePopup()
{
    juce::PopupMenu menu;
    for (int t = 0; t <= static_cast<int>(DspModuleType::GRANULAR_DELAY); ++t)
    {
        auto type = static_cast<DspModuleType>(t);
        menu.addItem(t + 1, PedalDefinitions::getDisplayName(type), true, type == m_currentType);
    }

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result)
        {
            if (result > 0)
            {
                auto type = static_cast<DspModuleType>(result - 1);
                m_currentType = type;
                repaint();
                audioProcessor.setPedalSlot(m_slotIndex, type);
            }
        });
}

void PedalComponent::syncFromProcessor()
{
    auto type = audioProcessor.getPedalSlot(m_slotIndex);
    if (type != m_currentType)
    {
        m_currentType = type;
        m_definition = &PedalDefinitions::get(type);
        repaint();
    }
}

void PedalComponent::setKnobValue(int knobIdx, float value)
{
    if (knobIdx >= 0 && knobIdx < kKnobCount)
    {
        m_knobValues[static_cast<size_t>(knobIdx)] = value;
        repaint();
    }
}

void PedalComponent::updateKnobBounds()
{
    if (m_definition == nullptr)
        return;

    auto pedalBounds = getLocalBounds().toFloat();
    const float pedalWidth = pedalBounds.getWidth();
    const float pedalHeight = pedalBounds.getHeight();

    // Knob size: 39x39 pixels (scaled up 10%)
    constexpr float kKnobSize = 39.0f;
    constexpr float kSpacingOffsetX = -5.0f;
    constexpr float kSpacingOffsetY = -5.0f;
    constexpr float kYShift = -10.0f;
    const float halfKnob = kKnobSize / 2.0f;

    for (int i = 0; i < kKnobCount; ++i)
    {
        const auto& normBounds = m_definition->knobLayout[static_cast<size_t>(i)];
        
        // Calculate center position from normalized coordinates with adjustments
        const float centerX = pedalBounds.getX() + (normBounds.centreX + kSpacingOffsetX / pedalWidth) * pedalWidth;
        const float centerY = pedalBounds.getY() + (normBounds.centreY + kSpacingOffsetY / pedalHeight + kYShift / pedalHeight) * pedalHeight;

        // Create square bounds centered at the adjusted position
        m_knobBounds[static_cast<size_t>(i)] = juce::Rectangle<float>(
            centerX - halfKnob,
            centerY - halfKnob,
            kKnobSize,
            kKnobSize);
    }
}

void PedalComponent::drawKnob(juce::Graphics& g, int knobIdx, float value)
{
    if (knobIdx < 0 || knobIdx >= kKnobCount)
        return;

    const auto& bounds = m_knobBounds[static_cast<size_t>(knobIdx)];
    if (!bounds.isEmpty())
    {
        const auto& knobImage = m_resources.getImage(ResourceManager::ImageId::PedalKnobImage);
        
        if (knobImage.isValid())
        {
            // Scale entire sprite sheet to fit the 35x35 square bounds
            const int destSize = static_cast<int>(bounds.getWidth());
            
            g.drawImage(knobImage,
                       static_cast<int>(bounds.getX()),
                       static_cast<int>(bounds.getY()),
                       destSize, destSize,
                       0, 0, knobImage.getWidth(), knobImage.getHeight());
        }
    }
}

juce::Point<float> PedalComponent::getInputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getX() + 42.0f, bounds.getY() + 15.0f };
}

juce::Point<float> PedalComponent::getOutputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getRight() - 42.0f, bounds.getY() + 15.0f };
}
