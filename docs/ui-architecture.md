# Drawdio UI Architecture

## Component Hierarchy

- `PedalboardGrid` — six `PedalComponent` slots, manual cable routing, cable rendering above pedals.
- `BottomControlBar` — `HeaderPill` automation-length pill, two `SplitPill` capsules (preset save/import, image import/export), `AutomationDisplay`, six `MixerStrip` gain strips.
- `PixelCanvasComponent` — 512x512 drawable grid, ARGB overlay, brush/fill/eraser, undo/redo stacks.
- `ColorPalette` — color wells, brush-size `ArcButton` toggle, tool selection.
- `PedalComponent` — enclosure sprite, LCD label, knob `SpriteKnob`s (unwired positions hidden), type popup, link indicators.
- `MixerStrip` — meter + gain slider per slot.
- `AutomationDisplay` — eight-bar envelope, section window, manual drawable envelope.
- `SplitPill` / `HeaderPill` — dual-action capsule / label+value capsule.

See [`ui-controls.md`](./ui-controls.md) for per-component behavior. Compilation,
parameter flow, and audio lifecycle live in [`architecture.md`](./architecture.md);
this document covers editor-side state flow, layout, and assets only.

## State Flow

The processor owns persistent and runtime state. The editor receives coherent,
revisioned presentation snapshots and sends typed actions back through the
editor bridge.

```text
UI component -> typed action -> EditorProcessorBridge -> processor state
processor state -> coherent snapshot -> presentation store -> view binder -> UI
```

`EditorSyncController` owns the editor tick: release-queue drain, deferred-config
application, compiled-knob sync, automation sync, routing refresh, and repaint
flags. Components receive only the view data and actions they need. Components may
keep temporary interaction state, such as an active canvas stroke or knob drag,
but canonical project state is owned by the processor.

## Paint and Resized Conventions

Components paint from local bounds using ratios of their own height/width
(see [`positioning.md`](./positioning.md)); `resized()` computes internal
geometry only. Parent-level bounds come from `EditorLayout::calculate`.
Repaints are gated on state change (no per-tick repaint loops).

## Undo Model

Canvas undo/redo lives in `PixelCanvasComponent` (64 levels / 12 MB combined
budget) and persists processor-lifetime via `ConfigManager::m_undoData`,
restored on editor reopen and synced on the editor tick. Importing an image or
replacing the grid clears undo (redo entries survive).

## File Paths

`.drawdio` preset save/import targets `Documents/Drawdio/Presets` (`*.drawdio`
filter). Image import/export targets `Pictures/Drawdio` (fallback
`Documents/Drawdio`): import accepts PNG/JPG/JPEG/BMP/GIF rescaled to 512x512
with Floyd-Steinberg dithering; export writes a 512x512 transparent PNG.

## Persistent State

`PresetState` contains reusable Drawdio behavior (canvas, 6 pedal slots, routing, knob values, override mask, bar/section settings, mode, gains, link flags/ranges, and `hasManualEnvelope` / `manualEnvelope[128]`). `EditorSessionState` contains
stable project-specific UI context (`selectedColour`, `selectedTool`, `selectedPedal`, `brushSizeIndex`, `linkRangeEditEnabled`, `isManualEnvelopeOverridden` — the last is in-memory only, never serialized). Host project state stores both; `.drawdio`
files store only the preset layer. See [`state-format.md`](./state-format.md).

## Layout

`EditorDesignMetrics` contains design-space constants. Asset-derived measurements
come from sprite self-scans surfaced through `EditorLayout` helpers
(`topOpaqueRatio` etc.). `EditorLayout` calculates bounds. `PluginEditor`
applies the resulting bounds. Components calculate only internal geometry.

Window resizing affects only presentation. It does not change the logical grid,
preset data, routing analysis, or compiler input.

The supported design scale is 0.75x through 1.25x, corresponding to 1050x675
through 1750x1125 at a locked 14:9 ratio.

## Assets

`ResourceManager` loads immutable source assets. `ScaledAssetProvider` owns
resampling policy and rendered-size caching. Pixel-art policy exists
(`PixelArt` maps to low resampling quality), but every current call site
passes `Continuous`, so all sprites render with high-quality resampling.
`bottomcontrolbar.png` is decoded as `BottomControlBarBg` for the mixer bottom bar; `input_jack.png` DAW jacks scale with board height (`EditorDesignMetrics::Cable::JackHeightRatio * DawJackScale`).

During live editor resizing, cached imagery is reused and the pixel overlay is
scaled from its last valid render. Exact-size assets and the final canvas
overlay are refreshed after the 100ms resize-settle interval. Cable geometry
remains synchronized with the current pedal bounds throughout resizing. DAW jack hit-testing via `JackHitMap` uses the same `dawEntryPos/dawExitPos` helper as rendering.
