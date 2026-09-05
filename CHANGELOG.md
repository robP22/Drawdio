# Changelog

## v0.2.5 - Finer Drawing, 512 Canvas, Multi-Format Build

### Added
- `GridSize 256->512` (`DrawdioConstants.h:6` 262144 cells, `DirtyRowWordCount 8`, queue 256KB/slot, analyzer 513 prefixes) with `2x2` legacy upscale.
- Brush map `4->5` (`ColorPalette.h:76` `0.375/0.75/1.5/2.5/4.0`), dot-only icon (ring removed), index `0..4` migration.
- Undo wire `uint32_t` index + `0x44553230` header, `12MB` cap; legacy `uint16_t` still loads.
- `SplitPill` dual-action capsules: preset `[ SAVE | IMPORT ]` and image `[ IMPORT | EXPORT ]` pills plus `[ LENGTH | 1 bar ]` automation pill; per-half hover/tooltips; `Image`/`Preset` captions.
- Header `[ MODE | Canvas/Manual ]` pill alongside `[ PEDALS | Reset ]`.
- `Presets/` cross-compatibility fixtures (`demo_preset.drawdio` v2 256-grid, `hidden.drawdio` v4 512-grid).

### Changed
- Docs: `SchemaVersion 3->4` (`StateSerializer.h:19`), grid `512x512`, automation `4 cols/slice`, brush `5`, undo `12MB`.
- Updaters build `VST3+Standalone(+AU macOS)` and install VST3; `docs/build.md` reflects multi-format.

### Fixed
- Stack overflow on pedal type select (`CanvasMessageQueue` heap storage; snapshot/preset/test locals heap-ified).
- Image label gap: stack budgets two label gaps so the `Image` caption renders.
- Pedal type swap now clears that pedal's links, ranges, routing, and knob values (plus stale-override filter).
- Mixer slider track narrowed to 60% width.

## v0.2.4 - Docs Refresh, DAW Jack Scale, and MIT License

### Added

- MIT License (`LICENSE`, `package.json:10` `MIT`).
- `docs/audits/redundancy-research-2026-09-03.md` on `drawdio_dev` (private).

### Changed

- README rewritten: What is Drawdio, screenshots (`images/screenshot-1.png` + `screenshot-2.png` both real), How to Use (canvas 256x256 12 colors + Transparent, 4 brushes, undo 64/8MB persists, 6 slots 2x3, 25 + BYPASS + reserved, gains, routing Canvas vs Manual, automation 128 slices bar/section Manual drawable, presets/session), Building (CMake 3.24 C++20 JUCE 8.0.15, targets, `updater.sh/ps1/cmd`), and Source Layout per `CMakeLists.txt:59-118`.
- Docs: `64->128` automation slices (`architecture.md:173`, `ui-controls.md:69`), `SchemaVersion 2->3` (`StateSerializer.h:19`) with `hasManualEnvelope` / `manualEnvelope[128]` and session fields (`brushSizeIndex`, `linkRangeEditEnabled`; struct also holds `isManualEnvelopeOverridden` in memory but it is not serialized), `BottomControlBarBg` (`bottomcontrolbar.png`), and positioning `DawJackScale 0.9` top-flush (`PedalboardGrid.h:70-72`).
- DAW `IN`/`OUT` jacks scale with board height (`Cable::DawJackScale 0.9`) and sit top-flush at all window sizes (verified `H 585/780/975 -> 9.45/12.6/15.75`).
- Version `0.2.3->0.2.4` (`CMakeLists.txt:2,33-34`, `package.json:3`, `docs/build.md:13`).
- `.gitignore`: `Testing/` + `nul`; on the public `release` branch, `tests/` is ignored and `docs/audits/**` / `docs/archive/**` are not whitelisted (private to `drawdio_dev`; this branch still whitelists them).

### Fixed

- Public `release` history no longer contains `docs/audits/**`, `docs/archive/**`, or `tests/**` (history rewrite via `git filter-repo`; core 8 docs only: `effects, architecture, build, ui-controls, state-format, resources, positioning, ui-architecture`).
- Wood roundover sprite shrunk `2304807->2278147` bytes; `screenshot-2.png` placeholder `7066` replaced with real `6615297`.

## v0.2.3 — Editor / Buffering / FL Delete Fixes

### Fixed
- Brush size persists across minimize/maximize and save/restore (`EditorSessionState.brushSizeIndex`).
- Canvas minimize/maximize no longer loses undo — `setGridData(clearUndo)` now optional on rehydrate; `applyFullConfigSync` checks `hasUndoData()` before overwriting grid-undo, and `m_undoBytes` tracking fixed.
- `floodFill` now marks `DirtyRowMask` so single-instance fill recompiles cables/chain via `CanvasGraphAnalyzer`.
- Pedal `BYPASS`→effect type/knobs update without minimize; `refreshRoutingFromConfig` polls `m_lastPedalTypes` every tick and `setManualRouting` triggers UI notification.
- FL mix-chain `DELETE` no longer freezes on last-instance unload — `ReleaseQueue::drainAsync` detaches deletes off loader lock via `MessageManager::callAsync`, `ConfigManager::~ConfigManager` detaches pending payloads (after stopping the compiler thread first), plus heap keep-alive `DirectX 1×1` image.
- Header pill is now `PEDALS | Reset` with the right slot turning destructive-red on hover.

### Changed
- Buffered-image flags cleaned up so stale caching no longer masks `PedalComponent`/`ArcButton` type changes until peer recreate (sole remaining call is `setBufferedToImage(false)` in `ArcButton`).

## v0.2.1 - Rendering and Mixer Bounds Fixes

- Rendering and mixer bounds corrections (see git history).

## v0.2.2 - Canvas Mapping Rework and DSP Stability

### Added

- VintageVerb-style FDN reverb network with improved diffuse tail.
- Knob snapping grids for step-quantized parameters (Re-Time, Sidechain, Bitcrusher, Convolution Damp, Tremolo Shape).
- Per-sample coefficient interpolation in MultiMode Filter, Spectral Filter, and HP/LP Filter.

### Changed

- Canvas-to-pedal mapping rework with unified positioning conventions.
- Chorus replaces Micro-Pitch Chorus (same slot, presets migrate); fixed free-running detuned taps drifting through the write head.
- Granular engine rewrite: two independent click-free grain heads; grain base clamped to a safe write-head invariant.
- Sidechain envelope shaper replaces the former Rhythm Gate (same slot, presets migrate); cycle length follows the transport BPM.
- Reverse effect keeps a per-channel freeze buffer and exits play mode with a short crossfade.
- Cable rendering uses magnetic routing with cached bezier paths.
- Effect naming unification (ReverseBufferEffect -> ReverseEffect and similar).
- HP/LP Filter fixed to the canonical TDF-II form with a bounded Q range.
- Bitcrusher dither gates to zero at digital silence.

### Fixed

- Cross-platform warnings surfaced on clang that MSVC hides by default (deprecated `Font` construction, constructor reorder, dead variable).
- Zero-delay and write-head artifacts in delay and granular paths.
- Various DSP anomalies in filter, pitch, and modulation paths.

## v0.2.0 - Hip-Hop FX Expansion and Stability Audit

### Added

- Resampler / Bitcrusher with sample-rate reduction, bit quantization, dither,
  anti-alias filtering, and DC blocking.
- Tremolo with sine, triangle, and smoothed-square amplitude modulation.
- Flanger with a short interpolated comb delay and bounded feedback.
- Per-pedal Drift / Unstable modulation for non-mix parameters.
- Image import into the 12-color Drawdio canvas palette.

### Changed

- Chain processing runs per block through `DspEffect::processBlock`.
- Effect instances are prepared before configuration publication and handed off
  through atomic configuration pointers.
- Delay, chorus, granular, reverse, glitch, and modulation state is maintained
  per channel where stereo behavior depends on channel history.
- Cables render above pedals in a single charcoal color.
- Parameter storage uses `KnobsPerPedal` and `TotalKnobs` constants.
- Preset serialization uses a versioned JUCE `ValueTree` document (`StateSerializer::SchemaVersion = 2`); the legacy binary `DRD 0x05` format is migrated on load.
- The former Tape Stop Echo slot is now Re-Time with transport-synchronized
  variable-speed loop processing.

### Fixed

- Canvas-only recompiles are consumed on the UI timer path.
- Transparent cells remain visually empty while drawn black cells remain distinct
  in DSP analysis and serialization.
- Automation linking skips the external mix knob.
- Output, per-pedal gain, and effect outputs pass through the unity soft clipper.
- Delay-line wrapping and interpolation avoid zero-delay gain jumps and zippering.
- Compressor, resonator, convolution, granular, reverse, glitch, and modulation
  paths include the current stability and click-reduction safeguards.

## v0.1.0 - Initial Release

- 22 effects plus Bypass in the original release catalog.
- 6-slot 2x3 pedalboard with automatic and manual cable routing.
- 256x256 drawable canvas with 12 colors plus transparent cells.
- Background canvas compilation with debounced drawing input.
- 20 ms crossfade between pedal configurations.
- DAW-synchronized automation envelopes with 1/2/4/8 bar lengths.
- VST3, AU, and Standalone build targets.
