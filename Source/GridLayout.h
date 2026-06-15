#pragma once

#include <cstdint>

namespace GridLayout
{
constexpr int RowCount = 2;
constexpr int ColCount = 3;

namespace DesignResolution
{
    constexpr int Width = 1400;
    constexpr int Height = 800;
}

constexpr float GridSidePaddingRatio = 0.01f;
constexpr float GridTopPaddingRatio = 0.0365f;

constexpr float PedalShrinkRatio = 0.95f;
constexpr float ColumnGapRatio = 0.005f;
constexpr float RowGapRatio = 0.06f;
constexpr float VerticalGroupOffsetRatio = 0.0375f;

constexpr float PedalWidthMinRatio = 0.18f;
constexpr float PedalWidthMaxRatio = 0.28f;
constexpr float PedalHeightMinRatio = 0.25f;
constexpr float PedalHeightMaxRatio = 0.39f;

constexpr float CanvasOuterMarginRatio = 0.016f;
constexpr float CanvasVerticalMarginRatio = 0.031f;
constexpr float PaletteHeightRatio = 0.26f;

constexpr float CanvasCenterXShiftRatio = 0.017f;
constexpr float CanvasCenterYShiftRatio = 0.051f;

constexpr float KnobSizeRatio = 0.16f;
constexpr float KnobSpreadRatio = 0.45f;
constexpr float KnobCenterYShiftRatio = -0.07f;
constexpr float JackInsetXRatio = 0.21f;
constexpr float JackInsetYRatio = 0.05f;
constexpr float KnobLabelOffsetYRatio = 0.10f;

constexpr float LabelInsetXRatio = 0.02f;
constexpr float LabelInsetYRatio = 0.035f;
constexpr float LabelTopTrimRatio = 0.023f;
constexpr float LabelReducedXRatio = 0.10f;
constexpr float LabelReducedYRatio = 0.038f;

constexpr float LedCenterXRatio = 0.5f;
constexpr float LedCenterYRatio = 0.185f;
constexpr float LedSizeRatio = 0.06f;

constexpr float BlobMaxSizeRatio = 0.40f;
constexpr float ButtonWidthRatio = 0.22f;
constexpr float ButtonHeightRatio = 0.22f;

constexpr float CellOverdrawRatio = 1.02f;
}
