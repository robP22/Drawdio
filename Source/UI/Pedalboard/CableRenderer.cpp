#include "CableRenderer.h"
#include "RenderUtils.h"

CableRenderer::CableRenderer(const IThemeProvider& theme, const IResourceProvider& resources)
    : m_theme(theme), m_resources(resources) {}

void CableRenderer::renderSegment(juce::Graphics& g,
                                  const juce::Path& left, const juce::Path& right,
                                  juce::Colour base)
{
    auto shadow = left;
    shadow.addPath(right);
    shadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
    RenderUtils::strokeCable(g, shadow, juce::Colours::black.withAlpha(0.30f), 8.0f);
    if (!left.isEmpty())
    {
        RenderUtils::strokeCable(g, left, base.darker(0.18f), 5.8f);
        RenderUtils::strokeCable(g, left, base, 4.8f);
        RenderUtils::strokeCable(g, left, juce::Colours::white.withAlpha(0.14f), 1.4f);
    }
    if (!right.isEmpty())
    {
        RenderUtils::strokeCable(g, right, base.darker(0.18f), 5.8f);
        RenderUtils::strokeCable(g, right, base, 4.8f);
        RenderUtils::strokeCable(g, right, juce::Colours::white.withAlpha(0.14f), 1.4f);
    }
}

void CableRenderer::drawRoutingCables(juce::Graphics& g,
                                      const std::vector<CachedSplitCable>& cables,
                                      int skipGrabbedIndex) const
{
    for (size_t i = 0; i < cables.size(); ++i)
    {
        if (skipGrabbedIndex >= 0 && static_cast<int>(i) == skipGrabbedIndex)
            continue;

        const auto& cable = cables[i];
        renderSegment(g, cable.left, cable.right, m_theme.cableColour());
    }
}

void CableRenderer::drawActiveDraggingCable(juce::Graphics& g,
                                            juce::Point<float> start, juce::Point<float> current,
                                            int srcJackIdx) const
{
    const float horizontal = std::abs(current.x - start.x);
    juce::Point<float> cp1, cp2;

    float curveX = std::max(horizontal * 0.35f, 20.0f);
    float lift = std::min(horizontal * 0.04f + 4.0f, 15.0f);
    cp1 = {start.x + curveX, start.y - lift};
    cp2 = {current.x - curveX, current.y - lift};

    auto split = CablePathBuilder::splitCubicBezier(start, cp1, cp2, current);

    RenderUtils::strokeCable(g, split.left, m_theme.cableColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.left, juce::Colours::white.withAlpha(0.16f), 1.2f);
    RenderUtils::strokeCable(g, split.right, m_theme.cableColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.right, juce::Colours::white.withAlpha(0.16f), 1.2f);
}

void CableRenderer::drawGrabbedCable(juce::Graphics& g,
                                     juce::Point<float> fromPos, juce::Point<float> toPos) const
{
    auto cps = CablePathBuilder::makeSameRowControlPoints(fromPos, toPos);
    auto split = CablePathBuilder::splitCubicBezier(fromPos, cps.first, cps.second, toPos);
    renderSegment(g, split.left, split.right, m_theme.cableColour());
}

void CableRenderer::drawInputJack(juce::Graphics& g, juce::Point<float> entryPos,
                                  const juce::Path& path) const
{
    drawJack(g, entryPos, path, true);
}

void CableRenderer::drawOutputJack(juce::Graphics& g, juce::Point<float> exitPos,
                                    const juce::Path& path) const
{
    drawJack(g, exitPos, path, false);
}

void CableRenderer::drawJackHighlight(juce::Graphics& g, juce::Point<float> position) const
{
    const juce::Colour colour = m_theme.jackHighlightColour();
    g.saveState();
    g.setColour(colour.withAlpha(0.22f));
    g.fillEllipse(position.x - 20.0f, position.y - 20.0f, 40.0f, 40.0f);
    g.setColour(colour.withAlpha(0.95f));
    g.drawEllipse(position.x - 15.0f, position.y - 15.0f, 30.0f, 30.0f, 2.5f);
    g.restoreState();
}

void CableRenderer::drawJack(juce::Graphics& g, juce::Point<float> position,
                             const juce::Path& path, bool isInput) const
{
    static constexpr float jackH = 14.0f;
    float jackW = jackH;

    if (!path.isEmpty())
    {
        juce::Path empty;
        renderSegment(g, isInput ? empty : path,
                      isInput ? path : empty,
                      m_theme.cableColour());
    }

    const auto& tex = m_resources.getTexture(IResourceProvider::TextureId::InputJack);
    if (tex.isValid())
    {
        const float aspect = static_cast<float>(tex.getWidth()) / static_cast<float>(tex.getHeight());
        jackW = jackH * aspect;
        const float scale = jackH / static_cast<float>(tex.getHeight());
        const float signedScaleX = isInput ? scale : -scale;
        const float offsetX = position.x + (isInput ? -jackW : jackW) * 0.5f;
        const float offsetY = position.y - jackH * 0.5f;

        g.saveState();
        g.setColour(juce::Colours::white);
        g.setOpacity(1.0f);
        g.drawImageTransformed(
            tex,
            juce::AffineTransform::scale(signedScaleX, scale).translated(offsetX, offsetY));
        g.restoreState();
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText(isInput ? "IN" : "OUT",
               position.x + (isInput ? jackW * 0.5f + 4.0f : -jackW * 0.5f - 34.0f),
               position.y - 9.0f, 30.0f, 18.0f,
               isInput ? juce::Justification::centredLeft
                       : juce::Justification::centredRight,
               false);
}
