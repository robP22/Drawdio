# Drawdio UI Architecture

## State Flow

The processor owns persistent and runtime state. The editor receives coherent,
revisioned presentation snapshots and sends typed actions back through the
editor bridge.

```text
UI component -> typed action -> EditorProcessorBridge -> processor state
processor state -> coherent snapshot -> presentation store -> view binder -> UI
```

`EditorSyncController` is a transitional coordinator during migration. Its
long-term responsibilities are divided between the bridge, presentation store,
view binder, automation coordination, and configuration maintenance.

Components receive only the view data and actions they need. Components may
keep temporary interaction state, such as an active canvas stroke or knob drag,
but canonical project state is owned by the processor.

## Persistent State

`PresetState` contains reusable Drawdio behavior (canvas, 6 pedal slots, routing, knob values, override mask, bar/section settings, mode, gains, link flags/ranges, and `hasManualEnvelope` / `manualEnvelope[128]`). `EditorSessionState` contains
stable project-specific UI context (`selectedColour`, `selectedTool`, `selectedPedal`, `brushSizeIndex`, `linkRangeEditEnabled`, `isManualEnvelopeOverridden`). Host project state stores both; `.drawdio`
files store only the preset layer. See [`state-format.md`](./state-format.md).

## Compilation

The encoded canvas remains a fixed 256x256 grid. Drawing submits one complete
grid after pen-up plus a dirty-row mask. The compiler retains cached graph
analysis and updates affected rows only.

Compilation requests and results carry revisions. Older requests are discarded
before publication, and stale results are rejected before configuration handoff.

Parameter-only graph changes use the compiled parameter path. Effect swaps with
stable topology use a slot replacement path. Active-count and routing-order
changes use a complete topology handoff. All effect preparation remains off the
audio thread.

Window resizing affects only presentation. It does not change the logical grid,
preset data, routing analysis, or compiler input.

## Layout

`EditorDesignMetrics` contains design-space constants. `SpriteMetrics` owns
asset-derived measurements. `EditorLayout` calculates bounds. `PluginEditor`
applies the resulting bounds. Components calculate only internal geometry.

The supported design scale is 0.75x through 1.25x, corresponding to 1050x675
through 1750x1125 at a locked 14:9 ratio.

## Assets

`ResourceManager` loads immutable source assets. `ScaledAssetProvider` owns
resampling policy and rendered-size caching. Pixel-art assets use nearest
neighbour scaling; continuous textures use high-quality resampling. `bottomcontrolbar.png` is decoded as `BottomControlBarBg` for the mixer bottom bar; `input_jack.png` DAW jacks scale with `PedalboardGrid::dawJackHeight()` (`Cable::JackHeightRatio * DawJackScale`).

During live editor resizing, cached imagery is reused and the pixel overlay is
scaled from its last valid render. Exact-size assets and the final canvas
overlay are refreshed after the 100ms resize-settle interval. Cable geometry
remains synchronized with the current pedal bounds throughout resizing. DAW jack hit-testing via `JackHitMap` uses the same `dawEntryPos/dawExitPos` helper as rendering.
