# DSP & Effects Audit — 2026-08-30

Full pass over the DSP core, the 26 effect slots, the parameter pipeline,
serialization, and test coverage. Follows the effect rewrites, the crossfade
overhaul (Smooth knobs), and the concurrent config-lifecycle crash fixes.

## Verified clean

### Real-time safety
- Every allocation in the effect tree is confined to `prepare()` (the
  `resize`/`assign`/`make_unique` hits are all preparation-time); FFT, ring,
  freeze, and delay buffers are fixed-size; no `new`/`delete` or
  `std::shared_ptr` mutation on the audio path.
- Config handoff is an atomic pointer exchange with a deferred-release queue;
  `ConfigManager`'s destructor now stops the compiler thread before tearing
  down the queue/debouncer.
- `juce::ScopedNoDenormals` at every `processBlock` entry plus the processor and
  plugin entry points.
- `interpolateDelayRead` (Catmull-Rom) is wrap-safe and non-finite-guarded; all
  ring/delay/FDN reads were verified to stay behind their write heads
  (SimpleDelay, Chorus 24-36ms window, Flanger 1-10ms, granular write-head
  clamp, FDN taps, modulated diffusers, glitch/reverse/freeze freeze buffers).

### Numerical guardrails
- `isfinite` guards on every input path; non-finite state resets in all filters
  (MultiMode, Formant, SpectralFilter, HpLp); R caps (0.995/0.98), feedback
  caps, gain clamps, and the unity-gain soft-knee output limiter.

### Parameter pipeline
- `processChainBlock`: `mask | manualParams` cache reads, 40 Hz smoothing,
  per-node snap (precomputed `snapSteps`), mix-knob exclusion, drift modulators,
  automation links — consistent end to end. Manual-mode cache seeding (slot
  defaults, `seedCacheFromCurrentConfig`, `resetParamDefaults`) is coherent with
  the override mask semantics.

### FDN reverb (Diffused/Plate)
- Matrix mixing stable (constant-mode eigenvalue −1 × feedback ≤ 0.97,
  per-line damping ≤ 0.16); `sizeScale` smoothed (~23 ms); wet scale
  `(1 − fb·0.92)·0.9`; modulated allpass diffusers never lap the write head.

### Convolution reverb
- Overlap-add structure correct; damp-grid precompute; FFT-path normalization
  verified by the full-level test; the brute-force fallback is unreachable at
  production block sizes (dead path).

## Findings (all addressed this pass)

| # | Finding | Fix |
|---|---------|-----|
| 1 | SimpleDelay delay-time smoothing at 5%/block (τ≈0.46 s) sweeps the read head on live Time moves → audible varispeed pitch bend. | Smoothing rate raised to 0.2/block (τ≈110 ms): still click-free, minimal bend. `DelayEffects.cpp` |
| 2 | MultiModeFilter switches LP→BP→HP instantaneously at the ⅓/⅔ Mode boundaries → click under automation sweeps. | 10%-wide boundary crossfade zones (bandPos 0.95-1.05, 1.95-2.05) blend the two filter outputs; stepped character preserved outside the zones. `FilterEffects.cpp` |
| 3 | Bitcrusher anti-alias biquad recomputed only when cutoff moves >5% → block-rate coefficient steps (zipper) on Filter sweeps. | Per-sample coefficient interpolation (prev/current sets lerped across each block; TDF-II stable under convex interpolation — a2 stays in (0,1)). `BitcrusherEffect.{h,cpp}` |
| 4 | `processSample` fallbacks carry stale/wrong param fixtures (DelayEffect maps effectParam to Time *and* Feedback; Chorus/Tremolo/Flanger/Bitcrusher/GrainScrubber use static fixtures). | Unused by the chain (always `processBlock`); documented here as latent traps for future callers. No change. |

## Test coverage matrix (65 cases)

Previously untested effects now covered (+4):

- **VCA Compressor** — loud-input compression below the input with neutral
  makeup, open threshold passes higher, bounded.
- **Formant Shifter** — finite, bounded at maximum Q over a 2 s run.
- **Spectral Filter** — center-tuned input resonates strongly; off-center
  input rejected at steady state (the first block's center sweep fires a
  natural resonance chirp as the narrow Q crosses the input frequency — a
  resonator property, not a defect).
- **Grain Scrubber** — non-silent finite, Position knob changes the output.

Remaining gaps (accepted): Smooth-knob fade-length behavior has no direct
test; reverb network tail spectra untested beyond non-silence/decay.

## Residual notes

- MultiModeFilter band crossfade uses `bandPos` (pre-clamp) — the blend zones
  are valid across the full knob range.
- VCA `if (gain < 0.0f)` after `exp2` is dead code (harmless).
- The first block of a fresh SpectralFilter/Formant state sweeps coefficients
  from the previous defaults; combined with automation jumps this produces a
  brief resonant chirp — the natural swept-filter behavior.

## Verification

- Full suite: 65 cases / 4,775,319 assertions, Release green.
- `updater.cmd` Release build + VST3 install clean.
