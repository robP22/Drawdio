#pragma once

#include <algorithm>
#include <cstdint>

namespace GridLayout
{
constexpr int RowCount = 2;
constexpr int ColCount = 3;

namespace DesignResolution
{
    constexpr int Width = 1400;
    constexpr int Height = 900;
}

// Window scale factor derived from a component whose height at design size is
// designRatio * DesignResolution::Height (e.g. the bottom bar: h / 120).
inline float scaleFromHeight(float componentHeight, float designRatio)
{
    return componentHeight / (designRatio * static_cast<float>(DesignResolution::Height));
}

// Pixel-value caps that must shrink with the window:
// min(usable * ratio, designPx * scale) keeps the design-size look while
// preventing oversized controls on large windows.
inline float scaledCap(float usable, float ratio, float designPx, float scale)
{
    return std::min(usable * ratio, designPx * scale);
}

constexpr float GridSidePaddingRatio = 0.01f;
constexpr float GridTopPaddingRatio = 0.0365f;

constexpr float PedalShrinkRatio = 0.95f;
constexpr float ColumnGapRatio = 0.015f;
constexpr float RowGapRatio = 0.0475f;
// Extra downward push of the pedal group; compensates the pedal sprite's
// taller bottom padding so the group reads vertically centered on the body.
constexpr float VerticalGroupOffsetRatio = 0.025f;

constexpr float PedalWidthMinRatio = 0.18f;
constexpr float PedalWidthMaxRatio = 0.28f;
constexpr float PedalHeightMinRatio = 0.25f;
constexpr float PedalHeightMaxRatio = 0.39f;

constexpr float PaletteHeightRatio = 0.26f;

// Horizontal inset of the palette panel within the canvas column so the
// full-bleed sprite never touches the window/column edges.
constexpr float PaletteColumnInsetRatio = 0.05f;

// The canvas texture is drawn at 90% of the canvas component's size, leaving
// a 5% margin around it so it never touches the window or palette edges.
constexpr float CanvasScaleRatio = 0.90f;

constexpr float KnobSizeRatio = 0.16f;
constexpr float KnobSpreadRatio = 0.35f;
// Compensates the schema's centred-on-body rows for the sprite's top padding;
// knobs sit slightly above their nominal rows so they read centred on the
// visible pedal body mask.
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

// ── Bottom Control Bar ──
namespace BottomBar {
    constexpr float HeightRatio           = 0.1333f;  // 120 / 900
    constexpr float PadRatio              = 0.0833f;  //  10 / 120
    constexpr float KnobMaxSizeRatio      = 0.50f;    //  50 / 100 usable
    constexpr float BtnWidthRatio         = 0.65f;    //  65 / 100 usable
    constexpr float BtnHeightRatio        = 0.14f;    //  14 / 100 usable
    constexpr float BtnMaxHeight          = 18.0f;    // cap
    constexpr float BtnMaxWidth           = 70.0f;    // cap
    constexpr float StripWidthRatio       = 0.42f;    //  45 / 100 usable
    constexpr float StripGapRatio         = 0.10f;    //  10 / 100 usable
    constexpr float StripTotalRatio       = 3.20f;    // 320 / 100 usable
}

// ── Mixer Strip ──
namespace Mixer {
    constexpr float NameLabelHeightRatio  = 0.14f;    // 14 / 100
    constexpr float MeterWidthRatio       = 0.12f;    // 12 / 100
    constexpr float MeterTrackGapRatio    = 0.05f;    //  5 / 100
    constexpr float TrackWidthRatio       = 0.08f;    //  8 / 100
    constexpr float ThumbWidthRatio       = 0.16f;    // 16 / 100
    constexpr float ThumbHeightRatio      = 0.10f;    // 10 / 100
    constexpr float SliderHitExpandRatio  = 0.04f;    //  4 / 100
}

// ── Pedal Body (moved from inline literals) ──
constexpr float PedalBodyInsetXRatio     = 0.075f;
constexpr float PedalBodyInsetYRatio     = 0.085f;
constexpr float PedalBodyMaskWRatio      = 0.850f;
constexpr float PedalBodyMaskHRatio      = 0.88333f;
constexpr float PedalBodyCornerRatio     = 0.08f;
constexpr float KnobFontSizeRatio        = 0.04f;
constexpr float KnobLabelWidthRatio      = 0.28f;

// ── Cable Drawing ──
// DAW jack vertical position as a fraction of the pedalboard grid height
// (14 px at the 780 px design grid height).
constexpr float CableJackHeightRatio      = 0.01795f;
constexpr float CableLaneSpacingPx       = 6.0f;
// Cable corner fillet control arm, scaled so the bend is about the size of
// a palette color blob at any window size (0.242 * 0.26 * gridH ~ 49px at
// the 780px design grid height).
constexpr float CableCurveBlobRatio      = 0.0629f;
// Top-row DAW band offset above the DAW jack line; sized so the entry/exit
// corner fillets stay smooth (~27px at design, scaling with the layout).
constexpr float CableArcLiftPx           = 60.0f;

// ── Palette ──
constexpr float PaletteBlob0CenterX      = 0.135f;
constexpr float PaletteBlobSpacingRatio  = 0.05415f;
constexpr float PaletteBlobSizeRatio     = 0.242f;
constexpr float PaletteBlobCenterY0      = 0.36f;
constexpr float PaletteBlobCenterY1      = 0.64f;
}
