# Drawdio Architecture

This document describes the current thread model, canvas compilation pipeline,
configuration lifecycle, parameter flow, audio processing, and known runtime
limitations. The active effect reference is [`effects.md`](./effects.md).

## 1. Thread Model

Drawdio uses three cooperating execution contexts:

| Context | Responsibilities |
|---|---|
| UI/message thread | Editor controls, canvas drawing, undo/redo, result consumption, effect preparation, state synchronization, release draining, session state |
| Compiler thread | Revisioned canvas snapshots, incremental graph analysis, routing and parameter compilation |
| Audio thread | Host `processBlock`, effect processing, configuration crossfade, gain, metering, and output limiting |

The compiler never runs on the audio thread. Effect instances and their buffers
are prepared before publication. The audio path uses atomic configuration
pointers and does not lock a mutex or intentionally allocate. Host lifecycle
calls (`prepareToPlay`, `releaseResources`, destruction) use the JUCE callback
lock (`AudioProcessor::getCallbackLock()`). `CompilerThread::stop()` is
serialized by `m_stopMutex` to avoid a double-join when the host races
`releaseResources()` against instance destruction. `DrawdioProcessor` guards
`processBlock` with a heap-allocated shutdown flag, a processing-enabled flag,
and an in-flight counter so destruction can wait briefly for the last audio
callback to exit.

## 2. Canvas Compilation

The canvas is a 256x256 grid containing 65,536 cells. Each completed UI stroke
submits a full grid snapshot, a dirty-row mask, and a monotonically increasing
`m_canvasRevision` to `CanvasMessageQueue`. The message also carries the current
pedal slots, manual routing, and existing parameter descriptors as a fixed-state
snapshot — the compiler no longer reads mutable shared vectors.

`CanvasGraphAnalyzer` caches per-row summaries (`paintedPrefix`, `xSumPrefix`,
`weightPrefix`, `colourPrefix`) and rebuilds only dirty rows. The compiler
thread waits (50 ms poll) until `PenDebouncer::isIdle()` and a message is
available. Compilation steps:

1. Update cached graph analysis for dirty rows only.
2. Determine active non-Bypass pedal slots.
3. Select automatic routing from horizontal pixel scores unless valid manual routing is active.
4. Divide the canvas rows among active pedals.
5. Divide each pedal row band among four knob positions.
6. Accumulate color-weighted values into normalized parameter descriptors.
7. Preserve manually overridden knob values via `existingParams`.
8. Produce a revisioned `PedalAssetPayload` for UI-thread preparation.

Automatic routing uses stable ordering for equal scores. Manual routing supports
one incoming and one outgoing connection per pedal and removes conflicting
connections. Invalid or empty manual routing falls back to automatic routing.
The cable renderer uses cached Bezier paths with gap-aware lane placement for
the current pedal layout.

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
m_slot.exchange(new payload)            compiler thread  (old payload deleted)
    |  -> m_resultAvailableCallback -> sendChangeMessage
    v
consumeCompiledResultIfAvailable()      UI thread (polls m_slot)
    |
    v
prebuildEffects(payload)                UI thread (create + prepare)
    |
    v
m_nextConfig.store(payload, release)   UI thread  (or deferred if occupied)
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

Once published, a payload is treated as immutable by the audio path. Preparation
tracks sample rate, block size, and channel count; re-preparation re-prepares
all live payloads. If `CanvasMessageQueue::pushSnapshot` returns `false` (full),
the UI retains the newest grid state and sets `m_compileRetryPending` to retry
on the next UI poll after the consumer frees capacity. Obsolete revisions are
discarded before and after compilation (`sourceRevision != latestRevision`),
and `ConfigManager::consumeCompiledResultIfAvailable` rejects results older
than `m_canvasRevision`.

When the new compilation has the same topology (`activeRoutingChain` and
`routingSlotOrder` identical) and mode as the current payload and no crossfade
is active, the manager takes a parameter-only fast path: it updates the
compiled parameter bank and the last-config sync without allocating a new
payload. Otherwise a full payload is built and crossfaded. If `m_nextConfig`
is already occupied, the new payload is deferred (`m_deferredConfig`) and
applied once the audio thread clears `m_nextConfig`.

The current source contains 27 enum/definition positions: BYPASS plus 25
active effects plus one reserved slot (ID 26, formerly Analog Octaver, now
migrated to BYPASS on load). Slot 22 was Random Modulator in older projects and
now holds the active HP/LP Filter. The factory creates only active effects.

## 4. Audio Processing

`PluginProcessor::processBlock` establishes the host entry and denormal guard,
updates transport (BPM/PPQ/playing), samples input/output peaks via a fast
path, and delegates to `UnifiedPedalProcessor::processAudioBlock`.

`UnifiedPedalProcessor` then:

1. Handles chunked blocks if `numSamples > maxSamplesPerBlock`.
2. Reads the current and next configurations atomically.
3. Builds per-slot parameter values from descriptors, the atomic parameter cache, or the compiled parameter bank.
4. Applies automation where linking is enabled.
5. Smooths non-mix parameters using the prepared block-time coefficient.
6. Applies optional per-slot Drift and Unstable modulation to non-mix parameters.
7. Processes the active chain through `DspEffect::processBlock`.
8. Applies per-pedal wet/dry mixing (linear ramp per block) and gain.
9. Applies the unity soft clipper and updates per-pedal meters; re-applies a final soft clip after output gain.

The external mix knob is excluded from parameter smoothing and uses a per-sample
linear interpolation across each block. Effects with no external mix remain
fully wet within the effect chain. Snap detents are applied after smoothing
and modulation (mix knob excluded).

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
are transferred to deferred deletion via `ReleaseQueue::pushSingle` rather than
destroyed on the audio thread. Crossfade state tracks the pending reset request
separately from the publishing path.

## 6. Parameter and Automation Pipeline

Parameter order is:

```text
compiled descriptor
    -> parameter-cache override (or compiled bank when useCompiledParameterBank)
    -> linked automation blend (range-mapped, mix knob excluded)
    -> non-mix smoothing (per-block 40 Hz, adapted to actual block size)
    -> Drift/Unstable modulation (per-slot dual-rate random walk, mix excluded)
    -> snap detents (mix excluded)
    -> effect processBlock
```

Automation is compiled from 64 horizontal canvas slices and uses the DAW PPQ
position when available (fallback 120 BPM / 0 PPQ). The display shows an
eight-bar envelope and highlights the selected active section. Bar counts are
1, 2, 4, or 8. Knob links support an adjustable automation range with a minimum
0.05 width; moving a linked knob still creates an override and removes that link
(and resets its range to 0..1).

The 40 Hz smoothing coefficient is prepared from the maximum block size and
adapted per actual host block for accurate smoothing. Automation smoothing is a
separate approximately 12 Hz path.

## 7. Real-Time Safety

Effect construction, buffer allocation, FFT construction, and configuration
preparation occur before publication. The normal audio path contains no deliberate
`new`, `delete`, vector growth, string construction, or mutex lock.

`ReleaseQueue` has a 16-entry ring (`kCapacity`) plus eight single-pointer slots,
an atomic overflow slot, a pending-delete slot, and a `droppedCount` counter.
The audio thread enqueues retired payloads via `pushSingle` (single slots first,
then overflow); the UI thread drains all slots and the ring. If the overflow
slot is overwritten while already occupied, the displaced pointer is moved to
`pendingDelete`; a second displacement is counted in `droppedCount` (a bounded
leak, not lossless).

`CanvasMessageQueue` is an SPSC ring with `QueueCapacity = 8` and one reserved
empty slot, giving seven usable entries (`CanvasMessageQueue.h:27`,
`CanvasMessageQueue.cpp:37` `nextWrite == readIndex` -> full -> `false`).
Messages carry a revision and dirty-row mask. When full, `pushSnapshot` returns
`false` without modifying an occupied slot; the caller retains state and retries.
The compiler result slot (`m_slot`) is a single atomic pointer that keeps the
newest available result, deleting the previous payload on exchange.

Obsolete revisions are discarded before publication, and the configuration
manager rejects results older than the latest canvas revision.

## 8. Numerical and Signal Guardrails

- `ScopedNoDenormals` is used at plugin, chain, and effect block entry points.
- Most delay writes sanitize non-finite input.
- `interpolateDelayRead` sanitizes non-finite results.
- Comb and reverb feedback are bounded by `tanh` and configured gains.
- Dynamic and spectral TDF-II filters reset state after non-finite output; SpectralFreeze uses a short xfade to smooth offset changes.
- HP/LP and Multi-Mode cutoffs are capped below Nyquist (0.45 * sample rate for Multi-Mode).
- The chain converts non-finite final samples to zero.
- Delay lengths, slice lengths, cycle lengths, and feedback parameters are bounded.

## 9. Tail and Silent-Block Behavior

`getTailLengthSeconds()` reports the maximum tail declared by the active effects,
with a five-second fallback when no configuration is available. Re-Time declares
`kReleaseSeconds = 0.05` (ring holds 16 s of history, release fades over 50 ms);
Simple Delay and plate/diffused reverbs declare 2.5 s; Spectral Freeze 2.0 s;
Convolution Space 1.5 s; Wavefolder 0.8 s. During shutdown `getTailLengthSeconds`
returns 0.

`silenceInProducesSilenceOut()` returns `false` only when the active chain
contains the Bitcrusher; otherwise it returns `true` (during shutdown it returns
`true`). Hosts may therefore apply their own silence optimization to effects
whose internal tails are not fully declared. Several stateful effects still rely
on the host keeping processing alive via `getTailLengthSeconds` rather than
overriding `hasActiveTail()`.

## 10. Canvas Queue and Routing Semantics

The SPSC queue uses release/acquire publication for complete snapshots. A full
queue returns `false` and leaves the occupied slot untouched; the UI retains the
newest grid/configuration state and retries after the consumer frees capacity. The
compiler discards obsolete revisions both before and after `compileCanvas`.

Manual parameter overrides are keyed by physical slot and parameter token
(`ParameterCache` is `PedalSlotCount x KnobsPerPedal` per-slot), while the
compiled descriptor also retains the current chain position for DSP dispatch via
`routingSlotOrder`.

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
| Synthetic convolution IR | Avoids external IR assets; uniform 512-sample partitions, ~0.8 s / 69 partitions at 44.1 kHz |
| Soft clipper | Provides unity behavior below the knee but is not a lookahead limiter |
| Bounded queues | Avoids blocking; a full queue drops the newest snapshot (`false`) and the UI retries; `ReleaseQueue` overflow is counted in `droppedCount` |

## Related Documents

- [`effects.md`](./effects.md) - active effect algorithms and parameters
- [`ui-controls.md`](./ui-controls.md) - editor and interaction behavior
- [`state-format.md`](./state-format.md) - preset serialization
- [`build.md`](./build.md) - build and deployment
- [`resources.md`](./resources.md) - embedded assets and sprite layouts
- [`audits/`](./audits/) - dated analysis reports
