#include "UI/Pedalboard/PedalComponent.h"
#include "GridLayout.h"
#include "RenderUtils.h"


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

PedalComponent::PedalComponent(IPedalComponentModel& model,
                               int slotIndex,
                               DspModuleType initialType,
                               const IResourceProvider& resources,
                               const IThemeProvider& theme)
    : m_model(model),
      m_resources(resources),
      m_theme(theme),
      m_slotIndex(slotIndex),
      m_currentType(initialType),
      m_definition(&PedalDefinitions::get(initialType))
{
    setBufferedToImage(true);
    const auto& knobImg = m_resources.getImage(IResourceProvider::ImageId::PedalKnobImage);
    for (int i = 0; i < kKnobCount; ++i)
    {
        m_knobs[i] = std::make_unique<SpriteKnob>(knobImg, 0.0f, 1.0f);
        m_knobs[i]->setValue(m_definition->parameters[static_cast<size_t>(i)].defaultValue);
        m_knobs[i]->onDragStart = [this, i](float v) { onKnobDragStart(i, v); };
        m_knobs[i]->onValueChanged = [this, i](float v) { onKnobValueChanged(i, v); };
        m_knobs[i]->onRightClick = [this, i]() { onKnobRightClick(i); };
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

    juce::Path pedalClipPath;
    pedalClipPath.addRoundedRectangle(insetX, insetY, maskW, maskH, corner);

    {
        const auto& enclosure = m_resources.getTexture(IResourceProvider::TextureId::PedalEnclosure);
        if (enclosure.isValid())
            g.drawImage(enclosure, 0, 0, pedalW, pedalH, 0, 0, enclosure.getWidth(), enclosure.getHeight());

        const auto& japSheet = m_resources.getTexture(IResourceProvider::TextureId::JapanesePedalSheet);
        if (japSheet.isValid())
        {
            const auto srcRect = japTextureCell(m_slotIndex % 6);
            g.saveState();
            g.reduceClipRegion(pedalClipPath);
            g.setOpacity(0.85f);
            g.drawImage(japSheet, 0, 0, pedalW, pedalH, srcRect.getX(), srcRect.getY(), srcRect.getWidth(), srcRect.getHeight());
            g.restoreState();
        }

        {
            g.saveState();
            g.reduceClipRegion(pedalClipPath);
            juce::Rectangle<float> body(insetX, insetY, maskW, maskH);
            RenderUtils::paintCurvatureVignette(g, body, corner);
            RenderUtils::paintEdgeHighlight(g, body, corner);
            g.setOpacity(0.03f);
            g.drawImage(RenderUtils::getNoiseTexture(), body.getX(), body.getY(), body.getWidth(), body.getHeight(), 0, 0, 128, 128);
            g.setOpacity(1.0f);
            g.restoreState();
        }
    }

    const auto& ledImage = m_resources.getImage(IResourceProvider::ImageId::PedalLedImage);
    const float ledDeviceScale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (ledImage.isValid() && ledDeviceScale > 0.0f)
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        const float deviceScale = ledDeviceScale;
        const int ledPx = juce::jmax(1, juce::roundToInt(pedalH * GridLayout::LedSizeRatio * deviceScale));

        if (ledPx != m_ledScaledSize || !m_ledScaled[0].isValid() || !m_ledScaled[1].isValid())
        {
            const int frameW = ledImage.getWidth() / 2;
            const int frameH = ledImage.getHeight();
            m_ledScaled[0] = ledImage.getClippedImage({ 0, 0, frameW, frameH })
                                 .rescaled(ledPx, ledPx, juce::Graphics::highResamplingQuality);
            m_ledScaled[1] = ledImage.getClippedImage({ frameW, 0, frameW, frameH })
                                 .rescaled(ledPx, ledPx, juce::Graphics::highResamplingQuality);
            m_ledScaledSize = ledPx;
        }

        if (m_ledScaledSize > 0 && m_ledScaled[0].isValid() && m_ledScaled[1].isValid())
        {
            const float destSize = static_cast<float>(m_ledScaledSize) / deviceScale;
            const float x = std::round((pedalW * GridLayout::LedCenterXRatio - destSize * 0.5f) * deviceScale) / deviceScale;
            const float y = std::round((pedalH * GridLayout::LedCenterYRatio - destSize * 0.5f) * deviceScale) / deviceScale;

            const bool isOn = (m_currentType != DspModuleType::BYPASS);
            g.drawImage(m_ledScaled[isOn ? 1 : 0],
                        juce::Rectangle<float>(x, y, destSize, destSize));
        }
    }

    const auto labelArea = getLabelArea();
    if (!labelArea.isEmpty())
    {
        g.setGradientFill(juce::ColourGradient(
            m_theme.pedalLcdTop(), labelArea.getTopLeft(),
            m_theme.pedalLcdBottom(), labelArea.getBottomLeft(), false));
        g.fillRoundedRectangle(labelArea, m_theme.pedalStyle().lcdRadius);

        g.setColour(juce::Colour(0xFFC8E0E8));
        g.setFont(m_lcdFont);
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
        if (m_model.isKnobLinked(m_slotIndex, i))
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
    g.setFont(m_labelFont);

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
    const float pedalH = static_cast<float>(getHeight());
    m_lcdFont = juce::Font(juce::FontOptions(std::max(8.0f, getLabelArea().getHeight() * 0.35f)));
    m_lcdFont.setTypefaceName(juce::Font::getDefaultMonospacedFontName());
    m_labelFont = juce::Font(juce::FontOptions(std::max(6.0f, pedalH * GridLayout::KnobFontSizeRatio)));

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
        {"Delay",        {11}},
        {"Distortion",   {1, 8}},
        {"Filter",       {3, 9, 20}},
        {"Glitch",       {6, 16, 18}},
        {"Time",         {10}},
        {"Modulation",   {2, 13, 17, 24, 25}},
        {"Pitch",        {4, 14, 19}},
        {"Resonance",    {15}},
        {"Reverb",       {7, 12, 21}},
        {"Resampling",   {23}},
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
                safeThis->m_model.setPedalSlot(safeThis->m_slotIndex, type);
            }
        });
}

void PedalComponent::syncFromProcessor()
{
    auto type = m_model.getPedalSlot(m_slotIndex);
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

void PedalComponent::onKnobDragStart(int knobIdx, float value)
{
    m_knobDragStartValues[knobIdx] = value;
}

void PedalComponent::onKnobValueChanged(int knobIdx, float value)
{
    m_model.setKnobParameter(m_slotIndex, knobIdx, m_knobDragStartValues[knobIdx], value);
}

void PedalComponent::onKnobRightClick(int knobIdx)
{
    bool linked = m_model.isKnobLinked(m_slotIndex, knobIdx);
    juce::PopupMenu menu;
    menu.addItem("Link to Automation", !linked, linked,
        [this, knobIdx]() { m_model.setKnobLink(m_slotIndex, knobIdx, true); repaint(); });
    menu.addItem("Unlink from Automation", linked, !linked,
        [this, knobIdx]() { m_model.setKnobLink(m_slotIndex, knobIdx, false); repaint(); });
    menu.showMenuAsync(juce::PopupMenu::Options());
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
    if (currentSpreadX < 0.001f)
        return;
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

juce::Rectangle<int> PedalComponent::getBounds() const
{
    return juce::Component::getBounds();
}

void PedalComponent::setBounds(juce::Rectangle<int> r)
{
    juce::Component::setBounds(r);
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
