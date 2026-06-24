#include "PedalComponent.h"
#include "GridLayout.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"
#include "PedalDefinition.h"

#include <cmath>

namespace
{
juce::Rectangle<int> japTextureCell(int idx)
{
    static constexpr int colW[5] = { 307, 307, 307, 307, 308 };
    static constexpr int rowH[2] = { 274, 279 };
    int r = idx / 5;
    int c = idx % 5;
    int x = 0;
    for (int i = 0; i < c; ++i) x += colW[i];
    int y = 0;
    for (int i = 0; i < r; ++i) y += rowH[i];
    return { x, y, colW[c], rowH[r] };
}
}

PedalComponent::PedalComponent(DrawdioProcessor& processor,
                               int slotIndex,
                               DspModuleType initialType,
                               const ResourceManager& resources,
                               const IThemeProvider& theme)
    : audioProcessor(processor),
      m_resources(resources),
      m_theme(theme),
      m_slotIndex(slotIndex),
      m_currentType(initialType),
      m_definition(&PedalDefinitions::get(initialType))
{
    const auto& knobImg = m_resources.getImage(ResourceManager::ImageId::PedalKnobImage);
    for (int i = 0; i < kKnobCount; ++i)
    {
        m_knobs[i] = std::make_unique<SpriteKnob>(knobImg, 0.0f, 1.0f);
        m_knobs[i]->setValue(m_definition->parameters[static_cast<size_t>(i)].defaultValue);
        m_knobs[i]->onDragStart = [this, i](float v) { m_knobDragStartValues[i] = v; };
        m_knobs[i]->onValueChanged = [this, i](float v) {
            audioProcessor.getDSPProcessor().applyParamOffset(m_slotIndex, i, m_knobDragStartValues[i], v);
        };
        m_knobs[i]->onRightClick = [this, i]() {
            auto& dsp = audioProcessor.getDSPProcessor();
            bool linked = dsp.isKnobLinked(m_slotIndex, i);
            juce::PopupMenu menu;
            menu.addItem("Link to Automation", !linked, linked,
                [this, i, &dsp]() { dsp.setKnobLink(m_slotIndex, i, true); repaint(); });
            menu.addItem("Unlink from Automation", linked, !linked,
                [this, i, &dsp]() { dsp.setKnobLink(m_slotIndex, i, false); repaint(); });
            menu.showMenuAsync(juce::PopupMenu::Options());
        };
        addAndMakeVisible(m_knobs[i].get());
    }
}

PedalComponent::~PedalComponent()
{
}

void PedalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto pedalW = bounds.getWidth();
    const auto pedalH = bounds.getHeight();

    const float insetX = pedalW * GridLayout::PedalBodyInsetXRatio;
    const float insetY = pedalH * GridLayout::PedalBodyInsetYRatio;
    const float maskW  = pedalW * GridLayout::PedalBodyMaskWRatio;
    const float maskH  = pedalH * GridLayout::PedalBodyMaskHRatio;
    const float corner = pedalW * GridLayout::PedalBodyCornerRatio;

    {
        const auto& enclosure = m_resources.getTexture(ResourceManager::TextureId::PedalEnclosure);
        if (enclosure.isValid())
            g.drawImage(enclosure, 0, 0, pedalW, pedalH, 0, 0, enclosure.getWidth(), enclosure.getHeight());

        const auto& japSheet = m_resources.getTexture(ResourceManager::TextureId::JapanesePedalSheet);
        if (japSheet.isValid())
        {
            const auto srcRect = japTextureCell(m_slotIndex % 6);
            g.saveState();
            {
                juce::Path clipPath;
                clipPath.addRoundedRectangle(insetX, insetY, maskW, maskH, corner);
                g.reduceClipRegion(clipPath);
            }
            g.setOpacity(0.85f);
            g.drawImage(japSheet, 0, 0, pedalW, pedalH, srcRect.getX(), srcRect.getY(), srcRect.getWidth(), srcRect.getHeight());
            g.restoreState();
        }

        {
            juce::Path clipPath;
            clipPath.addRoundedRectangle(insetX, insetY, maskW, maskH, corner);
            g.saveState();
            g.reduceClipRegion(clipPath);
            juce::Rectangle<float> body(insetX, insetY, maskW, maskH);
            RenderUtils::paintCurvatureVignette(g, body, corner);
            RenderUtils::paintEdgeHighlight(g, body, corner);
            g.setOpacity(0.03f);
            g.drawImage(RenderUtils::getNoiseTexture(), body.getX(), body.getY(), body.getWidth(), body.getHeight(), 0, 0, 128, 128);
            g.setOpacity(1.0f);
            g.restoreState();
        }
    }

    const auto& ledImage = m_resources.getImage(ResourceManager::ImageId::PedalLedImage);
    if (ledImage.isValid())
    {
        const float ledSize = pedalH * GridLayout::LedSizeRatio;
        const bool isOn = (m_currentType != DspModuleType::BYPASS);
        const auto& frame = m_resources.getSpriteFrame(
            isOn ? ResourceManager::SpriteId::PedalLedOn
                 : ResourceManager::SpriteId::PedalLedOff);
        g.drawImage(ledImage,
                    pedalW * GridLayout::LedCenterXRatio - ledSize * 0.5f,
                    pedalH * GridLayout::LedCenterYRatio - ledSize * 0.5f,
                    ledSize, ledSize,
                    frame.source.getX(), frame.source.getY(),
                    frame.source.getWidth(), frame.source.getHeight());
    }

    const auto labelArea = getLabelArea();
    if (!labelArea.isEmpty())
    {
        g.setGradientFill(juce::ColourGradient(
            m_theme.pedalLcdTop(), labelArea.getTopLeft(),
            m_theme.pedalLcdBottom(), labelArea.getBottomLeft(), false));
        g.fillRoundedRectangle(labelArea, m_theme.pedalStyle().lcdRadius);

        g.setColour(juce::Colour(0xFFC8E0E8));
        auto lcdFont = juce::Font(juce::FontOptions(labelArea.getHeight() * 0.35f));
        lcdFont.setTypefaceName(juce::Font::getDefaultMonospacedFontName());
        g.setFont(lcdFont);
        g.drawFittedText(m_definition ? juce::String(m_definition->displayName) : "---",
                         labelArea.toNearestInt().reduced(2, 0),
                         juce::Justification::centred, 2, 0.85f);

        g.setGradientFill(juce::ColourGradient(
            juce::Colours::black.withAlpha(0.35f), labelArea.getTopLeft(),
            juce::Colours::black.withAlpha(0.10f), labelArea.getBottomLeft(), false));
        g.fillRoundedRectangle(labelArea, m_theme.pedalStyle().lcdRadius);
    }
    
    for (int i = 0; i < kKnobCount; ++i)
    {
        if (audioProcessor.getDSPProcessor().isKnobLinked(m_slotIndex, i))
        {
            float ringD = m_knobBounds[i].getWidth() * GridLayout::KnobLinkRingRatio;
            float ringR = ringD * 0.5f;
            g.setColour(juce::Colours::limegreen.withAlpha(0.85f));
            g.drawEllipse(m_knobBounds[i].getCentreX() - ringR,
                          m_knobBounds[i].getCentreY() - ringR,
                          ringD, ringD, 2.0f);
        }
    }

    if (m_definition == nullptr)
        return;

    const float fontSize = pedalH * GridLayout::KnobFontSizeRatio;
    const float labelWidth = pedalW * GridLayout::KnobLabelWidthRatio;
    const float labelHeight = fontSize * 1.3f;
    const float offsetY = pedalH * GridLayout::KnobLabelOffsetYRatio;

    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(fontSize)));

    for (int i = 0; i < kKnobCount; ++i)
    {
        const auto& knobBounds = m_knobBounds[static_cast<size_t>(i)];
        const auto& param = m_definition->parameters[static_cast<size_t>(i)];

        g.drawText(param.label,
                   (knobBounds.getCentreX() - labelWidth * 0.5f),
                   (knobBounds.getCentreY() + offsetY - labelHeight * 0.5f),
                   labelWidth, labelHeight,
                   juce::Justification::centred, false);
    }
}

void PedalComponent::resized()
{
    updateKnobBounds();
    for (int i = 0; i < kKnobCount; ++i)
        if (m_knobs[i])
            m_knobs[i]->setBounds(m_knobBounds[i].toNearestInt());
}

juce::Rectangle<float> PedalComponent::getLabelArea() const
{
    const float pedalW = static_cast<float>(getWidth());
    const float pedalH = static_cast<float>(getHeight());
    auto bounds = getLocalBounds().toFloat()
                      .reduced(pedalW * GridLayout::LabelInsetXRatio, pedalH * GridLayout::LabelInsetYRatio);
    bounds.removeFromTop(pedalH * GridLayout::LabelTopTrimRatio);
    return bounds.withTrimmedTop(bounds.getHeight() * 0.67f)
                .reduced(pedalW * GridLayout::LabelReducedXRatio, pedalH * GridLayout::LabelReducedYRatio);
}

void PedalComponent::mouseDown(const juce::MouseEvent& event)
{
    if (getLabelArea().contains(event.position))
        showTypePopup();
}

void PedalComponent::mouseDrag(const juce::MouseEvent&) {}
void PedalComponent::mouseUp(const juce::MouseEvent&) {}

void PedalComponent::mouseMove(const juce::MouseEvent& event)
{
    setMouseCursor(getLabelArea().contains(event.position)
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void PedalComponent::showTypePopup()
{
    juce::PopupMenu menu;

    struct Cat { const char* name; std::vector<int> types; };
    Cat cats[] = {
        {"Compression",  {5}},
        {"Delay",        {10, 11}},
        {"Distortion",   {1, 8}},
        {"Filter",       {3, 9, 20}},
        {"Glitch",       {6, 16, 18}},
        {"Modulation",   {2, 13, 17, 22}},
        {"Pitch",        {4, 14, 19}},
        {"Resonance",    {15}},
        {"Reverb",       {7, 12, 21}},
    };

    for (auto& cat : cats)
    {
        menu.addSectionHeader(cat.name);
        for (int t : cat.types)
        {
            auto type = static_cast<DspModuleType>(t);
            juce::String label = "    ";
            label += PedalDefinitions::getDisplayName(type);
            menu.addItem(t + 1, label, true, type == m_currentType);
        }
    }

    menu.addSeparator();
    menu.addItem(1, "    Bypass", true, m_currentType == DspModuleType::BYPASS);

    SafePointer<PedalComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options(),
        [safeThis](int result)
        {
            if (safeThis == nullptr) return;
            if (result > 0)
            {
                auto type = static_cast<DspModuleType>(result - 1);
                safeThis->m_currentType = type;
                safeThis->m_definition = &PedalDefinitions::get(type);
                safeThis->repaint();
                safeThis->audioProcessor.setPedalSlot(safeThis->m_slotIndex, type);
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
        m_knobs[static_cast<size_t>(knobIdx)]->setValue(value);
}

void PedalComponent::updateKnobBounds()
{
    if (m_definition == nullptr)
        return;

    auto pedalBounds = getLocalBounds().toFloat();
    const float pedalWidth = pedalBounds.getWidth();
    const float pedalHeight = pedalBounds.getHeight();

    const float knobSize = pedalHeight * GridLayout::KnobSizeRatio;
    const float halfKnob = knobSize * 0.5f;

    float minCentreX = 1.0f, maxCentreX = 0.0f;
    for (int i = 0; i < kKnobCount; ++i)
    {
        minCentreX = juce::jmin(minCentreX, m_definition->knobLayout[static_cast<size_t>(i)].centreX);
        maxCentreX = juce::jmax(maxCentreX, m_definition->knobLayout[static_cast<size_t>(i)].centreX);
    }
    const float currentSpreadX = (maxCentreX - minCentreX) * pedalWidth;
    const float groupCenterX = pedalBounds.getX() + pedalWidth * (minCentreX + maxCentreX) / 2.0f;
    const float targetSpreadX = pedalWidth * GridLayout::KnobSpreadRatio;
    const float scaleX = targetSpreadX / currentSpreadX;

    for (int i = 0; i < kKnobCount; ++i)
    {
        const auto& normBounds = m_definition->knobLayout[static_cast<size_t>(i)];

        const float centerX = groupCenterX + (normBounds.centreX - (minCentreX + maxCentreX) / 2.0f) * scaleX * pedalWidth;
        const float centerY = pedalBounds.getY() + (normBounds.centreY + GridLayout::KnobCenterYShiftRatio) * pedalHeight;

        m_knobBounds[static_cast<size_t>(i)] = juce::Rectangle<float>(
            centerX - halfKnob,
            centerY - halfKnob,
            knobSize,
            knobSize);
    }
}

juce::Point<float> PedalComponent::getInputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getX() + bounds.getWidth() * GridLayout::JackInsetXRatio,
             bounds.getY() + bounds.getHeight() * GridLayout::JackInsetYRatio };
}

juce::Point<float> PedalComponent::getOutputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getRight() - bounds.getWidth() * GridLayout::JackInsetXRatio,
             bounds.getY() + bounds.getHeight() * GridLayout::JackInsetYRatio };
}
