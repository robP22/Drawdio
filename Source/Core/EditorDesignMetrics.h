#pragma once

namespace EditorDesignMetrics
{
constexpr int RowCount = 2;
constexpr int ColCount = 3;

namespace DesignResolution
{
constexpr int Width = 1400;
constexpr int Height = 900;
constexpr int MinimumWidth = 1050;
constexpr int MinimumHeight = 675;
constexpr int MaximumWidth = 1750;
constexpr int MaximumHeight = 1125;
}

constexpr float GridSidePaddingRatio = 0.01f;
constexpr float GridTopPaddingRatio = 0.0365f;
constexpr float PedalShrinkRatio = 0.95f;
constexpr float ColumnGapRatio = 0.015f;
constexpr float RowGapRatio = 0.060f;
constexpr float VerticalGroupOffsetRatio = 0.025f;
constexpr float PedalWidthMinRatio = 0.18f;
constexpr float PedalWidthMaxRatio = 0.28f;
constexpr float PedalHeightMinRatio = 0.25f;
constexpr float PedalHeightMaxRatio = 0.39f;
constexpr float PaletteHeightRatio = 0.26f;
constexpr float PaletteColumnInsetRatio = 0.05f;
constexpr float CanvasScaleRatio = 0.90f;
constexpr float KnobSizeRatio = 0.16f;
constexpr float KnobSpreadRatio = 0.35f;
constexpr float KnobCenterYShiftRatio = -0.02f;
constexpr float KnobLinkRingRatio = 0.92f;
constexpr float JackInsetXRatio = 0.26f;
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
constexpr float CellOverdrawRatio = 1.02f;
constexpr float PedalboardWidthRatio = 0.55f;
constexpr float PedalBodyInsetXRatio = 0.075f;
constexpr float PedalBodyInsetYRatio = 0.085f;
constexpr float PedalBodyMaskWRatio = 0.850f;
constexpr float PedalBodyMaskHRatio = 0.88333f;
constexpr float PedalBodyCornerRatio = 0.08f;
constexpr float KnobFontSizeRatio = 0.04f;
constexpr float KnobLabelWidthRatio = 0.28f;

namespace BottomBar
{
constexpr float HeightRatio = 0.1333f;
constexpr float PadRatio = 0.0833f;
constexpr float KnobMaxSizeRatio = 0.50f;
constexpr float BtnWidthRatio = 0.80f;
constexpr float BtnHeightRatio = 0.16f;
constexpr float BtnMaxHeight = 20.0f;
constexpr float BtnMaxWidth = 84.0f;
constexpr float BtnLabelGapRatio = 0.18f;
constexpr float BtnGroupGapRatio = 0.04f;
constexpr float BtnVerticalShiftRatio = 0.55f;
constexpr float StripWidthRatio = 0.42f;
constexpr float StripGapRatio = 0.10f;
constexpr float StripTotalRatio = 3.20f;
}

namespace PedalboardHeader
{
constexpr float HeightRatio = 0.10f;
constexpr float PadRatio = 0.25f;
constexpr float BtnGapRatio = 0.25f;
constexpr float BtnCenterShiftRatio = 0.05f;
constexpr float PillMaxHeight = 29.0f;
}

namespace Mixer
{
constexpr float NameLabelHeightRatio = 0.14f;
constexpr float MeterWidthRatio = 0.12f;
constexpr float MeterTrackGapRatio = 0.05f;
constexpr float TrackWidthRatio = 0.08f;
constexpr float ThumbWidthRatio = 0.16f;
constexpr float ThumbHeightRatio = 0.10f;
constexpr float SliderHitExpandRatio = 0.04f;
}

namespace Cable
{
constexpr float JackHeightRatio = 0.01795f;
constexpr float LaneSpacingPx = 6.0f;
constexpr float JackRiseMinPx = 30.0f;
constexpr float JackRiseMaxPx = 45.0f;
constexpr float JackRiseSpanRatio = 0.18f;
constexpr float CurveMinPx = 32.0f;
constexpr float CurveMaxPx = 68.0f;
constexpr float CurveBlobRatio = 0.082f;
constexpr float ArcLiftPx = 45.0f;
}

namespace Palette
{
constexpr float Blob0CenterX = 0.135f;
constexpr float BlobSpacingRatio = 0.05415f;
constexpr float BlobSizeRatio = 0.242f;
constexpr float BlobCenterY0 = 0.36f;
constexpr float BlobCenterY1 = 0.64f;
}
}
