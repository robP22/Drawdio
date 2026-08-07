# Drawdio Architecture & Design Decisions

This document is the deeper technical reference for Drawdio. The README covers the
public-facing overview; this document explains **why** the architecture is shaped
the way it is, with the design rationale behind each significant decision.

Related documents:
- [`effects.md`](./effects.md) — per-effect algorithm reference, guardrails, optimizations.
- `README.md` — features, build instructions, quick architecture overview.

---

## 1. Thread Model

Drawdio runs on three threads:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ UI Thread (message thread, 20 Hz timer)                                  │
│  ├── PixelCanvasComponent      drawing, undo/redo, flood fill             │
│  ├── ColorPalette              color blobs, arc buttons                   │
│  ├── PedalboardGrid            pedals, cable drag routing                 │
│  ├── BottomControlBar          mixer strips, gain knobs, automation graph │
│  ├── EditorSyncController      tick(): consumes compiled results,         │
│  │                             drains release queue, syncs knobs/peaks    │
│  └── ConfigManager             prebuildEffects(), loadPedalConfiguration  │
├─────────────────────────────────────────────────────────────────────────┤
│ Compiler Thread (background)                                             │
│  ├── CanvasMessageQueue        SPSC lock-free ring buffer (cap 8)         │
│  ├── PenDebouncer              300 ms idle gate                           │
│  └── compileCanvas()           grid → PedalAssetPayload                   │
├─────────────────────────────────────────────────────────────────────────┤
│ Audio Thread (processBlock)                                               │
│  ├── ScopedNoDenormals                                                   │
│  ├── processChainBlock()       iterates active routing chain              │
│  ├── Per-effect processBlock() batched inner loops                        │
│  ├── Per-pedal wet/dry crossfade (per-sample mix interpolation)           │
│  ├── Output limiting (softClip) + per-pedal gain                          │
│  ├── Peak meter reads (relaxed atomics)                                   │
│  └── Crossfade state machine (20 ms transition window)                    │
└─────────────────────────────────────────────────────────────────────────┘
```

**Design decision — why three threads?**
- The canvas compilation is a 65,536-cell analysis that runs in ~1–3 ms. Doing it on
  the UI thread would block drawing; doing it on the audio thread would violate
  real-time constraints. A dedicated compiler thread keeps both responsive.
- The audio thread is the only thread allowed to touch `processBlock`; everything
  else is staged through atomics or queues.

---

## 2. Config Lifecycle

The entire state of the pedalboard (which effects are active, their order, their
parameters) is captured in an immutable `PedalAssetPayload`.

```
 compileCanvas()                     (compiler thread)
      │  produces PedalAssetPayload
      ▼
 consumeCompiledResultIfAvailable()  (UI thread, 20 Hz tick)
      │
      ▼
 loadPedalConfiguration(payload)     (UI thread)
      │  prebuildEffects() creates+prepares effect instances,
      │  writes them + param pointers into the payload
      ▼
 m_nextConfig.store(payload)         (release ordering)
      │
      ▼
 Audio thread: crossfade old→new over 20 ms (equal-power)
      │
      ▼
 CAS nextConfig→null; exchange currentConfig→next;
      │  old config → ReleaseQueue.pushSingle()
      ▼
 UI thread: ReleaseQueue.drain()     deletes retired configs (and their effects)
```

### Why effects live inside the payload (the F1 refactor)

Originally, effect instances lived in `m_chainEffects` / `m_pendingEffects`
arrays in `ConfigManager`, and the audio thread swapped them with
`std::swap` after a crossfade. That design had a **data race**: the UI thread's
`prebuildEffects()` could write `m_pendingEffects[i]` in the window between the
audio thread's CAS-null of `nextConfig` and its array swap — corrupting the
chain and risking use-after-free.

The fix was structural: **move the effect instances and the param-pointer table
into the `PedalAssetPayload` itself**. The payload is built and prepared on the
UI thread, then published with one atomic release store. Once visible to the
audio thread it is immutable. The config pointer exchange *is* the swap — there
are no separate effect arrays to race on. This eliminated three bug classes at
once:

- unique_ptr array race (UI writes while audio reads/swaps)
- non-atomic param pointer table (`m_paramPtrs` was a plain array)
- crossfade passes reading the *next* config's params for the *current* chain

The audio thread now only ever dereferences `const PedalAssetPayload*` pointers
it loaded from atomics, and never mutates `std::unique_ptr` objects.

### ReleaseQueue — deferred deletion

The audio thread must never `delete`. When a config is retired, the audio thread
does `releaseQueue.pushSingle(oldCurrent)` — a lock-free 8-slot array of atomic
pointers. The UI thread calls `drain()` on its 20 Hz tick and physically deletes.
If all 8 slots are occupied, `pushSingle` increments a dropped counter instead of
deleting (the audio thread must never deallocate); the dropped counter is
diagnostic only.

---

## 3. Parameter Pipeline

Knob/parameter values flow through a chain of stages before reaching an effect:

```
compiler: ParameterDescriptor.currentValue   (in the immutable payload)
                │
                ▼
paramPtrs[row][k] ─────────────────────────┐  (pointer into payload)
                                           ▼
processChainBlock: params[4] per slot ──►  parameter cache override?
                                           │   (ParameterCache atomics,
                                           │    UI knob drags / presets)
                                           ▼
                                        automation blend?
                                           │   (knobLinked? blend with
                                           │    smoothed automation value)
                                           ▼
                                        one-pole smoothing (40 Hz)
                                           │   (skipped for the mix knob,
                                           │    which has a per-sample ramp)
                                           ▼
                                    effect->processBlock(b, c, n, params)
```

### Why 40 Hz block-rate smoothing

Parameters are read once per block (not per sample). Without smoothing, a knob
drag produces a staircase at block rate (~50 Hz at 512-sample blocks) — audible
zipper noise. A 40 Hz one-pole smoother turns the staircase into a glide with
~25 ms response, inaudible as zipper while still tracking fast gestures.

The **mix knob** is excluded from the smoother because it already has a
per-sample linear ramp inside the wet/dry blend (`m_prevMix → currMix`
interpolation across the block), which is cheaper and just as click-free.

### Automation blending

Automation (from canvas Y-position, 64 slices) is smoothed at 12 Hz and blended
per-knob with a configurable link strength. The blend happens *before* the
40 Hz smoother, so the smoothed target already includes the automation
contribution.

---

## 4. Crossfade State Machine

```
                +--------------------+
                |  idle (no next)    |
                +--------------------+
                     │
      new config published (m_nextConfig set, crossfadeReset)
                     ▼
      +--------------------------------------+
      |  ACTIVE: 20 ms equal-power fade      |
      |  copy input → process *current       |
      |  capture old output → restore input  |
      |  → process *next → fade outputs      |
      +--------------------------------------+
                     │ counter >= samples
                     ▼
      +--------------------------------------+
      |  COMPLETE: process *next only        |
      |  (single pass until swap fires)      |
      +--------------------------------------+
                     │ counter >= samples AND
                     │   nextConfig CAS-null succeeds
                     ▼
      +--------------------------------------+
      |  SWAP: currentConfig ← next          |
      |  old → release queue; reset effects  |
      +--------------------------------------+
```

### Why equal-power (cos/sin) fade

The two chains being crossfaded are effectively uncorrelated signals (different
effect topologies). A linear ramp between uncorrelated signals dips ~3 dB at
midpoint. The equal-power fade uses `gOld = cos(g·π/2)`, `gNew = sin(g·π/2)`,
so `gOld² + gNew² = 1` — constant power throughout the transition.

### Why 20 ms

20 ms (882 samples @ 44.1 kHz) is short enough to feel instantaneous for preset
changes but long enough to avoid zipper/click artifacts from the topology change.

### Swap atomicity

The swap is a single CAS on `nextConfig`; if the CAS fails (a newer config was
published mid-fade), the crossfade resets and the fade restarts toward the newer
config. The deferred-config mechanism in `ConfigManager` queues a config that
arrives while another is still fading. A guard clears `nextConfig` when it is
pointer-identical to `currentConfig` — this can never happen today (configs are
always freshly built), but protects the completion exchange from self-freeing a
live config if the invariant ever changes.

---

## 5. Drift / Unstable Modulation

A per-pedal analog-warmth system implemented in `UnifiedPedalProcessor` (not in
`DspEffect` — effect instances are ephemeral and recreated on every config
reload; drift state must survive pedal-type changes).

### Why in the processor

| Approach | Problem |
|---|---|
| State in `DspEffect` | Instances are destroyed/recreated per config compile → drift resets to zero on every canvas change |
| State in `DspEffect` | The base class can't know which params are "primary" per effect |
| State in processor, indexed by `physSlot` | Survives config changes; per-slot independence (real analog gear has independent component tolerances) |

### Signal design

Each of the 6 slots owns a `DriftModulator` with two independent random-walk
channels:

| Channel | Rate | Smoothing τ | Max depth |
|---|---|---|---|
| **drift** (slow) | 0.3–3.0 Hz (scales with amount) | 0.5–2.0 s | ±2% of parameter |
| **unstable** (fast) | 6.0–15.0 Hz | 80 ms | ±0.8% |

Both use xorshift32 (lock-free, deterministic, per-slot seeds — no two slots
correlate). On phase wrap a new target is drawn; the output value is one-pole
smoothed toward it per block, producing a natural filtered-noise wander.
Per-block update cost is ~6 ops per slot — negligible next to the DSP itself.

### Application

In `processChainBlock`, after the 40 Hz parameter smoother and *before*
`effectPtr->processBlock`:

```
mod = drift.value + unstable.value
for each knob k (k != mixKnobIndex):
    params[k] = clamp(params[k] + mod, 0, 1)
```

- Mix knob is excluded (its per-sample ramp already handles zipper-free motion).
- Additive modulation with clamping; at amount 0 the entire path is skipped.
- Amounts are `std::atomic<float>` per slot with release/acquire ordering
  (`setDriftAmount` from any thread, audio thread reads each block).

---

## 6. Effect Ownership & Real-Time Safety

### Zero heap allocation on the audio thread

| Resource | Created | Freed |
|---|---|---|
| Effect instances | `prebuildEffects()` on UI thread | via `ReleaseQueue::drain()` on UI thread |
| Delay/reverb/FFT buffers | effect `prepare()` on UI thread | effect destructor (UI thread) |
| `m_dryBuffer`, crossfade buffers | `prepareToPlay()` | destructor |
| Per-block temporaries | stack | stack |

The audio thread performs no `new`, no `vector::resize`, no `unique_ptr`
mutation, no `shared_ptr` copies. All state hand-offs are `std::atomic` with
explicit memory ordering (`release` for publish, `acquire` for read).

### Denormal protection

Every DSP entry point constructs `juce::ScopedNoDenormals` as its first
statement: `PluginProcessor::processBlock`, `processAudioBlock`,
`processChainBlock`, and every effect's `processBlock`. Effects whose
`processBlock` forwards to a per-sample helper removed the redundant per-sample
guard — the block-level fence covers the entire loop. A handful of effects keep
the per-sample guard for standalone `processSample` correctness (dead code on
the audio path but safe if ever called directly).

### NaN/Inf containment

NaN guards are layered:

1. **Input guard** — most effects check `std::isfinite` on samples read from
   the input buffer before writing to delay lines.
2. **Shared read path** — `interpolateDelayRead()` (the common interpolated
   delay-line read used by SimpleDelay, MicroPitch, TapeStop, Granular, Reverb
   comb reads) zeroes any non-finite result.
3. **Feedback paths** — CombResonator sanitizes the delayed read; reverb
   comb feedback is tanh-bounded.
4. **Filter state** — TDF-II biquad states (DynamicResonant, SpectralFilter)
   reset `y, z1, z2` to 0 on non-finite output, so a NaN can't persist in
   near-undamped state.
5. **Final stage** — `processAudioBlock` converts any non-finite output to 0.

This means a NaN anywhere upstream becomes silence rather than propagating
through the graph.

### The output stage: softClip, not tanh

The final output limiter is a **unity-gain soft-knee clipper**:

```
softClip(x): |x| <= 0.85 → x
             |x| >  0.85 → knee + (1-knee)·tanh((|x|-knee)/(1-knee))
```

Below the knee it's identity — a bypass chain is transparent. Above the knee it
compresses smoothly to ±1. The derivative is continuous at the knee (both sides
evaluate to 1), so there's no harmonic discontinuity. The original implementation
used full-range `tanh`, which audibly compressed even unity-gain material — that
was a sound-quality bug, not a design choice.

---

## 7. Design Decision Rationale

| Decision | Rationale |
|---|---|
| **Config-owned effects** | Eliminates the effect-array data race class (see §2). |
| **Block-rate parameter smoothing (40 Hz)** | Removes zipper noise at ~50 Hz block rate with ~25 ms glide (§3). |
| **Mix knob: per-sample ramp, not smoother** | The wet/dry blend already interpolates per-sample; cheaper and click-free (§3). |
| **Equal-power crossfade** | Constant power through uncorrelated topology transition (§4). |
| **Per-channel stereo delay lines** | Stereo image preserved through every delay/ring/tape/granular effect; no mono-summing anywhere. |
| **Rhythm Gate: single shared phase + 1 ms smoothing** | One phase accumulator drives all channels so L/R gating stays sample-aligned (the old per-channel-timer ducking wobbled the stereo image); 1 ms one-pole envelope smoothing makes hard gate transitions click-free while keeping rate responsiveness. |
| **Dual-grain granular (50% overlap, Hann)** | Two grains at half-grain offset with Hann windows sum to unity — zero amplitude modulation at grain boundaries. Single-grain designs with 100% window modulation "tapped" audibly. |
| **ADAA antiderivative (Waveshaper/Wavefolder)** | Antiderivative integration suppresses aliasing from the nonlinear curve without oversampling; the `dx > 1e-10` guard keeps the integral well-defined at DC. |
| **Direct convolution (512-tap, FFT-based)** | 512-tap IR is short enough that overlap-add FFT at 1024-frame blocks is cheap; the synthetic IR is normalized to peak 0.833 to prevent clipping. |
| **SPSC lock-free ring buffer (CanvasMessageQueue)** | Single producer (UI) / single consumer (compiler) with explicit fences; last-write-wins drops are safe because every message is a full grid snapshot. |
| **Deferred deletion (ReleaseQueue)** | Audio thread must never `free`; UI thread drains on its tick (§2). |
| **Unity-gain softClip output** | Bypass transparency + smooth limiting; C1-continuous at the knee (§6). |
| **20 ms crossfade window** | Instant-feeling preset changes without topology-change clicks (§4). |
| **12 Hz automation smoothing** | DAW-synced envelope is already slow-moving; 12 Hz removes staircase without lagging the visual playhead. |
| **Sub-block coefficient updates (every 8 samples)** | DynamicResonant biquad coefficients track a slow envelope; updating every 8 samples (~0.18 ms) is inaudible and cuts transcendental cost 8×. |
| **Undo: 8 MB byte budget + 32 levels** | Full-canvas transactions are 256 KB; a byte budget bounds worst-case memory while the level cap keeps undo semantics predictable. |
| **Drift in the processor, not the effect** | Effects are ephemeral (recreated per compile); per-slot processor state survives config changes and keeps pedals independent (§5). |
| **Cables above pedals (`paintOverChildren`)** | JUCE paints children after `paint()`; moving cable drawing to `paintOverChildren` renders cables on top of pedal enclosures, with the shadow landing on the pedals for depth. |

---

## 8. Guardrail Catalog

Where the key safety mechanisms live:

| Guardrail | Location |
|---|---|
| Denormal fence (plugin entry) | `PluginProcessor::processBlock` |
| Denormal fence (chain entry) | `UnifiedPedalProcessor::processChainBlock` |
| Denormal fence (per-effect) | every effect `processBlock` |
| Input NaN guard | per-effect input reads (delay writes, filter inputs) |
| Shared read NaN guard | `interpolateDelayRead` in `DelayPrimitives.h` |
| Feedback NaN guard | CombResonator, ReverbNetwork comb feedback |
| Biquad state reset on NaN | DynamicResonantFilter, SpectralFilterEffect |
| Output non-finite→0 | `UnifiedPedalProcessor::processAudioBlock` |
| Channel bounds | `std::min(c, state.size())` in every per-channel loop |
| Division-by-zero guards | `bufSize == 0` checks, `sliceSamples ≥ 2`, `delaySamples` clamped, `R` clamps |
| Feedback stability | tanh-bounded feedback, `R ≤ 0.995` pole-radius clamp |
| Zero heap on audio thread | structural — all allocation on UI thread (§6) |
| Lock-free hand-offs | `std::atomic` with release/acquire; SPSC queue with explicit fences |

---

## 9. Known Simplifications & Trade-offs

- **Unused knobs**: all pedals render 4 knobs for visual consistency, but many
  effects read only a subset of `params[0..3]`. This is intentional (uniform UI);
  the per-effect doc lists exactly which knobs are wired.
- **No oversampling**: the distortion family relies on ADAA instead of
  oversampling (cheaper, no latency).
- **No lookahead limiter**: the output stage is a soft clipper, not a true
  lookahead limiter. For a creative/experimental plugin this is acceptable;
  loud transients get a smooth clip rather than gain-reduction ducking.
- **Direct convolution only**: no IR import; the "space" is a synthetic,
  seeded exponential-decay noise IR (512 taps).
- **Tail length**: `getTailLengthSeconds()` returns 5.0 s to cover the longest
  delay (2 s) + reverb decay.
