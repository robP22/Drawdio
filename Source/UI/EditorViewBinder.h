#pragma once

#include "UI/EditorState.h"

class PedalboardGrid;
class BottomControlBar;
class PedalboardHeader;
class PixelCanvasComponent;
class ColorPalette;

class EditorViewBinder
{
public:
    EditorViewBinder(PedalboardGrid& pedalboardGrid,
                     BottomControlBar& bottomBar,
                     PedalboardHeader& pedalboardHeader,
                     PixelCanvasComponent& pixelCanvas,
                     ColorPalette& palette);

    void apply(const EditorUiSnapshot& state);

private:
    PedalboardGrid& m_pedalboardGrid;
    BottomControlBar& m_bottomBar;
    PedalboardHeader& m_pedalboardHeader;
    PixelCanvasComponent& m_pixelCanvas;
    ColorPalette& m_palette;
};
