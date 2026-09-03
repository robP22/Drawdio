# Drawdio UI and Interaction Reference

This document describes implemented editor behavior. It replaces the former
pending image-import plan and separates current behavior from future ideas.

## Canvas

- The drawable area is a 256x256 grid.
- Twelve drawable colors are available: Black, White, Red, Green, Blue, Yellow,
  Brown, Purple, Grey, Pink, Orange, and Violet.
- Transparent cells are empty and show the canvas texture.
- Four brush sizes are available.
- Flood fill, erasing, rebound drawing, and undo/redo are supported.
- Undo history is bounded by the implemented level and byte limits.
- Importing an image or replacing the grid clears the current undo history.

The compiler converts color-weighted cell coverage into normalized effect
parameters. Detailed color weights and routing behavior are in
[`architecture.md`](./architecture.md).

## Image Import

The Import control opens a file chooser and accepts files supported by JUCE's
image loader. The chooser currently uses a broad `*` filter rather than a
PNG/JPEG-only filter.

The selected image is rescaled to 256x256 with high-quality resampling. Pixels
with alpha below 128 become transparent. Opaque pixels are quantized to the
Drawdio palette using the weighted color-distance helper with Floyd-Steinberg
error diffusion and serpentine scanning, so regional color averages track the
source image rather than snapping each cell independently. Transparent cells
neither receive nor propagate diffusion error. The result replaces the canvas,
submits a new compiler snapshot, updates the visual grid, and marks the
automation envelope dirty.

Image import is not an undo transaction. There is no color-count preview or
sampling-mode selector.

## Palette and Tools

Palette wells are rendered beneath the color blobs. Transparent is an empty-cell
sentinel rather than a drawable palette color. Fill, eraser, and rebound modes
are mutually exclusive where the control state requires it. Rebound drawing can
choose colors procedurally rather than preserving the selected color.

## Pedals, Knobs, and Gain

The board supports six physical pedal slots in a 2x3 layout. Each pedal renders
four knobs for visual consistency, although individual effects may leave some
knob positions unlabeled or unwired. The active labels and effect mappings are
listed in [`effects.md`](./effects.md).

Pedal gain is independent of effect wet/dry mix. Moving a manually linked knob
creates a manual override and removes that knob's automation link. The current
UI uses full-strength links; there is no adjustable link-strength control.

## Routing

Automatic routing orders active effects using horizontal canvas scores. Manual
routing permits one incoming and one outgoing connection per pedal and removes
conflicting connections. Invalid or empty manual routing falls back to automatic
routing.

The current cable renderer uses Bezier paths and draws cables above pedal
components. It uses cached paths and gap-aware lane placement for the current pedal layout.

## Automation

Automation is compiled from 64 horizontal canvas slices. Each slice produces a
weighted Y-position value. Playback uses DAW PPQ position when available and
falls back to the processor's default transport values when it is not.

The editor displays an eight-bar timeline. Bar-count choices are 1, 2, 4, and 8;
shorter active windows can be repositioned within the displayed timeline. The
audio path smooths automation before applying it to linked parameters. BPM from
the host transport drives time-quantized parameters (Re-Time loop length and
Sidechain divisions, default 120 BPM until transport is available).

Manual automation point editing is not implemented.

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
