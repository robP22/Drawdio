#include "CableRenderer.h"
#include "RenderUtils.h"

CableRenderer::CableRenderer(const IThemeProvider& theme)
    : m_theme(theme) {}

void CableRenderer::renderSegment(juce::Graphics& g,
                                  const juce::Path& left, const juce::Path& right,
                                  juce::Colour leftBase, juce::Colour rightBase)
{
    auto shadow = left;
    shadow.addPath(right);
    shadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
    RenderUtils::strokeCable(g, shadow, juce::Colours::black.withAlpha(0.45f), 8.0f);
    if (!left.isEmpty())
    {
        RenderUtils::strokeCable(g, left, leftBase.darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, left, leftBase, 4.8f);
        RenderUtils::strokeCable(g, left, juce::Colours::white.withAlpha(0.14f), 1.4f);
    }
    if (!right.isEmpty())
    {
        RenderUtils::strokeCable(g, right, rightBase.darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, right, rightBase, 4.8f);
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
        renderSegment(g, cable.left, cable.right,
                      m_theme.cableOutColour(), m_theme.cableInColour());
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

    RenderUtils::strokeCable(g, split.left, m_theme.cableOutColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.left, juce::Colours::white.withAlpha(0.16f), 1.2f);
    RenderUtils::strokeCable(g, split.right, m_theme.cableInColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.right, juce::Colours::white.withAlpha(0.16f), 1.2f);
}

void CableRenderer::drawGrabbedCable(juce::Graphics& g,
                                     juce::Point<float> fromPos, juce::Point<float> toPos) const
{
    auto cps = CablePathBuilder::makeSameRowControlPoints(fromPos, toPos);
    auto split = CablePathBuilder::splitCubicBezier(fromPos, cps.first, cps.second, toPos);
    renderSegment(g, split.left, split.right,
                  m_theme.cableOutColour(), m_theme.cableInColour());
}

void CableRenderer::drawInputJack(juce::Graphics& g, juce::Point<float> entryPos,
                                  const juce::Path& path) const
{
    constexpr float jackR = 7.0f;

    g.setColour(juce::Colours::dimgrey);
    g.fillEllipse(entryPos.x - jackR, entryPos.y - jackR, jackR * 2.0f, jackR * 2.0f);
    g.setColour(juce::Colours::silver);
    g.drawEllipse(entryPos.x - jackR, entryPos.y - jackR, jackR * 2.0f, jackR * 2.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("IN", entryPos.x + jackR + 4.0f, entryPos.y - 9.0f, 30.0f, 18.0f,
               juce::Justification::centredLeft, false);

    if (path.isEmpty())
        return;

    juce::Path empty;
    renderSegment(g, empty, path,
                  juce::Colours::transparentBlack, m_theme.cableInColour());
}

void CableRenderer::drawOutputJack(juce::Graphics& g, juce::Point<float> exitPos,
                                   const juce::Path& path) const
{
    constexpr float jackR = 7.0f;

    g.setColour(juce::Colours::dimgrey);
    g.fillEllipse(exitPos.x - jackR, exitPos.y - jackR, jackR * 2.0f, jackR * 2.0f);
    g.setColour(juce::Colours::silver);
    g.drawEllipse(exitPos.x - jackR, exitPos.y - jackR, jackR * 2.0f, jackR * 2.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("OUT", exitPos.x - jackR - 30.0f - 4.0f, exitPos.y - 9.0f, 30.0f, 18.0f,
               juce::Justification::centredRight, false);

    if (path.isEmpty())
        return;

    juce::Path empty;
    renderSegment(g, path, empty,
                  m_theme.cableOutColour(), juce::Colours::transparentBlack);
}
