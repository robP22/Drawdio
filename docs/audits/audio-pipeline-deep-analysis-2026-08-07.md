# Drawdio Audio Pipeline Deep Analysis

> Status: Historical analysis from 2026-08-07. Superseded by the newer
> `audio-pipeline-audit-2026-08-22.md` and the current guides in `../`.

Date: 2026-08-07

Scope reviewed:
- Host entry and bypass/silence behavior: `Source/PluginProcessor.*`
- Audio-thread execution: `Source/UnifiedPedalProcessor.*`
- Config publication, deferred release, compiler handoff: `Source/State/ConfigManager.*`, `Source/State/ReleaseQueue.*`, `Source/Compile/*`
- Effect implementations: `Source/Effects/*`, `Source/Dsp/*`
- Parameter definitions and canvas compilation: `Source/State/PedalDefinition.cpp`, `Source/Compile/CompilerEngine.cpp`, `Source/Core/CanvasAnalysis.cpp`

Verification run:

```sh
cmake --build build --config Release --target Drawdio_VST3
```

Result: build completed successfully.

## Executive Summary

The current pipeline has a strong real-time architecture in broad strokes: the audio thread receives immutable config payloads through atomic pointers, effect instances are prebuilt away from `processBlock`, old configs are deferred to a UI-thread release queue, and most effects now run through block-level dispatch.

The highest-risk issues are concentrated around handoff edges and skip logic:

1. The compiler result slot has an unsynchronized `std::unique_ptr` producer/consumer race.
2. The host `processBlock` silence gate can skip blocks that contain real audio after the first four samples.
3. Buffered effects that do not report `hasActiveTail()` can be truncated by that same silence gate.
4. The new config is reset immediately after it becomes current at crossfade completion.
5. The granular dual-grain implementation does not currently maintain the documented 50% phase offset.

These are worth fixing before further effect tuning, because they can cause dropped audio, lost config ownership, audible discontinuities, or inaccurate validation of later DSP changes.

## Pipeline Trace

Runtime flow:

1. `DrawdioProcessor::processBlock()` reads playhead state, computes an input peak, optionally early-outs, wraps JUCE buffer channel pointers, then calls `UnifiedPedalProcessor::processAudioBlock()`.
2. `UnifiedPedalProcessor::processAudioBlock()` loads `currentConfig` and `nextConfig`, applies input gain, crossfades current/next configs if needed, runs the active chain, applies output gain/soft clipping, and retires old configs through `ReleaseQueue`.
3. `processChainBlock()` walks `config.activeRoutingChain`, maps chain position to physical slot through `routingSlotOrder`, resolves params from compiled descriptors or `ParameterCache`, applies linked automation, smoothing, drift modulation, wet/dry mix handling, per-pedal gain, soft clipping, and peak metering.
4. Canvas edits are pushed to `CanvasMessageQueue`, compiled on `CompilerThread`, consumed on the UI timer by `ConfigManager::consumeCompiledResultIfAvailable()`, effect instances are created/prepared in `prebuildEffects()`, and the resulting `PedalAssetPayload*` is published as either current or next config.

## Findings

### AP-01 - P0 - Compiler result slot has a data race and ownership race

Evidence:
- Producer writes `m_slot = std::make_unique<PedalAssetPayload>(...)` and then sets `m_slotFull` in `Source/Compile/CompilerThread.cpp:101`.
- Consumer does a flag CAS and returns `m_slot.release()` in `Source/Compile/CompilerThread.cpp:61`.
- There is no mutex or atomic pointer protecting `m_slot` itself.

Impact:
- If the compiler thread produces a second result while the UI thread consumes the previous result, both threads can access the same `std::unique_ptr` concurrently.
- Possible outcomes include dropped configs, releasing the wrong payload, deleting a payload while it is being consumed, or undefined behavior.

Recommended fix:
- Replace the `std::unique_ptr + atomic bool` pair with an atomic raw pointer exchange, or guard both producer assignment and consumer release with the same mutex.
- If "latest canvas wins" is desired, use `exchange(newPayload)` and delete the replaced payload on the compiler/UI thread, never on the audio thread.

### AP-02 - P0 - Silence gate can drop valid audio blocks

Evidence:
- `fastPeak()` scans only the first four samples of each channel before returning zero in `Source/PluginProcessor.cpp:70`.
- `processBlock()` returns early when that peak is below threshold, effects are considered silent, and no pending config exists in `Source/PluginProcessor.cpp:86`.

Impact:
- A block whose first four samples are near zero but later samples contain guitar audio is skipped entirely.
- This can cut attacks, chopped transients, or any block that begins with a few zero samples.
- Output metering also uses the same `fastPeak()` shortcut, so meters can show zero for nonzero blocks.

Recommended fix:
- Remove this early-out, or scan the whole block before skipping.
- If CPU-saving silence detection is needed, base it on a full-block peak/RMS plus reliable per-effect tail state.

### AP-03 - P0 - Buffered effects can be truncated because tail reporting is incomplete

Evidence:
- `allEffectsSilent()` only checks `effect->hasActiveTail()` in `Source/PluginProcessor.cpp:141`.
- `MicroPitchChorusEffect` owns a 0.5-second delay buffer but does not override `hasActiveTail()` in `Source/Effects/DelayEffects.h:6`.
- `GranularBaseEffect` owns granular delay buffers but does not override `hasActiveTail()` in `Source/Effects/GranularBaseEffect.h:5`.
- `ReverseBufferEffect` owns a 1.5-second buffer but does not override `hasActiveTail()` in `Source/Effects/ReverseBufferEffect.h:6`.
- `GlitchStutterEffect` owns a 1-second buffer but does not override `hasActiveTail()` in `Source/Effects/PitchEffects.h:28`.
- `silenceInProducesSilenceOut()` has special cases in `Source/PluginProcessor.cpp:132`, but this information is not used by the early-out.

Impact:
- Buffered wet output can be cut off as soon as the input block looks silent.
- This affects chorus tails, reverse playback, glitch repeats, granular output, and any future stateful effect that relies on default `hasActiveTail() == false`.

Recommended fix:
- Either remove the host-level early-out or require every stateful/buffered effect to implement a conservative tail predicate.
- Consider a separate capability such as `requiresProcessingOnSilence()` for effects that may output from internal state even when the last block peak was low.

### AP-04 - P1 - Crossfade completion resets the new config after it has accumulated state

Evidence:
- During an active crossfade, the new config is processed in `Source/UnifiedPedalProcessor.cpp:244`.
- When the fade completes, `currentConfig` is exchanged to `next`, then `reset(*next)` is called in `Source/UnifiedPedalProcessor.cpp:293` and `Source/UnifiedPedalProcessor.cpp:298`.

Impact:
- The new effects build state during the 20 ms crossfade, then that state is discarded immediately after publication.
- Delay, reverb, granular, convolution, freeze, reverse, glitch, and modulation states can jump or lose continuity exactly at the transition boundary.

Recommended fix:
- Do not reset `next` after it becomes current.
- If a new config must start from silence, reset it before the first crossfade block, not after it has been rendered into the transition.

### AP-05 - P1 - Granular dual-grain overlap is not actually 50% offset

Evidence:
- `prepareGranularProcessor()` initializes both `readPtr` and `grainPhase2` to zero in `Source/Dsp/GranularProcessor.h:34`.
- On restart, `relPhase2` is derived from the previous `grain2Pos - grainBase`, then assigned through `fmod()` in `Source/Dsp/GranularProcessor.h:68` and `Source/Dsp/GranularProcessor.h:87`.
- The output sums `window[readPtr]` and `window[grainPhase2]` in `Source/Dsp/GranularProcessor.h:122`.
- The docs state "Two grains at 50% overlap" in `docs/effects.md:105`, but the current code keeps both phases aligned from initialization.

Impact:
- The intended Hann window unity-sum behavior is not guaranteed.
- The granular clicking/tapping issue may still be present in this checkout, especially at certain pitch ratios.
- The random spray is also one-sided: `(rng / 16383.0f - 1.0f) * 0.10f` yields `[-0.1, 0]`, not `[-0.1, +0.1]`, in `Source/Dsp/GranularProcessor.h:79`.

Recommended fix:
- Initialize and restart the second grain at `0.5 * grainLen`, with any jitter centered around that offset.
- Make spray bipolar if the intended design is symmetric jitter.

### AP-06 - P1 - ReleaseQueue leaks configs if the audio-thread handoff slots fill

Evidence:
- `ReleaseQueue::pushSingle()` stores into one of eight atomic slots in `Source/State/ReleaseQueue.h:13`.
- If all slots are occupied, it increments `m_droppedCount` and drops ownership in `Source/State/ReleaseQueue.h:21`.
- `drain()` only deletes pointers still present in slots/ring storage in `Source/State/ReleaseQueue.cpp:18`.

Impact:
- A stalled UI thread or rapid config churn can leak entire `PedalAssetPayload` objects plus their effect buffers.
- This avoids audio-thread deletion, which is correct, but losing ownership silently is still a resource leak.

Recommended fix:
- Use a bounded lock-free queue large enough for worst-case UI stalls and expose/drop only after transferring ownership to a non-audio release list.
- At minimum, surface `droppedCount()` in diagnostics and consider retaining a last-resort pending pointer rather than leaking.

### AP-07 - P2 - Automation smoothing rate depends on active chain length

Evidence:
- `m_smoothedAutoValue` is updated inside the per-effect loop in `Source/UnifiedPedalProcessor.cpp:111`.
- The smoothing alpha is calculated from `maxSamplesPerBlock` in `Source/UnifiedPedalProcessor.cpp:27`, not the actual `numSamples` passed to the current block.

Impact:
- A six-pedal chain smooths automation up to six times per block; a one-pedal chain smooths once.
- Hosts with variable block sizes will get inconsistent automation response.

Recommended fix:
- Smooth automation once per audio block before iterating effects.
- Derive the alpha from actual `numSamples / sampleRate` when block sizes vary.

### AP-08 - P2 - Convolution can do heavy FFT/IR work on the audio thread

Evidence:
- `ConvolutionSpaceEffect::processBlock()` calls `recomputeIrFreq()` when damp changes by more than 0.001 in `Source/Effects/ConvolutionSpaceEffect.cpp:229`.
- `recomputeIrFreq()` performs a forward FFT in `Source/Effects/ConvolutionSpaceEffect.cpp:100`.
- If `n + irLen > kFftSize`, processing falls back to brute force convolution in `Source/Effects/ConvolutionSpaceEffect.cpp:231` and `Source/Effects/ConvolutionSpaceEffect.cpp:237`.

Impact:
- Canvas recompiles, automation, or drift can move Damp often enough to trigger FFT recomputation on the audio thread.
- Larger host block sizes can switch from FFT convolution to per-sample brute force work, producing CPU spikes.

Recommended fix:
- Quantize or precompute damp variants off the audio thread, or update IR frequency data through the same immutable-payload publication model.
- Keep partitioned convolution on the FFT path for all supported host block sizes.

### AP-09 - P2 - Reverb timing units are ambiguous and partly sample-rate dependent

Evidence:
- `ReverbNetworkConfig` names delay arrays `combTimesMs` and `apTimesMs` in `Source/Dsp/ReverbNetwork.h:13`.
- Config values such as `1557` and `225` are assigned in `Source/State/EffectConfigRegistry.cpp:10`.
- `prepareReverbNetwork()` converts them as `value * sampleRate / 44100`, which treats them like samples-at-44.1k rather than milliseconds in `Source/Dsp/ReverbNetwork.cpp:14`.
- Early-reflection tap offsets are fixed samples in `Source/Dsp/ReverbNetwork.cpp:70`.

Impact:
- If these values are intended as milliseconds, the reverb is far shorter than named.
- If they are intended as 44.1 kHz sample counts, the field names and docs are misleading.
- Early-reflection times change with sample rate because tap offsets are not scaled.

Recommended fix:
- Rename fields to sample-count terminology or convert actual milliseconds with `sampleRate / 1000`.
- Scale fixed early-reflection tap offsets and LFO increments by sample rate.

### AP-10 - P2 - UI/config sync can show a deferred config before audio receives it

Evidence:
- `consumeCompiledResultIfAvailable()` copies `payloadPtr` into `m_lastConfigSync`, calls `loadPedalConfiguration()`, increments revision, and triggers UI notification in `Source/State/ConfigManager.cpp:214`.
- `loadPedalConfiguration()` can defer publication when `m_nextConfig` is already occupied in `Source/State/ConfigManager.cpp:183`.

Impact:
- The UI can update knob/routing display for a payload that has not yet become `nextConfig` or `currentConfig`.
- During rapid canvas edits, visual state may lead audio state by one or more config transitions.

Recommended fix:
- Only advance `m_lastConfigSync`/revision after the payload has been accepted for publication, or track a separate "deferred UI preview" revision explicitly.

### AP-11 - P2 - Many labeled knobs are currently unwired in DSP

This is not a real-time safety issue, but it directly affects whether canvas-derived parameters do what the UI says.

Examples from current `processBlock()` mappings:

| Effect | Wired params | Unwired labeled params |
| --- | --- | --- |
| Waveshaper | Drive | Tone, Sym, Level |
| Wavefolder | Fold | Sym, Drive, Level |
| Multi Filter | Mode, Cutoff | Res, Level |
| Pitch Shifter | Rate | Spread, Grain, Level |
| Glitch Stutter | Intens | Gate, Rate, Level |
| Diffused Reverb | Decay | Diff, Size |
| Tape Stop Echo | Brake | Speed, Decay |
| Plate Reverb | Decay | Size, Damp |
| Rhythm Gate | Rate, Shape, Depth | — (all 4 wired; Mix handled by processor blend) |
| Comb Resonator | Freq | Feed, Decay, Level |
| Spectral Freeze | Freeze | Drift, Window |
| Frequency Shift | Shift | Spread, Depth, Level |
| Reverse Buffer | Density | Length, Dir |
| Grain Scrubber | Pos, Level-as-rate | Density, Size |
| Convolution Space | Damp | Space, Size |
| Random Modulator | Depth, Smooth, Rate | Shape |

Impact:
- Canvas regions can appear to control parameters that are ignored by the effect.
- Manual knob moves can be preserved and displayed but have no audible effect.

Recommended fix:
- Either implement the missing mappings or relabel disabled/unwired knobs so the UI and DSP contract match.

## Positive Findings

- `processBlock()` no longer consumes compiled results directly; config loading is in the UI timer path.
- Effect creation and buffer allocation are done in `ConfigManager::prebuildEffects()` and effect `prepare()` before payload publication.
- The audio thread performs pointer exchange and deferred release rather than constructing/destroying effect objects.
- Delay-oriented effects have per-channel state in the current checkout: Simple Delay, Tape Stop Echo, MicroPitch Chorus, GranularBase, Flanger, ReverseBuffer, GlitchStutter, and Convolution all maintain channel-specific buffers or state.
- Block-level `DspEffect::processBlock()` overrides remove most per-sample virtual dispatch.
- `juce::ScopedNoDenormals` is present at the plugin entry, chain processor, and effect block entry points.
- Per-pedal and output soft clipping guard against non-finite or runaway output.

## Suggested Fix Order

1. Fix `CompilerThread` result ownership (`AP-01`).
2. Remove or replace the host silence early-out (`AP-02`, `AP-03`).
3. Remove `reset(*next)` after crossfade completion (`AP-04`).
4. Correct granular dual-grain phase and spray behavior (`AP-05`).
5. Make deferred release non-leaking (`AP-06`).
6. Move convolution IR/FFT recompute off the audio thread or bound its cost (`AP-08`).
7. Normalize automation smoothing to once per block (`AP-07`).
8. Decide whether unwired knobs should be implemented or relabeled (`AP-11`).

