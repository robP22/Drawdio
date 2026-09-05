# Drawdio UI and Interaction Reference

This document describes implemented editor behavior. It replaces the former
pending image-import plan and separates current behavior from future ideas.

## Canvas

- The drawable area is a 512x512 grid.
- Twelve drawable colors are available: Black, White, Red, Green, Blue, Yellow,
  Brown, Purple, Grey, Pink, Orange, and Violet.
- Transparent cells are empty and show the canvas texture.
- Five brush sizes are available (0.375, 0.75, 1.5, 2.5, 4.0; dot-only icon).
- Flood fill, erasing, rebound drawing, and undo/redo are supported.
- Undo history is capped at 64 levels / 12 MB combined (shared between undo and redo) and persists across editor minimize/maximize; importing an image or replacing the grid clears undo (redo entries survive).
- Brush size and color persist across minimize/maximize and save/restore (`brushSizeIndex` 0..4, `selectedColour` in `EditorSessionState`).

The compiler converts color-weighted cell coverage into normalized effect
parameters. Detailed color weights and routing behavior are in
[`architecture.md`](./architecture.md).

## Header and Bottom-Bar Pills

The header holds two `HeaderPill` capsules: `[ PEDALS | Reset ]` (Reset fills
red on hover) and `[ MODE | Canvas/Manual ]`.

The bottom bar holds three pills: the `LENGTH | 1 bar` automation pill plus two
split pills — `Preset [ SAVE | IMPORT ]` and `Image [ IMPORT | EXPORT ]` (equal
halves, `SplitPill`) — with `Automation` / `Preset` / `Image` captions above
each.

## Image Import / Export

Preset save writes a `.drawdio` to `Documents/Drawdio/Presets`; preset import reads a `.drawdio` (filter `*.drawdio`) from the same directory. Image import opens a file chooser in `Pictures/Drawdio` (fallback `Documents/Drawdio`) with filter `*.png;*.jpg;*.jpeg;*.bmp;*.gif`.

The selected image is rescaled to 512x512 with high-quality resampling. Pixels
with alpha below 128 become transparent. Opaque pixels are quantized to the
Drawdio palette using the weighted color-distance helper with Floyd-Steinberg
error diffusion and serpentine scanning, so regional color averages track the
source image rather than snapping each cell independently. Transparent cells
absorb diffused error without propagating it onward (error written into a
transparent cell is discarded with the cell). The result replaces the canvas,
submits a new compiler snapshot, updates the visual grid, and marks the
automation envelope dirty.

Image import is not an undo transaction. There is no color-count preview or
sampling-mode selector.

Image export writes the current 512x512 canvas to a 512x512 transparent PNG (`Pictures/Drawdio/DrawdioCanvas.png` default, `*.png` filter, warn-on-overwrite). Empty cells write `transparentBlack`; drawn cells write `ThemeManager::canvasPixelColour` (drawn Black `5` → `0xFF121212`). No texture or grain is baked.

## Palette and Tools

Palette wells are rendered beneath the color blobs. Transparent is an empty-cell
sentinel rather than a drawable palette color. Fill, eraser, and rebound modes
are mutually exclusive where the control state requires it. Rebound drawing can
choose colors procedurally rather than preserving the selected color.

## Pedals, Knobs, and Gain

The board supports six physical pedal slots in a 2x3 layout. Each pedal allocates
four knob positions; positions without a label are hidden, so only wired knobs
render. The active labels and effect mappings are
listed in [`effects.md`](./effects.md).

Pedal gain is independent of effect wet/dry mix. Moving a manually linked knob
creates a manual override and removes that knob's automation link. Links are
full-strength (no adjustable link-strength control); each linked knob has an
adjustable automation range with drag handles on the knob.

## Routing

Automatic routing orders active effects using horizontal canvas scores. Manual
routing permits one incoming and one outgoing connection per pedal and removes
conflicting connections. Invalid or empty manual routing falls back to automatic
routing.

The current cable renderer uses Bezier paths and draws cables above pedal
components. It uses cached paths and gap-aware lane placement for the current pedal layout.

## Automation

Automation is compiled from 128 horizontal canvas slices (four columns per slice, weighted Y average ignoring transparent cells). Each slice produces a
weighted Y-position value. Playback uses DAW PPQ position when available and
falls back to the processor's default transport values when it is not.

The editor displays an eight-bar timeline. Bar-count choices are 1, 2, 4, and 8;
shorter active windows can be repositioned within the displayed timeline via
`sectionStartBar` (right-drag on the automation display). The
audio path smooths automation before applying it to linked parameters. BPM from
the host transport drives time-quantized parameters (Re-Time loop length and
Sidechain divisions, default 120 BPM until transport is available).

In Manual mode the 128-slice envelope is directly drawable: left-drag paints values (lerped across intermediate slices between the previous and current X) and persisting via `ConfigManager::setManualEnvelopeSlice` / `hasManualEnvelope` + `manualEnvelope[128]` serialized in the preset. In Canvas mode the envelope is compiled from the drawing and read-only.

## Mixer and Metering

The bottom control bar provides input gain, output gain, per-pedal gain, mixer
strips, and the automation display. Peak meters are driven by processor-side
atomics. The current meter fast path samples only the beginning of a block before
falling back to a full scan, so a quiet block beginning can temporarily
under-report a later peak.

## Related Documents

- [`effects.md`](./effects.md) - effect names, labels, and DSP behavior
- [`architecture.md`](./architecture.md) - canvas compilation and audio pipeline
- [`state-format.md`](./state-format.md) - saved state
