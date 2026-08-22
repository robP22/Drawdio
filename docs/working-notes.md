# Working Notes

This file is temporary project working memory, not a canonical product or
implementation reference. Current documentation is indexed from `README.md` and
the guides in this directory.

## Current state
- Windows sprite-graininess fix implemented (knob + LED). Both Release targets build clean; visual verification at 100/125/150% display scaling still pending user check.

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
