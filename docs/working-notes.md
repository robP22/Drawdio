# Working Notes

This file is temporary project working memory, not a canonical product or
implementation reference. Current documentation is indexed from `README.md` and
the guides in this directory.

## Current state
- Headless Catch2 unit-test suite added for the Drawdio DSP effects (`drawdio_tests` target). Vertical slice (DspEffectFactory + Filter + Delay) green, expanded to Distortion/Reverb/Modulation/Pitch/ReTime. All 29 cases / 4.76M assertions pass (Debug + Release).
- Import dithering implemented (Floyd-Steinberg, 12-color palette). Both Release targets build clean. Pending: user visual check importing gradient images.
- Effect overhaul (2026-08-29): GrainScrubber continuous-cursor granular read (content-discontinuity fix), ReTime HalfTime-style freeze-engine redesign (Mix/Time/Bars/Shift), new HP/LP Filter pedal (slot 22, was dead Random Modulator), Mix knob on every pedal, knob-position schema (1-5 knobs: single/column/triangle/square/die). Release VST3 + Standalone + all 22 tests green.
- Full effects audit (2026-08-29, this session): dual-grain staggered-head rewrite + write-head-safe position clamp in `GranularProcessor.h` (scrubber crush + grain clicks fixed), MicroPitchChorus replaced by classic Chorus (fixed 30ms center delay, bounded LFO — tap-drift "pitch on acid" fixed), SimpleDelay first-block snap (no load glide), ReverseBuffer per-repeat splice capture, MultiModeFilter Nyquist cap + non-finite reset, SpectralFilter per-sample coefficient interpolation, CombResonator tail aggregation. Audit + per-effect edge-case analysis in `docs/audits/effects-audit-2026-08-29.md`. Tests: 22 → 28 cases. NOTE: the "continuous readCursor" granular design described in the 2026-08-29 section below was never landed in source; the landed fix is the staggered dual-grain head design.

## 2026-08-29 — Full effects audit & fixes (dual-grain rewrite, Chorus, hardening)

### Grain Scrubber "destroys/crushes" — root cause & fix
- Root cause: `processGranularSample` mapped position to `offset = (position + spray) * bufSize` clamped only to `[0, bufSize)`. When `offset > bufSize - grainLen` (Position knob ~0.86-1.0 with spray), `grainBase` put the read window across the write head → reads un-written/stale-cycle buffer territory → crushed signal + mid-grain content switches. Unsigned-modulo base arithmetic also had underflow edges.
- Fix (`GranularProcessor.h`): `computeGrainBase()` clamps the total offset to `[128, bufSize - grainLen * max(1, 1/speed) - 128]`, signed 64-bit base math. The speed term matters: a grain lasts `grainLen/speed` samples and the write head laps the window start after `bufSize - grainLen - offset` samples — at speed < 1 the read head is slower than the write head and gets overtaken (verified with a harness: old clamp produced dist 0 / read-at-write-head at speed 0.5, position 0.99).

### Grain clicks — root cause & fix
- Old dual-grain: shared cycle, `grainPhase2 = 0.5*grainLen` reset at every restart → head-2 window jumped 0→(0.1-1.0) at the new random base → click at every restart (6-25 Hz).
- Fix: two INDEPENDENT grain heads, restarts staggered by half a grain length; each head's Hann window is zero at its own restart; windows stay complementary (unity sum; grainLen from `int(sr*0.08f)` is 3527 (odd) so the stagger is 1763.5 samples — sub-0.1% window-sum ripple, inaudible, verified by DC soak test: max step < 1e-3 after warm-up).

### MicroPitchChorus → Chorus
- Old taps were free-running with constant detune speeds against a 0.5s buffer: tap1 (speed > 1) caught and passed the write head, tap2 lagged off the buffer end — periodic glitches + sustained garbage ("pitch on acid"), every ~17-34s at default detune.
- `MICROPITCH_CHORUS` → `CHORUS` (integer 2 unchanged, presets migrate). New `ChorusEffect` (ModulationEffects): fixed 30ms center delay, LFO ±depth clamped below `centerDelay - 1` (10-50ms window behind the write head by construction), rate 0.05-3Hz, per-channel LFO phase offset. Removed from DelayEffects; factory/definitions/tests updated.

### Other fixes
- SimpleDelay: `m_smoothedDelaySamples` seeds 0.55·sr; `m_firstBlock` snaps to compiled time on first block after prepare/reset (killed ~230ms load glide).
- ReverseBuffer: `xfadeFrom` re-captured at every repeat (was capture-once → hard splice click at later repeats).
- MultiModeFilter: cutoff capped at 0.45·sr (block + sample paths); non-finite state reset (`st = {}`).
- SpectralFilter: per-sample coefficient interpolation via `m_prevCenter/Width/Q` (block-boundary zipper gone).
- CombResonator: `m_hasTail` peak aggregated across channels.

### ReTime / F3
- The old-design loop-overwrite finding is moot: the working tree already contains the freeze-engine redesign (ring + freeze buffer, recapture at sync/loop-end wrap) — loop never overwritten between captures. No change needed.

### Tests
- New: GranularProcessor DC soak (click-free), write-head invariant (9 position×speed configs, dist ≥ 100), Chorus depth + 60s drift soak (ZCR stable), SimpleDelay no-glide, ReverseBuffer splice bound, MultiMode extreme-cutoff finiteness. MicroPitch cases replaced. Suite: 28 cases / 4.76M assertions, Debug + Release green.

### Verification
- `drawdio_tests` Debug + Release: 28/28. Plugin `Drawdio` Debug builds clean. Docs updated: `docs/effects.md` (full rewrite to current wiring incl. mix indices + HP/LP + Chorus), `docs/state-format.md` (slot 2/22 migration notes), `AGENTS.md` progress.
- Pending manual: scrubber position sweep at multiple rates, chorus soak, pitch shifter, delay automation sweeps.

## 2026-08-29 (late) — ReverseBuffer clicks + knob layout staleness + 5-knob schema

### ReverseBuffer clicks (user report)
- Cause 1: play→record transition had NO crossfade — output jumped from the last played slice sample to live input → click at every cycle end.
- Cause 2: the 1.5s ring gets lapped by the record head mid-cycle for most densities (e.g., density 0.5 → 2.6s record+play cycle; overwrite of sliceStart begins 0.975s into a 2.1s play phase) → later repeats read live input → content discontinuities. The existing density-0.2 test (1.44s cycle < 1.5s buffer) could not catch it.
- Fix: per-channel `freeze` buffer (1.0s·sr, allocated in `prepare`, no audio-thread alloc) — slice copied wrap-aware from the ring at record→play, playback reads from `freeze` (ReTime-recapture pattern); `exitFadePos`/`exitFadeFrom` raised-cosine blend (32 samples) at play→record. Ring keeps recording raw input for the next capture. New test: density 0.5, 12s run, max step < 0.15. Suite now 29 cases / 4.76M assertions.

### Knob layout staleness (user report: labels top-left, one knob centered)
- Root cause: `syncFromProcessor()` and the type popup reassigned `m_definition` but never re-ran `updateKnobBounds()`. After a type change (preset load/undo/popup, typically BYPASS → real type), knob bounds/visibility stayed from the previous definition (BYPASS = count-1 → one center knob, others empty), while labels painted fresh at the stale bounds → labels stacked at the pedal origin, one knob at center.
- Fix: extracted `applyKnobLayout()` (updateKnobBounds + knob setBounds) called from `resized()`, the popup handler, and the sync path; `m_knobBounds{}` default-init; bounds cleared + knobs hidden on the empty-definition early return.

### 5-knob schema correction (user spec)
- `knobLayoutForCount(5)` was die-with-center-pip with the center at slot 2 (the 3rd labeled knob would have taken it). Now: 4 corners in slots 0-3 + 5th knob below-centered at `(0.50, 0.685)` in slot 4. Schema: 1 centered / 2 centered column / 3 triangle (third below a 2-row) / 4 square / 5 square + below-centered. No current pedal has 5 knobs (latent).

### Verification
- `drawdio_tests` Debug + Release: 29/29. Plugin `Drawdio` Debug + Release build clean. Pending manual: reverse buffer at several densities, knob layout across pedal types + preset loads + popup type changes.

## 2026-08-29 (late 2) — UI audit & fixes

- **P1 overlay staleness** (`PixelCanvasComponent.cpp:576-578`): the 4096-pending-cell cap CLEARED the list, permanently dropping unflushed overlay cells (pixels state vs rendered overlay diverged after large single-event strokes with big brushes). Fix: cap sets `m_overlayDirty = true` → next paint rebuilds fully. Verified consistent with `flushPendingOverlay` early-return and `paint()` rebuild check.
- **P2 automation width** (`BottomControlBar.cpp:124-126`): width `stripStartX - autoX - pad` goes negative below ~577px bar width → invalid JUCE bounds. Clamped with `jmax(0, ...)`. Note: `usableH = 0.833·h` is always positive — no other clamp needed.
- **P3 tooltips**: JUCE 8.0.4 has NO `Component::setTooltip` — tooltips live on `SettableTooltipClient`/`TooltipWindow` (juce_gui_basics). Added `juce::TooltipWindow` to `DrawdioProcessorEditor`, `ArcButton` now inherits `SettableTooltipClient`, 7 palette buttons set the description string as tooltip. `hasKeyboardFocus` in JUCE 8 requires an explicit bool arg (`hasKeyboardFocus(true)`).
- **P3 focus ring**: `ArcButton::paint` strokes the arc path white@55% when `hasKeyboardFocus(true)`; `focusGained`/`focusLost` repaint overrides.
- **P4 dead code**: `m_changedCellCount` removed (5 write sites, 0 reads).
- Theme interface (10 unconsumed colour methods) intentionally KEPT as the future theming seam (user decision).
- Docs: `docs/audits/ui-audit-2026-08-29.md` (six analyses + FIXED markers), AGENTS.md entry.
- Builds: Debug + Release clean; tests 29/29 stable. Pending manual: fast large-brush strokes (overlay freshness), narrow window, tooltips + tab focus ring.

## 2026-08-29 (late 3) — Positioning system unification

Anomalies (full table in ui-audit doc): fixed-px caps in ratio systems (bottom-bar 50/70/18, JackHitMap radius 24, DAW jack y 14px), sprite-scan coupling (editor held palette sprite ratios + pushed 3 px values into the palette and 1 into the canvas), fixed 9px fonts, undocumented tune constants, dead constants.

Fixes:
- `GridLayout.h`: `scaleFromHeight(h, designRatio)` + `scaledCap(usable, ratio, designPx, scale)` helpers; tune-constant comments; removed `PaletteButtonGapRatio`/`ButtonSquareSizeRatio`; `CableJackHeight` (14px) → `CableJackHeightRatio` (14/780).
- `BottomControlBar::resized`: caps via scaledCap (scale = h/120); "In"/"Out" font → clamp(7, usableH·0.09, 12). `MixerStrip` name font likewise (h·0.09).
- `JackHitMap`: radius now a member `clamp(12, min(w,h)·24/770, 24)` computed in `refresh()` (design: min(770,780)·24/770 = 24.0 exact); `refresh()` gained the grid bounds param; `ManualRoutingController` uses `m_jackMap.radius()`.
- `PedalboardGrid::dawEntryPos/dawExitPos`: y = gridH·CableJackHeightRatio·0.5 (7.0005 ≈ 7px at design).
- Sprite decoupling: `ColorPalette` scans its own `ColorPaletteBody` in the ctor (TextureId == ImageId, same bitmap — verified in ResourceManager); `setPedalBottomRatio(ratio)` replaces `setImageBottomShift`+`setContentCenterOffset`; shift = roundToInt(columnH·r) − roundToInt(paletteH·botR) with columnH = getY() + paletteH — PROVEN equal to the old pedalH-based formula (both areas share topArea height); content offset computed internally from own ratios. `PixelCanvasComponent::setContentTop(ratio, columnHeightPx)` replaces `setCanvasTopOffset(px)`; offsetY = roundToInt(columnHeightPx·ratio), fallback (centered) kept while unset. `PluginEditor` slimmed (palette ratios + 3 setters gone; ~20 lines).
- Docs: `docs/positioning.md` (four conventions + semantic-value table + tune index), ui-audit positioning section (A1-A8), AGENTS entry.
- Verification: Debug + Release clean, tests 29/29. Design-size arithmetic identical (checked by hand). Pending manual: visual A/B at design size (canvas top, palette bottom, blob grid, wheel, canvas centre).

## 2026-08-29 (late 4) — Knob snapping + Sidechain + ReTime double time

- `PedalParameterDefinition::snapSteps` (even grid on [0,1], 0 = off) + `PedalDefinitions::snapSteps/snapValue`; `makeDefinition` gained 4 snap args (default 0, existing rows untouched).
- Snap applied at 4 sites: drag (`PedalComponent::onKnobValueChanged` — snaps + sets the knob visual + stores), compiled display + override (`EditorSyncController::applyParameterToPedal` via `pedal->snapValue`), linked/canvas automation display (`syncKnobAutomation`), and the FINAL DSP value in `UnifiedPedalProcessor::processChainBlock` after smoothing (covers linked + canvas automation on the audio thread; mix knob excluded). `PedalAssetPayload::snapSteps` (per chain position × knob, uint8) filled in `ConfigManager::loadPedalConfiguration` — zero audio-thread cost.
- Grids: ReTime Time 5 → {0.25, 0.5, 0.75, 1, 2}x; ReTime Bars 4 → {½, 1, 2, 4}; Sidechain Rate 5 → {⅙, ¼, ⅓, ½, 1} beat; Bitcrusher Bits 15 → 2-16; Convolution Reverb Damp 15 → 16 rows (midpoint bias: detent 0.5 → row 8, row 7 unselected — harmless, tested monotonic); Tremolo Shape 3.
- ReTime DSP: `kRatios[5]` incl 2.0x, `kBars[4]` incl 0.5; `ratioIdx = lround(t·4)`, `barsIdx = lround(t·3)` — the old `lround((t−.25)/.25)` binned the new ⅓ detent wrong (verified numerically). 2x verified safe (freeze reads, phase wrap, recapture, length clamp).
- Sidechain: `RhythmGateEffect` → `SidechainEffect`, `RHYTHM_GATE` → `SIDECHAIN` (13 unchanged, presets migrate), display "Sidechain"; `setTransport` override (bpm floor 20, default 120) via the existing virtual (`DspEffect.h:21`, forwarded per block `UnifiedPedalProcessor.cpp:89` — no new plumbing); cycle = max(1.5ms, 60/bpm·division); phase `%`-guard + 1ms smoothing kept; Shape knob stays the attack/decay character control.
- Tests: `tests/test_SnapUtils.cpp` (grid math incl. midpoints/passthrough, detent→DSP-index consistency for all 6 grids, Sidechain gate-period counts at 120 BPM across all 5 detents) — `PedalDefinition.cpp` + the test file added to the CMake test target. Suite: 33 cases / 4,762,995 assertions, Debug + Release green; plugin builds clean both configs. Pending manual: detent feel, automation sweep stepping, sidechain at 60/120/200 BPM, ReTime ½-bar + double-time.

## 2026-08-29 (late 5) — HP/LP distortion (TDF-II bug), Pitch snap, Bitcrusher silence

- **HP/LP "distortion regardless of settings" — real bug found and fixed**: the biquads computed `y = b0·x + b1·z1 + b2·z2` inside the TDF-II v-recursion — the canonical form is `y = b0·v + b1·z1 + b2·z2`. The x-form gives the HP an effective numerator `b0·A(z) + b1z⁻¹ + b2z⁻²`; since A(1)≈0 for the HP, this is a near-DC pole with ~900x low-frequency gain (reproduced in python: 0.5-amp 1kHz sine → 379-398 output peak; corrected → 1.92 / 0.52). The old `!isfinite` reset was a band-aid over this instability. Plan's original fixes (Q cap 0.5-2.0, transparent defaults High 0 / Low 1 / Reso 0, prev-state per-sample coefficient interpolation) are all in too — the Q cap alone would NOT have fixed the reported bug.
- **Pitch Shifter**: `snapSteps 25` on knob 2 — the `exp2(rate·2−1)` mapping is semitone-linear (2^((24r−12)/12)), so detents are exact ±12 semitones; no DSP change.
- **Bitcrusher silence**: TPDF dither gated by `min(1, |hold|·10)` (full above ~−20 dBFS, fades to zero at silence) — was ±1 LSB noise on zero input (full-scale at 2 bits).
- `makeDefinition` gained default-value args (dv0..dv3, default 0.5) for per-row defaults.
- Tests: +5 → 38 cases / 4,763,081 assertions (filter bounded peak 3.9x vs 760x pre-fix, filter transparency 0.52, bitcrusher silence < 1e-4 + signal sanity, pitch grid). NOTE: `ReverseBufferEffect.h` include in test_ExpandedEffects updated — the tree renamed it to `ReverseEffect` mid-session.
- Builds: Debug + Release clean, suite 38/38 both configs. Pending manual: HP/LP default cleanliness + Reso sweep, pitch detents in key, bitcrusher silence.






- Cable magnetic routing (2026-08-29, v0.2.2): lane-based gap router — cables pulled into vertical column gaps + row-gap corridor with per-channel lane fan-out; ColumnGapRatio 0.005 -> 0.015. User reverted the granular dual-cursor rewrite to a dual-grain-head design (readPtrA/B + computeGrainBase); fixed a compile break in it (computeGrainBase call sites missing playbackSpeed arg).

## 2026-08-29 — Magnetic cable routing with lane fan-out

### Problem
- Same-row cables arced over pedal faces; cross-row cables shared identical control points -> 5+ overlapping cables across one pedal; no lane separation anywhere.

### Implementation
- `GridLayout.h`: `ColumnGapRatio` 0.005 -> 0.015 (~20px gaps); new `CableLaneSpacingPx` 6.0, `CableWaypointCurvePx` 60.
- `CablePathBuilder`: new `buildWaypointCable(waypoints, startTangent, endTangent)` — cubic segments with chord-tangent rounded corners at interior waypoints; jacks leave/arrive perpendicular to pedal top edge (tangents (0,-1)/(0,1)); each segment split via `splitCubicBezier` into CachedSplitCable halves. Removed dead `buildInputCable`/`buildOutputCable`.
- `PedalboardGrid`: single `rebuildConnectionCables()` now routes EVERYTHING (connection edges in both modes + DAW in/out):
  - Channels: 4 vertical (row*2+gap, midpoint of adjacent pedal edges) + 1 horizontal (row-gap center).
  - Route kinds: SameRow (cubic, control points at gapX+laneX, lifted), CrossRow (waypoints; adjacent cols = 2-seg through shared gap, distant = 4-waypoint through src/dst gaps + row-gap corridor), DawIn/DawOut (single cubic down nearest gap).
  - Two-pass lane allocation: count users per channel, assign `(i - (count-1)/2) * 6px` in stable edge order; DAW cables share pools with connection cables.
  - Gap selection: same-row -> gap between the two cols; cross-row -> gap toward destination (clamped [0,1]); DAW -> min(col,1) for input, max(col-1,0) for output.
- `rebuildCableCache()` now just calls `rebuildConnectionCables()`. Removed `buildSameRowCable`/`buildAdjacentColumnCable`/`buildDistantColumnCable`/`buildInputCableTo`/`buildOutputCableFrom` (internal only).

### Note: GranularProcessor.h was user-modified between sessions
- User replaced the continuous-cursor rewrite with a dual-grain-head design (readPtrA/B, grainBaseA/B, granularSpray, computeGrainBase with speed-aware offset cap). It did not compile (call sites passed 5 args to a 6-arg function). Fixed the 3 call sites to pass `playbackSpeed`.

### Verification
- Release VST3 + Standalone + drawdio_tests all green (tests now 28 cases — user added granular tests).

## 2026-08-29 — Jack highlighting + grabbed-cable preview fix

### Jack highlighting (manual mode, drag active)
- `ManualRoutingController::validTargetJackIndices()`: polarity = NewCable ? !src.isInput : m_grabbingSrcEnd. Pedal targets: moving output -> input jacks, moving input -> output jacks; excludes source/grabbed/anchored pedals + inputs with hasDawIn / outputs with hasDawOut (mirrors mouseUp rejections). DAW entry/exit are targets only for NewCable drags started from a pedal jack (grabs + DAW-started cables can't drop on DAW jacks per reconnectGrabbedCable).
- `IThemeProvider::jackHighlightColour()` (ThemeManager: 0xFF50F07E); `CableRenderer::drawJackHighlight()` (soft 40px glow + 30px ring). Hooked in `PedalboardGrid::paintOverChildren()` after the drag wire; repaint already fires per drag tick.

### Grabbed-cable preview fix (from earlier approved plan)
- Exposed `isNewCableDrag()`/`isGrabDrag()` on the controller; reworked paintOverChildren:
  - drawInputJack skipped only when grabbing the DAW-in cable (was: `!isGrabbing` typo left the original DAW-in cable drawn while its end was dragged).
  - drawOutputJack skipped only when grabbing the DAW-out cable.
  - drawRoutingCables skips the grabbed edge during grabs.
  - drawGrabbedCable renders only during grabs (single preview wire for pedal-pedal + DAW grabs).
  - drawActiveDraggingCable renders only for new-cable drags (killed the double-wire on grabs).
- Net: exactly one wire at all times.

### Verification
- Release VST3 + Standalone + drawdio_tests (28 cases) all green.

## 2026-08-29 — Soft magnetic routing v2 (top-row gaps only, no crimps)

### User feedback on v1
- Magnetism too strong: cables crimped at the row x column intersections; cables exited jacks diagonally (single-cubic gap controls), not straight up; bottom-row gaps shouldn't be magnetic (bottom-row cables never route downward).

### Changes
- Channels: vertical channels now ONLY the 2 top-row gaps (bottom-row gaps removed) + the row-gap corridor. Cross-row routes use ONE top gap + the corridor (destination-side waypoint sits directly above the destination jack - removes the row x column waypoint corner).
- Route kinds now 8 (SameRowTop/Bottom, CrossRowTopDown/BottomUp, DawInTop/Bottom, DawOutTop/Bottom), all built via buildWaypointCable/Path with perpendicular jack tangents (straight-up exits, arrivals from above):
  - SameRowTop: dome through the top gap (lift = min(h*0.3, 70)).
  - SameRowBottom: plain dome at midpoint, no channel.
  - CrossRowTopDown: [p1, (srcGap+laneA, corr+laneH), (p2.x, corr+laneH), p2].
  - CrossRowBottomUp: [p1, (p1.x, corr+laneH), (dstGap+laneB, corr+laneH), (dstGap+laneB, p2.y-lift), p2] (corridor run confirmed by user).
  - DAW: 4 variants; bottom destinations descend the gap to the corridor then drop straight onto the jack; CableArcLiftPx (40) for bend-above-jack waypoints.
- `CablePathBuilder`: refactored into shared `appendWaypointSegments` + new `buildWaypointPath` (continuous Path for DAW cables); `buildWaypointCable` (split halves) unchanged in behavior. Interior corners are chord-tangent (C1 continuous - no sharp corners); endpoints perpendicular.
- GridLayout: added `CableArcLiftPx = 40.0f`.

### Verification
- Release VST3 + Standalone + drawdio_tests (29 cases) all green. Pending user visual check: no crimps, smooth upward jack exits, top-gap routing only.

## 2026-08-29 — Backend audit fixes (verified plan, 5 changes)

### Verification pass outcome (previous plan corrected)
- A1 "crossfade CAS-failure silent block" was REFUTED during verification: the completion check runs AFTER the block is processed and gained (UnifiedPedalProcessor.cpp:289-297); the failure branch is only reachable in the benign next==current case. No fix applied.
- B4 reverb: confirmed mono-FDN + decorrelation (ReverbNetwork.cpp:79 monoIn; lines 127-132 lossy decorrelator). Design choice, not a bug; stereo-FDN redesign decision left to the user.

### Changes applied
1. `CanvasMessageQueue::pushSnapshot` — latest-wins overwrite: when the ring is full, overwrite the newest slot (writeIdx-1) and re-store writeIdx with release (same value). Validated: consumer can't be mid-copy of the newest slot while full (N=8); release-acquire synchronization holds per cppreference (load reads-from last store in modification order). Kills the stale-config window where the final canvas state was dropped and never compiled.
2. `DspEffect::processBlock` — made pure-virtual (default passed params[0], which is now the Mix knob on every effect — silent-miswire footgun). All 26 effects already override; build confirms.
3. `UnifiedPedalProcessor::processChainBlock` — loop now also bounds `idx < config.effects.size()`.
4. `ParameterCache` update/store/applyOffset — values clamped to [0,1] (isfinite guard added to store; std::clamp passes NaN, so isfinite must come first). Offsets untouched (display reconciliation).
5. `interpolateDelayRead` — negative-pos wrap (fmod + add n) before the float->size_t casts (kills UB landmine; identity for valid callers).

### Verification
- Release VST3 + Standalone build clean, zero warnings; drawdio_tests 29 cases / 4,762,874 assertions — identical count to pre-change (no behavioral drift).

## 2026-08-29 — ReleaseQueue deferred delete + undo cap

- **M1 (UAF on overflow):** `ReleaseQueue::push` deleted the payload immediately when the 16-slot ring was full — could delete an empty-chain `oldCurrent` the audio thread was still processing in its block (block <= 23ms, exchange+delete microseconds apart). Fixed: displaced pointers now go to a new `m_pendingDelete` atomic slot; `drain()` deletes it (>=50ms later, after any in-flight block). No delete on the audio thread.
- **M2 (overflow leak):** `pushSingle` discarded the previous `m_overflow` pointer without freeing (counted only). Fixed: displaced overflow pointer routed through `m_pendingDelete`.
- Residual: two overflows within one 50ms tick displace an older pending pointer -> counted in `droppedCount` (practically unreachable; bounded + observable).
- **Undo cap:** `MaxUndoLevels` 32 -> 64 (user preference: larger undo reserve); `MaxUndoBytes` 8MB stays as the hard bound (stack already trimmed on both in stroke-commit and flood-fill paths).
- Note: the user renamed `GridLayout::CableJackHeight` -> `CableJackHeightRatio` (grid-height fraction) and refactored `ColorPalette` API (`setImageBottomShift`/`setContentCenterOffset` -> `setPedalBottomRatio`/`setImageCenterX`) concurrently with this work — transient build errors during the overlap; settled tree builds clean.
- Verification: Release VST3 + Standalone clean, zero warnings; 29 tests / 4,762,874 assertions unchanged.

## 2026-08-29 — Effect overhaul: GrainScrubber fix, ReTime halftime, HP/LP Filter pedal, universal Mix knob

### Grain Scrubber root cause (the "sounds off" bug)
- Old `processGranularSample` re-anchored `grainBase` to the live write pointer and re-seeded `grainPhase2 = 0.5*grainLen` at EVERY grain restart. Because the ring advanced `grainLen/speed` samples between restarts, grain 2's read position jumped `grainLen/speed` samples (0.08-0.16s of material) at full window amplitude each cycle (~every 0.04-0.16s) -> periodic water/ticks. Hann envelopes summed to unity (no amplitude click) but the CONTENT was discontinuous.
- Fix: per-channel continuous `readCursor`; grain 1 reads `cursor`, grain 2 reads `cursor + 0.5*grainLen`; windows indexed by `fmod(cursor, grainLen)` (Hann sum == 1 and content continuous at ANY speed, provably). Position knob maps monotonically to target age `[grainLen, bufSize - 1.5*grainLen]` with a per-sample glide (0.01 alpha) — fixes the old non-monotonic mapping where knob end re-read newest material. Age clamped ahead of write head. Dropped spray/`grainBase`/`rngState`. Also fixes the same discontinuity class in GranularDelay + PitchShifter.

### ReTime redesign (HalfTime-style)
- Old: live-ring read at continuous speed with beat-quantized loops; write head overwrote long loops mid-read; no filters/bars.
- New: 16s live ring + per-channel freeze buffer. On sync (transport start, knob change, or loop-end wrap while playing/unknown transport) the last `loopLength` samples are copied (bar-quantized via ppq) into the freeze buffer (memcpy, RT-safe, no alloc) and read at the ratio with continuous phase wrap; `m_phase = shift*loopLength` at capture.
- Knobs: Mix[0] (processor wet/dry), Time[1] quantized {1/4, 1/2, 3/4, 1} (midpoint = half-time), Bars[2] quantized {1, 2, 4}, Shift[3] (start offset in captured loop). Smooth knob removed (fixed alpha 0.5).

### HP/LP Filter pedal (new, slot 22)
- `RESERVED_REMOVED_RANDOM_MODULATOR` -> `HP_LP_FILTER` (value 22 kept; old presets clamped to BYPASS previously, now load the filter). Serializer clamp for 22 removed (26 stays clamped).
- New `HpLpFilterEffect.{h,cpp}`: TDF-II biquads HP (log 30Hz-2kHz, knob "High") + LP (log 500Hz-16kHz, knob "Low") in series, shared Q ("Reso", 0.5-10). Mix[0]. Added to factory + popup Filter category {3,9,20,22} + CMakeLists.

### Universal Mix knob (labels + mixKnobIndex only; param indices preserved -> presets safe)
- New mix: Wave Shaper(0), Multi-Mode(1), Pitch Shifter(0), Glitch(1), Wave Folder(0), Comb(1), Freq Shifter(1), Grain Scrubber(1), VCA(1), Formant(0), Resonant Filter(3), Resampler(0).
- Baked params to free slots: VCA Release (fixed 120ms); Formant "Level" was already dead — Shift (params[1], +2400Hz) and Q (params[3], band narrowing) now WIRED (were inert); Resonant Filter "Level" was already dead (params[3] unused); Resampler Dither always on (dither knob removed; Rate[1]/Bits[2]/Filter[3] moved one slot).

### Knob-position schema
- `knobLayoutForCount(count)` in PedalDefinition.{h,cpp}: 1 = single center, 2 = centered column, 3 = triangle (bottom centered), 4 = square, 5 = die (4 corners + center pip). `PedalDefinition.knobLayout` member removed; `knobCount` = number of non-empty labels. `PedalComponent` renders only labeled knobs (empty-label slots hidden), schema positions assigned to non-blank slots in index order; single-knob spread guard removed (scaleX=1 when spread < 0.001).

### Verification
- Release build: VST3 + Standalone clean. `drawdio_tests` Debug + Release: 22 cases / 61 assertions pass (ReTime/GranularPitch warm-up tests unaffected).
- Pending manual: scrubber clicks across rate range, ReTime 1/4-1x + Shift, filter sweep, knob schemas visually, preset round-trip.

## 2026-08-27 — Headless Catch2 unit-test suite for DSP effects

### Goal
- Build a vertical-slice unit-test suite for the VST audio effects, headless (no GUI/plugin host), using Catch2 via FetchContent and only the needed JUCE modules.

### What was added (only test files + minimal CMake)
- `tests/JuceHeader.h`: shim for the plugin's generated `JuceHeader.h` so the
  Effects `.cpp` files compile in the test target. Pulls in only
  `juce_core`, `juce_audio_basics`, `juce_dsp` (the Effects only use
  `juce::ScopedNoDenormals`, `juce::jlimit`, `juce::dsp::FFT`).
- `tests/main.cpp`: Catch2 session entry point (links `Catch2::Catch2`, not
  `WithMain`, so no duplicate `main`).
- `tests/test_DspEffectFactory.cpp`: factory returns non-null for known ids,
  correct concrete type per id (incl. expanded effects), null for BYPASS/unknown.
- `tests/test_FilterEffects.cpp`: MultiModeFilter non-silent/finite + cutoff and
  mode params have an effect.
- `tests/test_DelayEffects.cpp`: SimpleDelay delayed/non-silent/finite + feedback
  tail energy + MicroPitchChorus non-silent.
- `tests/test_ExpandedEffects.cpp`: Distortion (Waveshaper/Wavefolder/CombResonator),
  Reverb (Diffused/Plate), Modulation (Tremolo/Flanger), Pitch (GranularPitch/
  FrequencyShifter/GlitchStutter), ReTime — each non-silent/finite and a
  parameter-sensitivity check.

### CMake changes (CMakeLists.txt)
- Appended an `if(DRAWDIO_BUILD_TESTS)` block (default ON): FetchContent Catch2
  v3.5.2, a `drawdio_tests` executable compiling `Source/Effects/*.cpp` +
  `Source/Dsp/DspEffectFactory.cpp` + `Source/Dsp/ReverbNetwork.cpp` +
  `Source/State/EffectConfigRegistry.cpp` (ReverbNetwork depends on these two;
  no other State/UI deps), linked to `juce::juce_audio_basics`,
  `juce::juce_dsp`, `juce::juce_core`, `Catch2::Catch2`. `enable_testing()` +
  `add_test`.

### Build / test commands that worked (reuse existing MSVC build dir)
- `cmake --build build --target drawdio_tests --config Debug`
- `ctest --test-dir build --output-on-failure -C Debug`
- Generator is "Visual Studio 18 2026" (VS 2022 Build Tools, x64). JUCE 8.0.4
  was already fetched into `build/_deps`, so no re-fetch; Catch2 fetched on
  first configure. Network (github.com) is available.

### Environment notes / gotchas
- No git commits made; no existing source files modified except CMakeLists.txt.
  `Source/Effects/ReTimeEffect.cpp/.h`, `docs/*`, `updater.*`, `README.md`,
  `PluginEditor.cpp`, UI files left untouched.
- Effects `.cpp` include `<JuceHeader.h>` (plugin-generated). The shim resolves it.
- Effects `.cpp` include their own headers via the including file's directory
  (MSVC quoted-include rule); `Source` on the include path covers the rest.
- Test failures were all "output == 0" logic/setup issues, not corruption:
  - CombResonator delay (~2205 samples @20 Hz) exceeded my 2048-sample block.
  - GranularPitch / ReTime need warm-up past their internal buffer / looper
    window before replayed audio appears (used n=30000 / n=400000).
  - Reverb decay knob index differs: Diffused=3, Plate=2; at decay 0 the
    Diffused wet output is ~0, so the non-silent check is done on the
    high-decay buffer.
  - Catch2 default test ordering is random, so transient zeros surfaced as
    different failing lines per run until each effect's block length / decay
    param was corrected.

### Results
- 22 test cases, 61 assertions, all passing (confirmed over multiple runs).



## 2026-08-22 — Import Dithering

### Decision
- User goal: reduce import resolution loss while retaining original colors. Analysis showed the bottleneck is per-cell quantization (no error diffusion), not palette size; lowering resolution increases color loss.
- Floyd-Steinberg dithering chosen over palette expansion: serialized byte budget 0-12 is fully consumed, so new colors would force format v0x06 + UI/DSP changes. Dithering also synergizes with DSP: CanvasAnalysis averages `colorWeight` values, so dither mixes compile as intermediate parameter values (intentional behavior change to compiled params).

### Implementation
- `Source/PluginEditor.cpp`: `nearestDrawdioColor(juce::Colour)` replaced by `findNearestPaletteEntry(float,float,float)` (same 2/4/3-weighted squared-RGB metric, returns full PaletteEntry for error math) + `ditherImageToGrid()`.
- Pipeline unchanged up to rescale (`256x256`, highResamplingQuality); direct per-pixel map replaced with serpentine FS pass: weights 7/16 forward, 3/16 back-down, 5/16 down, 1/16 ahead-down, mirrored on odd rows; working RGB clamped [0,255]; alpha<128 cells emit 0 and neither receive nor propagate error; ~768KB scratch buffer, UI thread only.

### Verification
- Release Standalone + VST3 build clean.
- Pending manual check: gradient image imports should show smooth blends instead of posterized bands.

## 2026-08-22 — Windows UI Graininess Audit + Fix

### Root cause
- JUCE 8 on Windows renders via Direct2D; default `mediumResamplingQuality` maps to `D2D1_INTERPOLATION_MODE_LINEAR` (bilinear only). No `setImageResamplingQuality` calls existed anywhere in `Source/`. macOS CoreGraphics default looked fine, hence "grainy after porting".
- Knob: 1024×1024 `Knob_Generic_alpha_cutout.png` rotated + ~23× minified per repaint (`SpriteKnob.h`). LED: 150×150 frames from `ledonoff.png` drawn at ~17px with fractional position (`PedalComponent.cpp`).
- User-confirmed symptom scope: knob sprites + LED only. Canvas texture stretch, colorwell DPI magnification, buffered-to-image softening — all verified present but NOT symptomatic; left untouched per user decision.

### Implementation
1. `SpriteKnob.h`: highResamplingQuality flag; pre-scale cache — 1024² sprite rescaled once to `round(diam × deviceScale)` px via `Image::rescaled(..., high)`, only the small copy is rotated per frame (`drawImageTransformed`, matrix scale = `diam / cachePx`; context transform maps back to exact device size → 1:1). Cache rebuilds only when physical target px changes (resize/DPI change), never during drags.
2. `PedalComponent.cpp/.h`: both LED frames extracted via `getClippedImage` + `rescaled(high)` into `m_ledScaled[2]`, keyed by physical px; destination snapped to device-pixel grid (`round(logical*scale)/scale`) so blit is 1:1.
3. `CMakeLists.txt`: `JucePlugin_EditorHeight` 800→900 matching `GridLayout::DesignResolution::Height`.

### Key API facts (verified in pinned JUCE 8 source)
- Device scale inside paint: `g.getInternalContext().getPhysicalPixelScaleFactor()`. There is NO `Graphics::getInternalScaleFactor()`; `Component::getDesktopScaleFactor()` only applies to desktop components; peer scale is `ComponentPeer::getPlatformScaleFactor()`.
- `Graphics::drawImage` has NO (img,x,y,w,h) overload — use `Rectangle<float>` form or the 9-arg source-rect form.
- D2D quality mapping: medium→LINEAR, high→HIGH_QUALITY_CUBIC, low→NEAREST_NEIGHBOR (`juce_Direct2DGraphicsContext_windows.cpp`).
- Buffered-to-image (`setBufferedToImage(true)` on PedalComponent/ColorPalette): caches own paint only, not children; kept as-is by decision.

### Verification
- `cmake --build build --config Release --parallel 2` — Standalone exe + VST3 built clean (one intermediate fix: drawImage rect overload).
- Pending: manual visual check of knobs/LED while dragging at Windows 100%/125%/150% scaling.

## Previous work (for context)## 2026-08-14 — Cable Routing Plan (implemented in v0.2.1)

### Problem
- Persistent cables are painted above every pedal in `PedalboardGrid::paintOverChildren()` (Source/PedalboardGrid.cpp:25-49); shadow stroke 8 px, main 4.8 px, highlight 1.4 px (CableRenderer.cpp:7-26).
- Only the top row is visually obstructed; no cables cross the bottom pedals (cross-row cables already use the row gap).
- Same-row routes are not obstacle-aware: `makeSameRowControlPoints()` lifts only up to 10 px (CablePathBuilder.cpp:22-34).
- Multiple cables share a gap receive no lane allocation — identical geometry stacks.
- Current `ColumnGapRatio = 0.005f` (GridLayout.h:20) → ~4 px gaps at design width, narrower than the cable shadow.

### Constraints (user-approved)
- Both: widen inter-pedal horizontal spacing AND magnetic gap routing.
- Slight cable overlap with pedal bodies is acceptable.
- Z-order stays as-is (cables + jacks above pedals).
- No change to internal knob spread (`KnobSpreadRatio`); the gaps provide the clearance.

### Changes
1. **Source/GridLayout.h**
   - Raise `ColumnGapRatio` from `0.005f` to `0.03f` (~23 px channels at design width; ~30 px if 0.04f). Pedal width stays clamped at `PedalWidthMaxRatio` at design size.
   - Add cable constants: `CableLaneSpacingRatio` (~0.008f) and `CableChannelMarginRatio` for shadow clearance (shadow stroke is 8 px).

2. **Lane-based magnetic router — CablePathBuilder + PedalboardGrid glue**
   - Compute channel centers from live pedal bounds: two vertical gaps per row (`gapCenter(col0-1)`, `gapCenter(col1-2)`), plus horizontal channels at the top margin and row gap.
   - Route by case:
     - Same-row adjacent columns → vertical lane in the shared gap.
     - Same-row distant (0→2) → horizontal channel above the row.
     - Cross-row → vertical lane in source gap → row-gap channel → vertical lane in target gap.
     - DAW in/out cables → snap toward nearest gap/top channel (replaces fixed 30 px offset in `buildInputCable`/`buildOutputCable`).
   - Lane allocation: per channel, lane offset = `(laneIndex - (count-1)/2) * laneSpacing` (~5 px) so multiple cables fan out.
   - Smooth path: waypoints + cubic segments with tangent extensions for rounded corners (preserve Bezier look).
   - Replace internals of `buildSameRowCable` / `buildAdjacentColumnCable` / `buildDistantColumnCable`; `CachedSplitCable` split behavior and hit-testing unchanged.

3. **Files touched**: `Source/GridLayout.h`, `Source/UI/Pedalboard/CablePathBuilder.{h,cpp}`, `Source/PedalboardGrid.cpp`.

### Verification
- `cmake --build build --parallel 2`, `git diff --check`.
- Manual: all top-row routing combos (same-row, adjacent, distant) with 3+ cables sharing a gap; confirm knobs/labels unobstructed; verify jack dragging, DAW in/out cables, resize down to minimum window size.

## Previous work (for context)
- v0.2.1 committed and pushed: public repo robP22/Drawdio (e0536d5), private robP22/drawdio_dev (4212456). GitHub release v0.2.1 with macOS VST3 + Standalone zips.
- Jack sprite render fix: centered transform, isolated opacity state, horizontal-only output mirror (CableRenderer.cpp drawJack).
- Mixer bounds fix: MeterTrackGapRatio 0.05, gap excluded from slider bounds, travel-inset thumb clamping, PedalGainMin/Max constants in DrawdioConstants.h, defensive clamp in PedalState::setPedalGain.
## 2026-08-14 Windows build (drawdio_dev @ 36a11ff)
- First Windows test build: VS Build Tools 2026 (MSVC 14.51) + CMake 4.3.3, generator "Visual Studio 18 2026" x64.
- Blocker: EffectConfigRegistry.cpp uses C++20 designated initializers (`.feedbackBase = ...`); MSVC C7555 under /std:c++17. The current CMake requirement is C++20.
- Both targets build clean in Release: `build/Drawdio_artefacts/Release/Standalone/Drawdio.exe`, `.../VST3/Drawdio.vst3`.
- VST3 copied to `C:\Program Files\Common Files\VST3\` (overwrote earlier install).
- Standalone smoke test: launches, stays alive, ~147MB working set, no crash.

## 2026-08-22 Audio Pipeline Audit
- Completed report-only static audit of all 24 creatable effects, the real-time/configuration pipeline, and documentation.
- Report written to `docs/audits/audio-pipeline-audit-2026-08-22.md`.
- No source changes or build verification performed for this audit.
- Highest-priority findings retained after review: granular unsigned arithmetic underflow, reverse-repeat hard splice, and mono reverb output. The spectral-freeze loop-wrap finding was subsequently withdrawn.
- Documentation drift included removed Random Modulator/Tape Stop sections, missing Re-Time documentation, stale reverb topology, and stale convolution/tail/optimization claims.

## 2026-08-22 Documentation Consolidation
- Product documentation now uses Drawdio v0.2.0 as the public product label.
- Current guides: `README.md`, `docs/effects.md`, `docs/architecture.md`, `docs/build.md`, `docs/ui-controls.md`, `docs/state-format.md`, and `docs/resources.md`.
- Historical audits and completed plans are under `docs/audits/` and `docs/archive/`.

## 2026-08-22 Plugin Metadata
- `CMakeLists.txt` now identifies the product/plugin as version `0.2.0`.
- JUCE metadata uses company `robP`, bundle ID `com.robp.drawdio`, manufacturer code `DrDd`, and plugin code `Draw`.
- The installed CMake version is a toolchain detail; the project minimum remains 3.24.

## 2026-08-22 Cross-Platform Updater
- `updater.sh` now detects macOS, Linux, and Windows Git Bash/MSYS2, always builds the Release VST3 target, and no longer calls macOS-only `sysctl`.
- Added native `updater.ps1` and `updater.cmd` entry points for Windows.
- Windows defaults to `%ProgramFiles%\\Common Files\\VST3` and falls back to `%LOCALAPPDATA%\\Programs\\Common\\VST3` when the system directory is not writable.
- Updaters validate and stage the complete VST3 bundle before replacement, with best-effort restoration if replacement fails.
- Verified Git Bash, PowerShell, Command Prompt launcher, custom paths containing spaces, Release output, and non-destructive reconfigure behavior on Windows.

## 2026-08-29 — Naming unification + backend cleanup (4 phases)

### Phase 0 — Naming (enum/class/display unified; integer values preserved -> presets safe)
- Enums: WAVESHAPER_DISTORTION->WAVESHAPER, PITCH_SHIFTER_GRANULAR->PITCH_SHIFTER, ENVELOPE_VCA_COMPRESSOR->VCA_COMPRESSOR, DIFFUSED_DELAY_NETWORK->DIFFUSED_REVERB, MATHEMATICAL_WAVEFOLDER->WAVEFOLDER, FORMANT_VOCAL_SHIFTER->FORMANT_SHIFTER, SIMPLE_DELAY->DELAY, REVERSE_BUFFER->REVERSE, CONVOLUTION_SPACE->CONVOLUTION_REVERB, RESAMPLE_BITCRUSH->BITCRUSHER.
- Classes: SimpleDelayEffect->DelayEffect, GranularPitchEffect->PitchShifterEffect, DynamicResonantFilter->FormantShifterEffect, TimeDomainFreezeEffect->SpectralFreezeEffect, ResamplerEffect->BitcrusherEffect, ConvolutionSpaceEffect->ConvolutionReverbEffect, ReverseBufferEffect->ReverseEffect (3 file renames + CMakeLists).
- Displays: "Simple Delay"->"Delay", "Wave Shaper"->"Waveshaper", "Wave Folder"->"Wavefolder", "Time Freeze"->"Spectral Freeze", "Resonant Filter"->"Spectral Filter", "Re-Time"->"ReTime", "Resampler"->"Bitcrusher", "Convolution Space"->"Convolution Reverb", "Reverse Buffer"->"Reverse".
- Tests updated (test_DelayEffects x3, test_DspEffectFactory x8, test_ExpandedEffects x5).

### Phase 1 — Trivial cleanups
- 4 dead GridLayout constants removed (CableJackRadiusRatio, CableBaseLiftRatio, CableMinCurveRatio, CableLabelFontSizeRatio).
- Unused ParameterTypes.h include dropped from CanvasRoutingManager.h.
- AutomationCompiler::compile dead pedalSlots param removed (+ DspModuleType include; caller cleaned).
- CombResonator processBlock + DelayEffect processSample fallback re-indented.

### Phase 2 — Consolidations
- PedalParameterDefinition embeds ParameterDescriptor + label via constexpr ctor (PedalDefinition.h/.cpp, CompilerEngine.cpp, PedalComponent.cpp).
- CompiledPedalConfig.h forward-declares DspEffect (Core no longer includes Effects); DspEffect.h added to CompilerThread.cpp, ReleaseQueue.cpp, CompilerEngine.cpp (payload-destroying TUs).

### Phase 3 — SpectralFreeze true freeze (hold semantics)
- Per-channel freezeBuf (1.0s) captured at freeze onset from the ring; frozen branch reads the FROZEN buffer at pitchRatio 0.25-2.0 with a tail-window wrap splice (last 32 samples blend into the loop start via hann; removed the old xfadePos/oldReadPos continuation machinery which pointed at the moving write head). Ring keeps writing live input (captures stay fresh); entry/exit fades unchanged.
- hasActiveTail() -> true + getTailLength() -> 2.0 (freeze state survives mix automation; host tail reporting honest).

### Verification
- Release VST3 + Standalone clean, zero warnings; drawdio_tests 29 cases / 4,762,874 assertions identical (no drift). Grep sweep for all old enum/class/display names: clean.

## 2026-08-29 — Canvas + palette centering (opaque-content alignment)
- User: palette white rectangle too low, canvas too high/left; target = palette texture center == canvas component center, canvas centered in its bounds.
- `EditorLayout`: added leftOpaqueRatio/rightOpaqueRatio (column scans).
- `PixelCanvasComponent`: texture ratios (top/bottom/left/right opaque) computed once in the ctor; computeCanvasLayout now centers the OPAQUE content on both axes (offsetX = (w-opaqueW)/2 - opaqueLeft, offsetY likewise) - robust to the texture's asymmetric transparent padding (fixes the "slightly left" look). Removed setContentTop/m_contentTopRatio/m_columnHeightPx (pedal-top pinning) and GridLayout CanvasCenterXShiftRatio/CanvasCenterYShiftRatio (dead). Pointer mapping unchanged (same layout source).
- `ColorPalette`: setPedalBottomRatio -> setCanvasCenterY(float) (parent coords); resized() shift = round(getY() + paletteH/2 - canvasCenterY) so the white texture's center lands on the canvas component center; blobs/wheel follow via the existing -shift + contentCenterOffset. setImageCenterX now fed the canvas component center X.
- `PluginEditor`: removed the pedal-top/bottom ratio computations + members (dead); resized passes pixelCanvasBounds centres to the palette.
- Verification: Release VST3 + Standalone clean, zero warnings; 33 tests pass (user added 4 more); grep sweep clean.

## 2026-08-29 — Three DSP bug fixes (mix dead-zone, SpectralFilter instability, ReTime idle recapture)

### A. Effects inert until a knob is adjusted
- Root cause: the canvas->param mapping (calculatePixelAccumulation bias) can clamp to 0 for negative-weight-heavy bands; with the mix knob now on every pedal, a compiled Mix = 0 made the effect fully dry until a knob drag wrote the cache override.
- Fix (CompilerEngine.cpp): the definition parameter labeled "Mix" is now compiled to a FIXED 1.0 (manual-only control, never canvas-derived); all other knobs map through normalized = 0.05 + 0.9*normalized (no dead zones; mapped through min/max ranges).

### B. Spectral Filter 'center' crushed/distorted audio
- Root cause: wrong TDF-II state update (z2 = b0*x - a2*y) collapsed the 2nd-order resonator into a 1st-order pole p = R^2 - 2R*cos(theta); |p| > 1 for R > ~0.41 near Nyquist (high centers + narrow width) -> unstable blow-up.
- Fix: proper TDF-II v = x - a1*z1 - a2*z2; y = b0*v; z2 = z1; z1 = v (poles at R*e^(+-jtheta), stable for all center/width) in both processBlock and processSample; R <= 0.995 clamp and non-finite guard kept.

### C. ReTime Time/Bars destroyed audio + startup clicks
- R1: entry condition (!m_isPlaying && !m_wasPlaying) was TRUE EVERY BLOCK while idle (stopped/no transport) -> recapture() every ~10ms reset m_phase = shift*loopLength -> phase never advanced -> static-sample output. Fixed with a one-shot m_hasCaptured flag (capture once, hold; transport start / knob changes recapture via m_needsSync; wrap-path recapture while playing/standalone unchanged). Removed the now-dead m_wasPlaying member.
- R2: clicks at start/capture boundaries came from the 0.5-alpha one-pole smoothing; replaced with a 15ms time-based constant (1 - exp(-1/(0.015*sr))) set in prepare.

### Verification
- Release VST3 + Standalone clean, zero warnings; 38 test cases / 4,763,081 assertions pass (user added 5 more tests).
