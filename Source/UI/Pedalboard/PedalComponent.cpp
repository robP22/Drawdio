#include "UI/Pedalboard/PedalComponent.h"
#include "Core/EditorDesignMetrics.h"
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

PedalComponent::PedalComponent(int slotIndex,
                               DspModuleType initialType,
                               const IResourceProvider& resources,
                               const ScaledAssetProvider& assets,
                               const IThemeProvider& theme,
                               Actions actions)
    : m_resources(resources),
      m_assets(assets),
      m_theme(theme),
      m_actions(std::move(actions)),
      m_slotIndex(slotIndex),
      m_currentType(initialType),
      m_definition(&PedalDefinitions::get(initialType))
{
    for (int i = 0; i < kKnobCount; ++i)
    {
        m_knobs[i] = std::make_unique<SpriteKnob>(m_assets, IResourceProvider::ImageId::PedalKnobImage, 0.0f, 1.0f);
        m_knobs[i]->setValue(m_definition->parameters[static_cast<size_t>(i)].param.defaultValue);
        m_knobs[i]->setLinked(false);
        m_knobs[i]->setLinkRange(0.0f, 1.0f);
        m_linkMins[static_cast<size_t>(i)] = 0.0f;
        m_linkMaxs[static_cast<size_t>(i)] = 1.0f;
        m_knobs[i]->onDragStart = [this, i](float v) { onKnobDragStart(i, v); };
        m_knobs[i]->onValueChanged = [this, i](float v) { onKnobValueChanged(i, v); };
        m_knobs[i]->onRightClick = [this, i]() { onKnobRightClick(i); };
        m_knobs[i]->onLinkRangeChanged = [this, i](float rMin, float rMax)
        {
            m_linkMins[static_cast<size_t>(i)] = rMin;
            m_linkMaxs[static_cast<size_t>(i)] = rMax;
            if (m_actions.setLinkRange) m_actions.setLinkRange(m_slotIndex, i, rMin, rMax);
            repaint();
        };
        const auto& param = m_definition->parameters[static_cast<size_t>(i)];
        m_knobs[i]->setVisible(param.label != nullptr && param.label[0] != '\0');
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

    const float insetX = pedalW * EditorDesignMetrics::PedalBodyInsetXRatio;
    const float insetY = pedalH * EditorDesignMetrics::PedalBodyInsetYRatio;
    const float maskW  = pedalW * EditorDesignMetrics::PedalBodyMaskWRatio;
    const float maskH  = pedalH * EditorDesignMetrics::PedalBodyMaskHRatio;
    const float corner = pedalW * EditorDesignMetrics::PedalBodyCornerRatio;

    juce::Path pedalClipPath;
    pedalClipPath.addRoundedRectangle(insetX, insetY, maskW, maskH, corner);

    {
        m_assets.drawImage(g, IResourceProvider::ImageId::PedalEnclosure,
                           bounds, ScaledAssetProvider::ResamplingPolicy::Continuous);

        const auto srcRect = japTextureCell(m_slotIndex % 6);
        g.saveState();
        g.reduceClipRegion(pedalClipPath);
        g.setOpacity(0.85f);
        m_assets.drawFrame(g, IResourceProvider::ImageId::JapanesePedalSheet,
                           srcRect, bounds, ScaledAssetProvider::ResamplingPolicy::Continuous);
        g.restoreState();

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
        const float deviceScale = ledDeviceScale;
        const int ledPx = juce::jmax(1, juce::roundToInt(pedalH * EditorDesignMetrics::LedSizeRatio * deviceScale));

        if (ledPx != m_ledScaledSize || !m_ledScaled[0].isValid() || !m_ledScaled[1].isValid())
        {
            const int frameW = ledImage.getWidth() / 2;
            const int frameH = ledImage.getHeight();
            m_ledScaled[0] = m_assets.getScaledFrame(
                IResourceProvider::ImageId::PedalLedImage,
                { 0, 0, frameW, frameH }, ledPx, ledPx,
                ScaledAssetProvider::ResamplingPolicy::Continuous);
            m_ledScaled[1] = m_assets.getScaledFrame(
                IResourceProvider::ImageId::PedalLedImage,
                { frameW, 0, frameW, frameH }, ledPx, ledPx,
                ScaledAssetProvider::ResamplingPolicy::Continuous);
            m_ledScaledSize = m_ledScaled[0].isValid() ? m_ledScaled[0].getWidth() : 0;
        }

        if (m_ledScaledSize > 0 && m_ledScaled[0].isValid() && m_ledScaled[1].isValid())
        {
            const float destSize = static_cast<float>(m_ledScaledSize) / deviceScale;
            const float x = std::round((pedalW * EditorDesignMetrics::LedCenterXRatio - destSize * 0.5f) * deviceScale) / deviceScale;
            const float y = std::round((pedalH * EditorDesignMetrics::LedCenterYRatio - destSize * 0.5f) * deviceScale) / deviceScale;

            const bool isOn = (m_currentType != DspModuleType::BYPASS);
            g.saveState();
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(m_ledScaled[isOn ? 1 : 0],
                        juce::Rectangle<float>(x, y, destSize, destSize));
            g.restoreState();
        }
    }

    const auto labelArea = getLabelArea();
    if (!labelArea.isEmpty())
    {
        g.setGradientFill(juce::ColourGradient(
            m_theme.pedalLcdTop(), labelArea.getTopLeft(),
            m_theme.pedalLcdBottom(), labelArea.getBottomLeft(), false));
        g.fillRoundedRectangle(labelArea, m_theme.pedalStyle().lcdRadius);

        g.setColour(juce::Colour(0xFFE8F1F5));
        g.setFont(m_lcdFont);
        g.drawFittedText(m_definition ? juce::String(m_definition->displayName) : "---",
                         labelArea.toNearestInt().reduced(2, 0),
                         juce::Justification::centred, 2, 0.85f);

        g.setGradientFill(juce::ColourGradient(
            juce::Colours::black.withAlpha(0.35f), labelArea.getTopLeft(),
            juce::Colours::black.withAlpha(0.10f), labelArea.getBottomLeft(), false));
        g.fillRoundedRectangle(labelArea, m_theme.pedalStyle().lcdRadius);
    }
    
    juce::ignoreUnused(g);

    if (m_definition == nullptr)
        return;

    const float fontSize = pedalH * EditorDesignMetrics::KnobFontSizeRatio;
    const float labelWidth = pedalW * EditorDesignMetrics::KnobLabelWidthRatio;
    const float labelHeight = fontSize * 1.3f;
    const float offsetY = pedalH * EditorDesignMetrics::KnobLabelOffsetYRatio;

    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(m_labelFont);

    for (int i = 0; i < kKnobCount; ++i)
    {
        const auto& knobBounds = m_knobBounds[static_cast<size_t>(i)];
        const auto& param = m_definition->parameters[static_cast<size_t>(i)];
        if (param.label == nullptr || param.label[0] == '\0')
            continue;

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
    m_labelFont = juce::Font(juce::FontOptions(std::max(6.0f, pedalH * EditorDesignMetrics::KnobFontSizeRatio)));

    applyKnobLayout();
}

void PedalComponent::applyKnobLayout()
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
                      .reduced(pedalW * EditorDesignMetrics::LabelInsetXRatio, pedalH * EditorDesignMetrics::LabelInsetYRatio);
    bounds.removeFromTop(pedalH * EditorDesignMetrics::LabelTopTrimRatio);
    return bounds.withTrimmedTop(bounds.getHeight() * 0.67f)
                .reduced(pedalW * EditorDesignMetrics::LabelReducedXRatio, pedalH * EditorDesignMetrics::LabelReducedYRatio);
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
        {"Filter",       {3, 9, 20, 22}},
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
                safeThis->applyKnobLayout();
                safeThis->repaint();
                if (safeThis->m_actions.setType)
                    safeThis->m_actions.setType(safeThis->m_slotIndex, type);
            }
        });
}

void PedalComponent::syncType(DspModuleType type)
{
    if (type != m_currentType)
    {
        m_currentType = type;
        m_definition = &PedalDefinitions::get(type);
        applyKnobLayout();
        repaint();
    }
}

void PedalComponent::setKnobLinked(int knobIdx, bool linked)
{
    if (knobIdx < 0 || knobIdx >= kKnobCount)
        return;
    m_linked[static_cast<size_t>(knobIdx)] = linked;
    if (m_knobs[static_cast<size_t>(knobIdx)])
        m_knobs[static_cast<size_t>(knobIdx)]->setLinked(linked);
    repaint();
}

void PedalComponent::setKnobLinkRange(int knobIdx, float rMin, float rMax)
{
    if (knobIdx < 0 || knobIdx >= kKnobCount)
        return;
    rMin = std::clamp(rMin, 0.0f, 1.0f);
    rMax = std::clamp(rMax, 0.0f, 1.0f);
    if (rMax < rMin + 0.05f) rMax = std::min(1.0f, rMin + 0.05f);
    if (rMin > rMax - 0.05f) rMin = std::max(0.0f, rMax - 0.05f);
    m_linkMins[static_cast<size_t>(knobIdx)] = rMin;
    m_linkMaxs[static_cast<size_t>(knobIdx)] = rMax;
    if (m_knobs[static_cast<size_t>(knobIdx)])
        m_knobs[static_cast<size_t>(knobIdx)]->setLinkRange(rMin, rMax);
    repaint();
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
    const float snapped = PedalDefinitions::snapValue(m_currentType, knobIdx, value);
    if (snapped != value)
        m_knobs[static_cast<size_t>(knobIdx)]->setValue(snapped);
    if (m_actions.setKnob)
        m_actions.setKnob(m_slotIndex, knobIdx, m_knobDragStartValues[knobIdx], snapped);
}

void PedalComponent::onKnobRightClick(int knobIdx)
{
    bool linked = m_linked[static_cast<size_t>(knobIdx)];
    juce::Component::SafePointer<PedalComponent> safeThis(this);
    juce::PopupMenu menu;
    menu.addItem("Link to Automation", !linked, linked,
        [safeThis, knobIdx]()
        {
            if (safeThis == nullptr)
                return;
            safeThis->m_linked[static_cast<size_t>(knobIdx)] = true;
            if (safeThis->m_knobs[static_cast<size_t>(knobIdx)])
                safeThis->m_knobs[static_cast<size_t>(knobIdx)]->setLinked(true);
            if (safeThis->m_actions.setLink) safeThis->m_actions.setLink(safeThis->m_slotIndex, knobIdx, true);
            safeThis->repaint();
        });
    menu.addItem("Unlink from Automation", linked, !linked,
        [safeThis, knobIdx]
        {
            if (safeThis == nullptr)
                return;
            safeThis->m_linked[static_cast<size_t>(knobIdx)] = false;
            if (safeThis->m_knobs[static_cast<size_t>(knobIdx)])
                safeThis->m_knobs[static_cast<size_t>(knobIdx)]->setLinked(false);
            if (safeThis->m_actions.setLink) safeThis->m_actions.setLink(safeThis->m_slotIndex, knobIdx, false);
            safeThis->repaint();
        });
    menu.showMenuAsync(juce::PopupMenu::Options());
}

void PedalComponent::updateKnobBounds()
{
    if (m_definition == nullptr)
        return;

    auto pedalBounds = getLocalBounds().toFloat();
    const float pedalWidth = pedalBounds.getWidth();
    const float pedalHeight = pedalBounds.getHeight();

    const float knobSize = pedalHeight * EditorDesignMetrics::KnobSizeRatio;
    const float halfKnob = knobSize * 0.5f;

    const auto layout = knobLayoutForCount(m_definition->knobCount);
    float minCentreX = 1.0f, maxCentreX = 0.0f;
    int slot = 0;
    for (int i = 0; i < kKnobCount; ++i)
    {
        const auto& param = m_definition->parameters[static_cast<size_t>(i)];
        if (param.label == nullptr || param.label[0] == '\0')
            continue;
        minCentreX = juce::jmin(minCentreX, layout[static_cast<size_t>(slot)].centreX);
        maxCentreX = juce::jmax(maxCentreX, layout[static_cast<size_t>(slot)].centreX);
        ++slot;
    }
    if (slot == 0)
    {
        m_knobBounds = {};
        for (int i = 0; i < kKnobCount; ++i)
            if (m_knobs[static_cast<size_t>(i)])
                m_knobs[static_cast<size_t>(i)]->setVisible(false);
        return;
    }

    const float currentSpreadX = (maxCentreX - minCentreX) * pedalWidth;
    const float groupCenterX = pedalBounds.getX() + pedalWidth * (minCentreX + maxCentreX) / 2.0f;
    float scaleX = 1.0f;
    if (currentSpreadX >= 0.001f)
        scaleX = (pedalWidth * EditorDesignMetrics::KnobSpreadRatio) / currentSpreadX;

    slot = 0;
    for (int i = 0; i < kKnobCount; ++i)
    {
        const auto& param = m_definition->parameters[static_cast<size_t>(i)];
        if (param.label == nullptr || param.label[0] == '\0')
        {
            m_knobBounds[static_cast<size_t>(i)] = {};
            if (m_knobs[static_cast<size_t>(i)])
                m_knobs[static_cast<size_t>(i)]->setVisible(false);
            continue;
        }

        const auto& normBounds = layout[static_cast<size_t>(slot)];
        const float centerX = groupCenterX + (normBounds.centreX - (minCentreX + maxCentreX) / 2.0f) * scaleX * pedalWidth;
        const float centerY = pedalBounds.getY() + (normBounds.centreY + EditorDesignMetrics::KnobCenterYShiftRatio) * pedalHeight;

        m_knobBounds[static_cast<size_t>(i)] = juce::Rectangle<float>(
            centerX - halfKnob,
            centerY - halfKnob,
            knobSize,
            knobSize);
        if (m_knobs[static_cast<size_t>(i)])
            m_knobs[static_cast<size_t>(i)]->setVisible(true);
        ++slot;
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
    return { bounds.getX() + bounds.getWidth() * EditorDesignMetrics::JackInsetXRatio,
             bounds.getY() + bounds.getHeight() * EditorDesignMetrics::JackInsetYRatio };
}

juce::Point<float> PedalComponent::getOutputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getRight() - bounds.getWidth() * EditorDesignMetrics::JackInsetXRatio,
             bounds.getY() + bounds.getHeight() * EditorDesignMetrics::JackInsetYRatio };
}
