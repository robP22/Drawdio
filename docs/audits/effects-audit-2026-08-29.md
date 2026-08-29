# Effects Audit — 2026-08-29

Static analysis of every effect's algorithm, parameter wiring, edge-case
prevention, and audio-thread safety, plus the fixes applied for the reported
grain-scrubber and delay-pitch anomalies. Ground truth: current `Source/Effects/`,
`Source/Dsp/`, and `UnifiedPedalProcessor` at commit `225178c` + working tree.

## Reported anomalies — root causes

### 1. Grain Scrubber "destroys/crushes" the signal at high Position

Root cause (`GranularProcessor.h`): the grain read window `[base, base+grainLen)`
was selected with `offset = (position + spray) * bufSize` clamped only to
`[0, bufSize)`. When `offset > bufSize - grainLen` (Position knob above
~0.86-0.96 with spray jitter), the window wraps past the write head: the read
lands on not-yet-written buffer positions (previous-cycle audio) and on the
live input, so the grain content becomes a moving fresh/stale blend with
discontinuities — perceived as crushed, destroyed audio. The unsigned-modulo
base arithmetic also had an underflow edge near buffer boundaries.

Fix: `computeGrainBase()` now clamps the total offset to
`[128, bufSize - grainLen * max(1, 1/speed) - 128]`. The speed term is
required because a grain lasts up to `grainLen/speed` samples and the write
head laps the window start after `bufSize - grainLen - offset` samples; at
speed < 1 the read head is slower than the write head and would otherwise be
overtaken mid-grain. Base arithmetic is now signed 64-bit before modulo.

### 2. Small clicks when playing grains (all granular effects)

Root cause (`GranularProcessor.h`): the old dual-grain used one shared cycle
with `grainPhase2` reset to `(0.5 ± spray) * grainLen` at every restart. Both
Hann windows were near zero just before a restart (silent seam), then head 2's
window jumped instantly to 0.1-1.0 at a new random base — an amplitude step of
up to ~0.9 x sample level at every restart (6-25 Hz depending on Rate).

Fix: replaced the shared-cycle design with two independent grain heads whose
restart events are staggered by half a grain length. Each head's Hann window
is zero at its own restart, the windows stay complementary (unity sum for even
grain lengths, negligible ripple for odd), and each head re-rolls its own
base/spray. Verified with a DC soak: maximum output step after warm-up < 1e-3.

### 3. Delay-based effects "pitch on acid"

Three verified contributors:

- **MicroPitch Chorus tap drift** (`DelayEffects.cpp`): free-running read taps
  with constant detune speeds (1 ± cents/1200) against a 0.5 s buffer. Tap 1
  (speed > 1) catches and passes the write head; tap 2 lags until its delay
  exceeds the buffer and snaps back. Both produce periodic glitches and, after
  a lap, sustained garbage — an audible pitch warble every ~17-34 s at default
  detune, faster at high detune. **Replaced**: the effect is now a classic
  Chorus (fixed 30 ms center delay, bounded LFO ±20 ms, per-channel LFO phase
  offset, Rate 0.05-3 Hz). The read position is always 10-50 ms behind the
  write head by construction — no drift, no write-head crossing.
- **Simple Delay load glide**: `m_smoothedDelaySamples` started at 0.1 s, so
  every config load ramped the delay from 0.1 s (~230 ms pitch glide). Fixed:
  the first block snaps to the compiled delay time; prepare() seeds the 0.55 s
  default.
- **Re-Time loop overwrite** (old design): a static loop region was overwritten
  by the advancing write head after `buffer - loopLength` samples, degrading
  the loop into live input. Already resolved by the freeze-engine redesign
  present in the working tree: a 16 s ring plus per-channel freeze buffer,
  re-captured at sync and at loop-end wrap — the played loop is never written
  to between captures.

The DRIFT/UNSTABLE modulators in `UnifiedPedalProcessor` are disabled by
default (`setDriftAmount` is never called). If enabled later, delay-time and
rate parameters must be exempted: a ±2% delay-time modulation is a ±2% pitch
modulation.

## Fixes applied

| Area | Change |
|---|---|
| `Dsp/GranularProcessor.h` | Write-head-safe, speed-aware base clamp; signed 64-bit base math; staggered dual-grain heads (click-free restarts). |
| `Effects/ModulationEffects.{h,cpp}` | New `ChorusEffect` replacing `MicroPitchChorusEffect` (fixed center delay + bounded LFO). |
| `Core/DspModuleType.h` | `MICROPITCH_CHORUS` -> `CHORUS` (integer 2 unchanged — presets migrate). |
| `Effects/DelayEffects.{h,cpp}` | MicroPitch removed; SimpleDelay first-block delay snap + default-seeded smoother. |
| `Effects/ReverseBufferEffect.cpp` | Repeat boundaries re-capture `xfadeFrom` from the last played sample (no hard splice). |
| `Effects/FilterEffects.cpp` | Multi-Mode Filter: cutoff capped at 0.45 x sr (block + sample paths); non-finite state reset. |
| `Effects/SpectralFilterEffect.{h,cpp}` | Per-sample coefficient interpolation (no block-boundary steps); prev-state members. |
| `Effects/DistortionEffects.cpp` | Comb resonator `m_hasTail` peak aggregated across channels. |
| `Dsp/DspEffectFactory.cpp`, `State/PedalDefinition.cpp` | CHORUS wiring ("Chorus", Mix/Depth//Rate). |
| `tests/*` | MicroPitch cases replaced; new granular (DC soak, write-head invariant), Chorus (depth, 60 s drift soak), SimpleDelay (no glide), ReverseBuffer (splice), Multi-Mode (extreme cutoff) tests. |

## Per-effect edge-case status (current source)

| Effect | Algorithm | Edge cases verified |
|---|---|---|
| Wave Shaper | ADAA arctan clipper | Drive bypass, non-finite input, derivative fallback. ADAA reduces aliasing but not fully at extremes. |
| Chorus | Center-delay + LFO tap | Read window 10-50 ms behind write head, no drift, depth clamped below center delay. |
| Multi-Mode Filter | SVF LP/BP/HP + mode morph | Cutoff capped < Nyquist, state clamp ±8, non-finite reset, per-sample fc ramp. |
| Pitch Shifter (granular) | Dual-grain crossfade | Engine-wide: write-head clamp, click-free restarts. |
| VCA Compressor | Peak envelope + soft knee | Envelope floor, attack/release floor, gain >= 0, makeup above unity. |
| Glitch Stutter | Record/play/gate slices | 32-sample cosine splices; repeat wrap to offset 32 is deliberate (splice seam continuity), not a bug. |
| Diffused/Plate Reverb | FDN 8 lines + ER taps | tanh-bounded feedback stable; mono-summed network input is a known design limitation. |
| Wave Folder | Sine-fold ADAA | Fold capped at 4.0; derivative fallback. |
| Formant Shifter | Envelope-driven TDF-II resonator | Bandwidth max(50 Hz, fc/5); per-sample coefficients; non-finite reset. |
| Re-Time | Ring + freeze loop player | Freeze buffer never overwritten between captures; loop length clamped to ring - 2; bar-quantized capture. |
| Simple Delay | Feedback delay + damp | Catmull interpolation, per-block smoothing + in-block ramp, first-block snap, delay clamped below buffer. |
| Rhythm Gate | 3-zone envelope shaper | Shared phase, 1 ms smoother, phase preserved on rate change, gain clamped. |
| Granular Delay | Dual-grain crossfade | See granular engine. |
| Comb Resonator | Fractional Karplus-Strong | Damped tanh feedback, interpolated reads, delay clamped below buffer, tail aggregated. |
| Time Freeze | 1.5 s ring freeze loop | Entry/exit/loop cosine fades; loop splice crossfade; 0.25-2x ratio. |
| Frequency Shifter | Allpass Hilbert + SSB | Stable allpass cascades; quadrature accuracy limited outside the design band (known). |
| Reverse Buffer | Reversed slice repeats | 32-sample entry fade; repeat fade re-captured per repeat. |
| Grain Scrubber | Dual-grain + position | Write-head clamp (speed-aware), click-free restarts, monotonic position mapping. |
| Resonant Filter | TDF-II bandpass | Per-sample coefficient interpolation; pole radius < 0.995; non-finite reset. |
| HP/LP Filter | Biquad HP + LP series | Stable at documented Q range. |
| Convolution Space | Overlap-add FFT | IR clamped [16, 1024]; overlap-add verified; brute-force fallback for large blocks. |
| Resampler | S&H + bitcrush + dither | DC blocker, anti-alias biquad retuned on cutoff change, TPDF dither, finite input. |
| Tremolo | 3-shape LFO gain | Depth-zero bypass, smoothed square, per-channel phase. |
| Flanger | Swept comb + feedback | tanh-bounded feedback, delay clamped below buffer, stereo LFO offset. |

## Known remaining limitations

- Granular position knob has a small top-end dead zone at slow rates (the
  speed-aware clamp) — ~4% of the range at speed 1, ~8% at speed 0.5.
- Diffused/Plate Reverb sum stereo input to mono before the network.
- Frequency Shifter Hilbert accuracy is band-limited by design.
- `processSample` overrides on several effects are vestigial (every effect
  overrides `processBlock`; the base fallback passes `params[0]`) — kept for
  interface compatibility, values may diverge from block paths.
- `interpolateDelayRead` uses Catmull-Rom, which can overshoot on transients;
  safe at the current feedback caps (0.9 max + damping + tanh).
- Drift modulators exist but are never enabled; if enabled, time/rate
  parameters must be exempt.

## References

- Roads, C. (2001). *Microsound*. MIT Press — grain envelopes, crossfade design.
- Zölzer, U. (ed.) (2011). *DAFX: Digital Audio Effects*, 2nd ed. Wiley —
  modulated delay lines, pitch shifting with crossfading read heads.
- Nilsen, E. (2019). "The Theory of Modulated Delay Lines". — read heads must
  stay behind the write head; re-anchoring with crossfades.
- Parker, J. & Hélie, T. (2019). "Perceptually Relevant Aspects of ADAA". DAFx.
