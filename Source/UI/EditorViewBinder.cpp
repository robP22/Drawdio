#include "EditorViewBinder.h"

#include "UI/Canvas/ColorPalette.h"
#include "UI/Canvas/PixelCanvasComponent.h"
#include "UI/Controls/BottomControlBar.h"
#include "UI/Pedalboard/PedalboardHeader.h"
#include "PedalboardGrid.h"

EditorViewBinder::EditorViewBinder(PedalboardGrid& pedalboardGrid,
                                   BottomControlBar& bottomBar,
                                   PedalboardHeader& pedalboardHeader,
                                   PixelCanvasComponent& pixelCanvas,
                                   ColorPalette& palette)
    : m_pedalboardGrid(pedalboardGrid),
      m_bottomBar(bottomBar),
      m_pedalboardHeader(pedalboardHeader),
      m_pixelCanvas(pixelCanvas),
      m_palette(palette)
{
}

void EditorViewBinder::apply(const EditorUiSnapshot& state)
{
    m_pedalboardGrid.setViewState(state);
    m_bottomBar.setViewState(state);
    m_pedalboardHeader.updateModeButton(state.manualMode);
    m_palette.setSelectedColor(state.session.selectedColour);
    if (!m_pixelCanvas.isStrokeOpen())
    {
        const auto color = m_palette.isEraserActive()
            ? PixelCanvasComponent::PixelColor::Transparent
            : static_cast<PixelCanvasComponent::PixelColor>(state.session.selectedColour);
        m_pixelCanvas.setCurrentColor(color);
    }
}
