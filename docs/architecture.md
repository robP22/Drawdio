# Drawdio Architecture

This document describes the current thread model, canvas compilation pipeline,
configuration lifecycle, parameter flow, audio processing, and known runtime
limitations. The active effect reference is [`effects.md`](./effects.md).

## 1. Thread Model

Drawdio uses three cooperating execution contexts:

| Context | Responsibilities |
|---|---|
| UI/message thread | Editor controls, canvas drawing, undo/redo, result consumption, effect preparation, state synchronization, release draining |
| Compiler thread | Debounced canvas snapshots, routing and parameter compilation |
| Audio thread | Host `processBlock`, effect processing, configuration crossfade, gain, metering, and output limiting |

The compiler never runs on the audio thread. Effect instances and their buffers
are prepared before publication. The audio path uses atomic configuration
pointers and does not lock a mutex or intentionally allocate.

## 2. Canvas Compilation

The canvas is a 256x256 grid containing 65,536 cells. A UI edit submits a full
snapshot to `CanvasMessageQueue`; the compiler thread consumes snapshots after a
300 ms pen debounce and calls `compileCanvas()`.

Compilation:

1. Determines active non-Bypass pedal slots.
2. Selects automatic routing from horizontal pixel scores unless valid manual routing is active.
3. Divides the canvas rows among active pedals.
4. Divides each pedal row band among four knob positions.
5. Accumulates color-weighted values into normalized parameter descriptors.
6. Preserves manually overridden knob values.
7. Produces a `PedalAssetPayload` for UI-thread preparation.

Automatic routing uses stable ordering for equal scores. Manual routing supports
one incoming and one outgoing connection per pedal and removes conflicting
connections. Invalid or empty manual routing falls back to automatic routing.
The current cable renderer uses simple Bezier paths; lane allocation and
gap-aware routing remain future work.

### Color and Empty Cells

The UI uses `PixelColor::Transparent(5)` as the empty-cell sentinel. The grid
cache serializes transparent cells as zero and drawn Black as five, so an
initial all-zero grid remains visually empty while drawn Black still contributes
to compilation. Transparent cells are not rendered in the colored overlay.

The twelve drawable colors are Black, White, Red, Green, Blue, Yellow, Brown,
Purple, Grey, Pink, Orange, and Violet. Color weights and paired-color bias are
defined by `CanvasAnalysis.cpp`.

## 3. Configuration Lifecycle

The compiler produces a payload on the compiler thread. The UI timer consumes
it, creates and prepares effect instances, and publishes the finished payload:

```text
compileCanvas()                         compiler thread
    |
    v
consumeCompiledResultIfAvailable()     UI thread
    |
    v
prebuildEffects(payload)                UI thread
    |
    v
m_nextConfig.store(payload, release)   UI thread
    |
    v
audio thread acquires and crossfades
    |
    v
retired payload -> ReleaseQueue
    |
    v
UI thread drains and deletes
```

Once published, a payload is treated as immutable by the audio path. The
re-preparation path can mutate effect preparation state during the host lifecycle
operation that invokes it; that operation must not overlap active processing.

The current source contains 27 enum/definition positions: Bypass, 24 active
effects, and two reserved legacy positions. The factory creates only the active
effects. Legacy IDs 22 and 26 are migrated to Bypass by state loading.

## 4. Audio Processing

`PluginProcessor::processBlock` establishes the host entry and denormal guard.
`UnifiedPedalProcessor` then:

1. Reads the current configuration.
2. Builds per-slot parameter values from descriptors or the atomic parameter cache.
3. Applies automation where linking is enabled.
4. Smooths non-mix parameters using the prepared block-time coefficient.
5. Applies optional per-slot Drift and Unstable modulation to non-mix parameters.
6. Processes the active chain through `DspEffect::processBlock`.
7. Applies per-pedal wet/dry mixing and gain.
8. Applies the unity soft clipper and updates meters.

The external mix knob is excluded from parameter smoothing and uses a per-sample
linear interpolation across each block. Effects with no external mix remain
fully wet within the effect chain.

## 5. Configuration Crossfade

Replacing an active configuration uses a 20 ms equal-power crossfade. The audio
thread processes the old and new chains separately, captures their outputs, and
mixes them with cosine/sine gains. Initial publication may use a direct path.

During an active fade both chains process the input. Stateful smoothing and drift
updates therefore advance in both passes for the same physical slot; this is a
known implementation tradeoff. A newer deferred configuration restarts the fade
toward the newest payload. Pointer-identical configurations are guarded against
self-release.

The fade does not reset the new effect after it becomes current. Retired payloads
are transferred to deferred deletion rather than destroyed on the audio thread.

## 6. Parameter and Automation Pipeline

Parameter order is:

```text
compiled descriptor
    -> parameter-cache override
    -> linked automation blend
    -> non-mix smoothing
    -> Drift/Unstable modulation
    -> effect processBlock
```

Automation is compiled from 64 horizontal canvas slices and uses the DAW PPQ
position when available. The display shows an eight-bar envelope and highlights
the selected active section. Bar counts are 1, 2, 4, or 8. The current UI uses
full-strength knob links; there is no user-adjustable link-strength control.
Moving a manually linked knob creates an override and removes that link.

The 40 Hz smoothing coefficient is prepared from the maximum block size rather
than recomputed from each actual host block, so its exact response varies with
host block sizing. Automation smoothing is a separate approximately 12 Hz path.

## 7. Real-Time Safety

Effect construction, buffer allocation, FFT construction, and configuration
preparation occur before publication. The normal audio path contains no deliberate
`new`, `delete`, vector growth, string construction, or mutex lock.

`ReleaseQueue` has eight single-pointer slots plus an atomic overflow slot. The
audio thread enqueues retired payloads; the UI thread drains and deletes them.
If the overflow slot is overwritten while already occupied, the displaced
pointer is currently leaked. This is a known limitation, not a guarantee of
lossless deferred deletion.

`CanvasMessageQueue` is an SPSC ring with an eight-entry backing array and one
reserved empty slot, giving seven usable entries. When full, it drops the newest
snapshot. The compiler result slot separately keeps the newest available result.

## 8. Numerical and Signal Guardrails

- `ScopedNoDenormals` is used at plugin, chain, and effect block entry points.
- Most delay writes sanitize non-finite input.
- `interpolateDelayRead` sanitizes non-finite results.
- Comb and reverb feedback are bounded by `tanh` and configured gains.
- Dynamic and spectral TDF-II filters reset state after non-finite output.
- The chain converts non-finite final samples to zero.
- Delay lengths, slice lengths, cycle lengths, and feedback parameters are bounded.

These are layered protections, not a guarantee that every internal state recovers
from every non-finite input. Multi-Mode Filter still lacks explicit state reset
for NaN values, and cutoff should be capped below Nyquist for low-rate hosts.

## 9. Tail and Silent-Block Behavior

`getTailLengthSeconds()` reports the maximum tail declared by the active effects,
with a five-second fallback when no configuration is available. Re-Time declares
an eight-second maximum. Several stateful effects do not yet override
`hasActiveTail()`, so exact mix-zero processing can skip their internal state and
host tail reporting can understate their tails.

`silenceInProducesSilenceOut()` is false for the Resampler and true for the other
current processor configurations. Hosts may therefore apply their own silence
optimization to effects whose internal tails are not fully declared.

## 10. Canvas Queue and Routing Semantics

The SPSC queue uses release/acquire publication for complete snapshots. A full
queue drops the newest message rather than overwriting the oldest. This is safe
for memory ownership but can delay the visual state represented by the compiler.

Manual parameter overrides are keyed by physical chain position and parameter
token. Rerouting can therefore cause an override to apply to a different effect
at the same position; effect identity is not part of the current override key.

## 11. Design Trade-offs

| Decision | Current rationale or limitation |
|---|---|
| Config-owned effects | Avoids effect-array races during publication and crossfade |
| Block processing | Reduces virtual dispatch while keeping effect-specific inner loops |
| ADAA instead of oversampling | Reduces nonlinear aliasing without oversampling latency; residual aliasing remains at high drive |
| External mix ramp | Avoids zippering without adding mix state to every effect |
| Equal-power crossfade | Maintains approximately constant power between unrelated chains |
| Uniform four-knob pedals | Preserves the visual language; unused labels/parameters are intentional in many effects |
| Dual-grain engine | Reduces single-grain boundary amplitude modulation; spray makes unity overlap approximate |
| Synthetic convolution IR | Avoids external IR assets; current FFT path is limited to 1024 IR samples |
| Soft clipper | Provides unity behavior below the knee but is not a lookahead limiter |
| Bounded queues | Avoids blocking; saturation can drop snapshots or expose deferred-release ownership limits |

## Related Documents

- [`effects.md`](./effects.md) - active effect algorithms and parameters
- [`ui-controls.md`](./ui-controls.md) - editor and interaction behavior
- [`state-format.md`](./state-format.md) - preset serialization
- [`build.md`](./build.md) - build and deployment
- [`resources.md`](./resources.md) - embedded assets and sprite layouts
- [`audits/`](./audits/) - dated analysis reports
