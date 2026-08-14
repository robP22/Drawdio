# Working Notes

## Current state
- Implementing the gap-lane cable routing plan (see below). Nothing implemented yet.

## 2026-08-14 — Cable Routing Plan (finalized)

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