# UI Positioning Conventions

How components derive their geometry, so new layout code follows one system.

## Design size

All ratios are tuned at the design resolution `EditorDesignMetrics::DesignResolution`
(1400 x 900). The editor is laid out at design size on open; the host window
scale changes everything through the same ratios.

## The four rules

1. **Ratios reference the component's own local bounds** unless a constant
   explicitly states another anchor. A child positions itself with
   `getLocalBounds().toFloat()` in its `resized()`; nothing reaches into a
   parent's geometry except through the documented setters below.
2. **Parent-space coordinates only through the `IComponentBounds` contract.**
   The one sanctioned exception: `PedalComponent::getInputJackPos()` /
   `getOutputJackPos()` return points in the pedalboard grid's space so the
   jack map, cables, and routing controller share one coordinate frame.
   Painting code is always local.
3. **Pixel values prefer the scaled-cap helper.** Fixed pixel sizes must not
    scale with the window; use `EditorLayout::scaledCap(usable, ratio, designPx,
    scale)` (e.g. bottom-bar button caps) or a height-derived ratio
    (`EditorLayout::scaleFromHeight`). DAW jack height follows the rule
    (`Cable::JackHeightRatio * DawJackScale`, top-flush via `dawJackHeight()*0.5`).
    Known exceptions documented as-is: `JackHitMap` radius uses a manual clamp,
    cable-lane pixel constants are used directly, and `BottomControlBar::paint`
    duplicates its caption caps as plain minima.
4. **Sprite alignment only through `EditorLayout`.** Sprite opaque-edge scans
   (`topOpaqueRatio` / `bottomOpaqueRatio`) stay in `UI/EditorLayout.h`; a
   component scans its own sprite (e.g. `ColorPalette` scans
   `ColorPaletteBody`) and receives cross-column facts through semantic layout
   values such as `ColorPalette::setImageCenterX(x)`.

## Semantic cross-component values (not pixel pushes)

| Setter | Meaning | Consumer computes |
|---|---|---|
| `ColorPalette::setImageCenterX(x)` | Canvas centre X (palette-local) | Palette texture horizontal placement |
| `PedalboardHeader::setButtonCenterY(y)` | Header button row Y (parent sprite scan) | Header pill vertical placement |

The editor owns the column layout; each component owns the formulas that map
its own sprite and the semantic values into pixels. Other layout is via
`EditorLayout::calculate` and `EditorDesignMetrics` constants.

## Tune-constant index

Constants that compensate sprite padding or visual centring; keep them next to
their use and document the intent:

- `KnobCenterYShiftRatio` (-0.02) — knob rows sit slightly above nominal so
they read centred on the pedal body mask.
- `PaletteHeightRatio` / `PaletteColumnInsetRatio` / `Palette Blob*` — palette content tuned against the
palette sprite's own padding.
- `VerticalGroupOffsetRatio` — pedal group pushed down against the sprite's
taller bottom padding.
- `DawJackScale` (0.9) — preserves the 10% `input_jack.png` reduction while keeping `dawEntryPos/dawExitPos.y = dawJackHeight()*0.5` top-flush; `dawJackHeight() = H * Cable::JackHeightRatio * DawJackScale` scales with `PedalboardGrid` height (verified: `H 585/780/975 -> 9.45/12.6/15.75`, gap `0` at all scales).

## Per-component anchors (reference)

| Component | Anchor base | Key ratios |
|---|---|---|
| `PedalboardGrid` | own bounds | grid padding, column/row gaps, pedal size clamps |
| `PedalComponent` | own bounds | body mask, LED, LCD label area, knob schema |
| `PixelCanvasComponent` | own bounds + column facts | `CanvasScaleRatio`, shift ratios, content top |
| `ColorPalette` | own bounds + sprite ratios | blob grid, wheel ring, arc buttons |
| `BottomControlBar` | own height (`usableH = h - 2 pad`) | button stack, strips, automation width |
| `MixerStrip` | own bounds | name label, meter, track |
| `SpriteKnob` / `ArcButton` | own bounds | self-centred, device-pixel snapped |
