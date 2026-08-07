# Drawdio Effect Algorithm Reference

Per-effect documentation of the DSP algorithm, knob mappings, guardrails, and
optimizations. Companion to [`architecture.md`](./architecture.md).

## Conventions

- **Param mapping** shows which `params[0..3]` the effect's `processBlock`
  reads and how the normalized [0,1] value maps to a coefficient.
- **Mix knob**: `mixKnobIndex() == 0` means `params[0]` drives the wet/dry
  blend in the processor (per-sample linear ramp). Effects with a mix knob do
  *not* use `params[0]` for their own algorithm.
- **Guardrails** are the NaN/bounds/stability protections specific to the effect.
- **Optimizations** are the block-level dispatch, coefficient hoisting, and
  other performance measures applied.

---

## Distortion

### Waveshaper (`WAVESHAPER_DISTORTION`, `WaveshaperEffect`)

- **Algorithm**: ADAA (antiderivative) arctan soft-clipper.
  `F(x) = x·atan(αx) − ln(1 + α²x²)/(2α)`, output `k·(F(x₂) − F(x₁))/(x₂ − x₁)`
  where α = drive·5.0. ADAA integration suppresses the aliasing a raw
  per-sample nonlinearity would produce. The `dx > 1e-10` guard substitutes the
  analytic derivative `k·atan(α·(x₁+x₂)/2)` at near-DC.
- **Knobs**: `params[2]` (Drive) → α = drive·5.0, clip = 1 − drive·0.5.
  Tone(0), Sym(1), Level(3) are unwired.
- **Guardrails**: input `x₂` sanitized with `std::isfinite` before ADAA math;
  `drive < 0.01` early-returns (bypass); no division by zero (dx guard + α ≥ 0.05).
- **Optimizations**: block-level `processBlock` with hoisted α/k/clip; no
  per-sample virtual dispatch.

### Wavefolder (`MATHEMATICAL_WAVEFOLDER`, `WavefolderEffect`)

- **Algorithm**: ADAA sine-fold. Fold curve `sin(x·a)` with fold amount
  `a = π·(1 + params[1]·4)` (capped at 5× from the original 9× to reduce
  aliasing). ADAA antiderivative `−cos(x·a)/(a·norm)`.
- **Knobs**: `params[1]` (Fold) → d = 1 + fold·4, a = d·π, norm = 1 + d·0.1.
  Sym(0), Drive(2), Level(3) are unwired.
- **Guardrails**: input sanitized `std::isfinite`; dx guard; `a·norm` never zero.
- **Optimizations**: block-level loop, hoisted d/norm/a.

### Comb Resonator (`COMB_RESONATOR`, `CombResonatorEffect`)

- **Algorithm**: Karplus-Strong-style comb with saturating feedback.
  Delay line of `sr/freq` samples; write `in + tanh(damp·feedback)`; output the
  interpolated fractional delayed tap. Feedback fixed at 0.85, damped by a 3 kHz
  one-pole lowpass.
- **Knobs**: `params[0]` (Freq) → `freq = 20·pow(66.666, val)` (20 Hz–1.33 kHz).
  Feed(1), Decay(2), Level(3) are unwired (feedback/damp hardcoded).
- **Guardrails**: input sanitized; **delayed read sanitized** (prevents NaN
  feedback loop); delay clamped to `bufSize−1`; `bufSize == 0` guard.
- **Optimizations**: block-level loop; fractional reads via
  `interpolateDelayRead` (fixes zipper noise on frequency sweeps — was
  integer-indexed).

---

## Delay

### Simple Delay (`SIMPLE_DELAY`, `SimpleDelayEffect`)

- **Algorithm**: Interpolated delay line (linear interpolation) with damped
  feedback. Write `in + tanh(fbLp·feedback)`; output interpolated delayed tap.
  Delay time one-pole-smoothed once per block (`+= 0.05·(target − current)`)
  to avoid pitch jumps when the Time knob moves. No delay-time modulation
  (unconditional LFO removed — was a ±1.5% read-position wobble at ~2.8 Hz).
- **Knobs**: `params[1]` (Time) → 0.1–1.0 s; `params[2]` (Feed) → 0.3–0.9;
  `params[3]` (Damp) → one-pole LP cutoff 500 Hz–15.5 kHz. Mix(0) for blend.
- **Guardrails**: input sanitized; `delaySamples` clamped to `bufSize−1`
  (fixed zero-delay wrap bug); `bufSize == 0` guard; feedback ≤ 0.9 + tanh clamp.
- **Optimizations**: block-level; smoothing runs once per block (was per-channel,
  causing L/R delay mismatch in stereo); dampCoeff hoisted per block.

### MicroPitch Chorus (`MICROPITCH_CHORUS`, `MicroPitchChorusEffect`)

- **Algorithm**: Dual-tap detuned chorus. Two reads from a delay line at
  `±detune` cents with per-channel LFO-modulated positions.
  Detune: `effectParam·50` cents. Modulation depth corrected (a stray `·0.001`
  scalar was cancelling the LFO depth — chorus modulation is now audible).
- **Knobs**: `params[1]` (Depth), `params[2]` (Detune → effectParam),
  `params[3]` (Rate) → LFO 0.05–3.0 Hz. Mix(0) for blend.
- **Guardrails**: input sanitized; wrap guards on read positions; LFO phase
  single-subtract wrap.
- **Optimizations**: block-level wrapper forwards to per-sample core; per-sample
  denormal guard removed (block fence covers the loop).

### Tape Stop Echo (`TAPE_STOP_REVERSE_ECHO`, `TapeStopEchoEffect`)

- **Algorithm**: Variable-speed tape loop. Writes input (tanh·0.77) at a fixed
  write pointer; reads at an independent head whose speed ramps down during
  braking (`readSpeed·0.98−0.05·braking`, clamped ≥ 0.001). Brake onset/release
  crossfades over 32 samples to hide the speed discontinuity. No permanent
  wow/flutter modulation (unconditional ±1.8-sample sine wobble removed).
- **Knobs**: `params[1]` (Brake) → braking amount (0–1). Speed(2), Decay(3) are
  unwired. Mix(0) for blend.
- **Guardrails**: input sanitized; readSpeed lower-bound clamped; 32-sample
  brake crossfade; wrap guards.
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

### Granular Delay (`GRANULAR_DELAY`, `GranularDelayEffect`)

- **Algorithm**: Dual-grain granular delay. Two grains at 50% overlap with
  independent Hann windows (windows sum to unity — zero amplitude modulation).
  Grain length 0.15 s, buffer 2.0 s. Random start spray (up to 10% one-sided)
  jitters grain placement; grain2 phase gets a bounded jitter of up to
  0.1·grainLen (one-sided, spray ∈ [−1, 0]) around the 50% overlap point.
- **Knobs**: `params[3]` (Rate) → `playbackSpeed = exp2(rate·2 − 1)` (0.5–2.0×).
  Spread(1), Size(2) are unwired. Mix(0) for blend.
- **Guardrails**: input sanitized inside `processGranularSample`; grain-length
  ≥ 1 clamp; buffer index wrap guards; `grain2Pos` computed from the *new*
  `grainBase` after restart (ordering bug fixed).
- **Optimizations**: block-level loop; Hann window precomputed once per grain
  length; grain restart at 50% overlap keeps window-sum unity.

---

## Filter

### Multi Mode Filter (`MULTI_MODE_FILTER`, `MultiModeFilterEffect`)

- **Algorithm**: Orfanidis SVF (state-variable filter) with LP/BP/HP mode
  morph. `g = tan(π·fc/sr)`, `R = 1 − withinBand·0.9` (clamped ≤ 0.98).
  `bandPos = params[0]·3` selects mode and interpolates resonance.
- **Knobs**: `params[0]` (Mode) → band position 0–3 (LP→BP→HP morph + resonance);
  `params[2]` (Cutoff) → fc = 20 + 19,980·val². Res(1), Level(3) are unwired.
- **Guardrails**: state clamped to ±8.0 (runaway protection); `invScale`
  denominator ≥ 1; mode clamped to 2.
- **Optimizations**: block-level loop; per-sample state updates inlined.

### Formant Shifter / Dynamic Resonant (`FORMANT_VOCAL_SHIFTER`, `DynamicResonantFilter`)

- **Algorithm**: Envelope-followed TDF-II biquad resonator. A per-sample
  envelope follower (attack 2 ms, release 100 ms, max across channels) modulates
  the formant frequency: `center = 200 + formant·1800·(0.3 + env·0.7) Hz`,
  bandwidth `max(50 Hz, center/5)`.
- **Knobs**: `params[2]` (Formant) → base formant frequency. Q(0), Shift(1),
  Level(3) are unwired.
- **Guardrails**: biquad output `y` sanitized (NaN resets y, z1, z2);
  channel count bounded by `min(c, m_lp1.size())`; pole radius
  R = exp(−π·bw/sr) ≤ ~0.996 (stable).
- **Optimizations**: coefficients recomputed every 8 samples (sub-block rate —
  inaudible for a slow envelope, cuts exp/cos cost 8×); per-sample envelope
  tracking retained.

### Spectral Freeze (`SPECTRAL_FREEZE`, `TimeDomainFreezeEffect`)

- **Algorithm**: Time-domain freeze. Continuously writes input to a 1.5 s
  buffer; when frozen, reads a fixed-length window (1.0 s) at a fixed pitch
  ratio (0.25–2.0×) with Hann crossfades at the loop point. Entry
  (dry→frozen) and exit (frozen→dry) transitions are 32-sample crossfades.
  No LFO on the read ratio (unconditional ±5% wobble removed — clean freeze).
- **Knobs**: `params[1]` (Freeze) → ≥ 0.05 freezes, and scales pitch ratio.
  Drift(2), Window(3) are unwired. Mix(0) for blend.
- **Guardrails**: input sanitized; exit crossfade added (was an instant jump
  → click); entry crossfade linear, loop crossfade Hann (equal-power).
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

### Spectral Filter (`SPECTRAL_FILTER`, `SpectralFilterEffect`)

- **Algorithm**: TDF-II biquad bandpass/notch resonator.
  `R = 1 − π·bw/(Q·sr)`, pole radius clamped ≤ 0.995. No center-frequency
  LFO (unconditional ±10% wobble removed — knob-only control).
- **Knobs**: `params[0]` (Width) → bw = 20–4,020 Hz; `params[1]` (Center) →
  center = 100–8,100 Hz; `params[2]` (Q) → 0.5–10.0.
  Level(3) is unwired.
- **Guardrails**: R clamped to [0, 0.995] (prevents self-oscillation boundary);
  biquad states sanitized on non-finite output; channel count bounded.
- **Optimizations**: block-level loop; coefficients hoisted per block; the
  per-sample fallback uses the same design (bw = 20 + center·4000, Q = 1).

---

## Pitch / Glitch

### Granular Pitch (`PITCH_SHIFTER_GRANULAR`, `GranularPitchEffect`)

- **Algorithm**: Same dual-grain engine as Granular Delay but full-wet pitch
  shifter. Grain 0.11 s, buffer 1.0 s.
- **Knobs**: `params[2]` (Rate) → playback speed 0.5–2.0×. Spread(0), Grain(1),
  Level(3) are unwired.
- **Guardrails**: same as GranularBase (input sanitized, wrap guards, unity-sum
  windows).
- **Optimizations**: same as GranularBase.

### Frequency Shifter (`FREQ_SHIFTER`, `FrequencyShifterEffect`)

- **Algorithm**: SSB (single-sideband) frequency shift via Hilbert transform.
  Two cascades of three allpass filters each (Bristow-Johnson coefficients)
  produce a 90°-shifted quadrature signal; the shifted output is
  `x·cos(φ) + q·sin(φ)` with `φ` advancing at `shiftHz/sr` (up to 2 kHz shift,
  quadratic knob law).
- **Knobs**: `params[0]` (Shift) → shift²·2000 Hz (0–2 kHz). Spread(1), Depth(2),
  Level(3) are unwired.
- **Guardrails**: input sanitized; phase single-subtract wrap; allpass
  coefficients < 1 (stable).
- **Optimizations**: block-level wrapper; per-sample denormal guard removed;
  allpass taps inlined per sample.

### Glitch Stutter (`GLITCH_STUTTER`, `GlitchStutterEffect`)

- **Algorithm**: Slice/repeat/gate. Records into a 1.0 s buffer, plays back a
  slice with equal-power Hann crossfades at the loop boundary, repeats up to
  `1 + (1 − intens)·4` times, then gates to silence with fades. The entry
  crossfade blends from the captured last dry sample (`entryXfadeFrom`) — was a
  pure fade-from-zero that dropped the first PLAYING sample to silence.
- **Knobs**: `params[0]` (Intens) → slice length 0.05–0.5 s and repeat count
  (higher intens → longer slices, fewer repeats). Gate(1), Rate(2), Level(3)
  are unwired.
- **Guardrails**: input sanitized; slice length ≥ 2 samples; gate ≥ 1 sample;
  32-sample entry/loop/gate fades.
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

### Grain Scrubber (`GRAIN_SCRUBBER`, `GrainScrubberEffect`)

- **Algorithm**: Position-scrubbed granular playback using the shared dual-grain
  engine. `grainPosition` (0–1) selects the read window into the buffer.
- **Knobs**: `params[0]` (Pos) → scrub position; `params[3]` (Level label, used
  as speed) → playback speed 0.5–2.0×. Density(1), Size(2) are unwired.
- **Guardrails**: input sanitized in granular core; position offset wraps;
  playback speed bounded 0.5–2.0.
- **Optimizations**: block-level loop.

---

## Reverb

### Diffused Reverb (`DIFFUSED_DELAY_NETWORK`, `DiffusedReverbEffect`)

### Plate Reverb (`PLATE_REVERB`, `PlateReverbEffect`)

Both share `ReverbNetworkEffect` (a `DspEffect` wrapper) over
`processReverbNetworkSample` in `ReverbNetwork.cpp`.

- **Algorithm**:
  1. **Early reflections**: 5-tap read (offsets 882/1102/1411/1852/2426 samples,
     gains 0.32/0.20/0.13/0.07/0.03) from a 3072-sample delay line.
  2. **Diffuser**: 4 parallel combs (each with slow LFO-modulated read,
     ±50 samples, hf damping on the tap, tanh-saturated feedback) feeding a
     2-stage allpass.
  3. **Stereo decorrelation**: a cross-feeding Schroeder decorrelator
     (k = 0.25) on the mono comb output, with independent left/right state.
  - Diffused config: comb times 1373–1617 ms, combGains 0.78–0.85,
    feedbackBase 0.75 (+0.15 range), allpass coeff 0.5, allpass times
    225/556 ms.
  - Plate config: comb times 1189–1355 ms, combGains 0.85–0.90,
    feedbackBase 0.82 (+0.10 range), allpass coeff 0.65, allpass times
    178/467 ms.
- **Knobs**:
  - Diffused: `params[3]` (Decay) → feedback = 0.75 + decay·0.15, hfDamp =
    decay²·0.5. Diff(1), Size(2) unwired. Mix(0) for blend.
  - Plate: `params[2]` (Decay) → feedback = 0.82 + decay·0.10. Size(1),
    Damp(3) unwired. Mix(0) for blend.
- **Guardrails**: dry L/R sanitized; comb feedback tanh-bounded (feedback ≤ 0.92
  × combGain, ×0.7 tail); allpass |coeff| < 1; buffer lengths ≥ 1;
  `interpolateDelayRead` NaN guard covers comb reads.
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

### Convolution Space (`CONVOLUTION_SPACE`, `ConvolutionSpaceEffect`)

- **Algorithm**: Overlap-add FFT convolution with a synthetic IR
  (up to 512 taps, seeded `std::mt19937(42)` noise × exp(−4t), normalized to
  peak 0.833). Damp modifies the IR in the frequency domain (scaled decay);
  `recomputeIrFreq` re-FFTs only when damp changes. Falls back to brute-force
  per-sample convolution if the block + IR exceeds the FFT size.
- **Knobs**: `params[3]` (Damp) → IR decay slope. Space(1), Size(2) are unwired.
  Mix(0) for blend.
- **Guardrails**: brute-force path sanitizes input; IR length clamped
  [16, kFftSize/2] (raised from 256 — longer tails); damp scale bounded.
- **Optimizations**: FFT performed once per sub-block per channel; IR
  frequency-domain update cached per damp value (no recompute per block).

---

## Modulation / Dynamics

### VCA Compressor (`ENVELOPE_VCA_COMPRESSOR`, `VcaCompressorEffect`)

- **Algorithm**: Envelope-followed compressor. Peak detection across channels →
  one-pole attack/release follower → soft-knee gain reduction (4:1 ratio,
  6 dB knee) + makeup gain.
  `gain = exp2f((gain_dB + makeup_dB)·log2(10)/20)` — the standard 10^(dB/20)
  curve, implemented with a fast exp2f and the constant
  `dB20ToLinearExp2 = log2(10)/20 ≈ 0.1660964`.
- **Knobs**: `params[0]` (Attack) → 0.5–50 ms; `params[1]` (Release) →
  10–500 ms; `params[2]` (Thresh) → −45 to −5 dB; `params[3]` (Level/makeup) →
  0.5–2.0×.
- **Guardrails**: envelope floored at 1e-8 before log10 (no −inf/div0);
  gain clamped ≥ 0.0 (makeup gain may boost above unity — the old ≤ 1.0 clamp
  defeated the makeup knob); attack/release denominators floored.
- **Optimizations**: makeup_dB hoisted per block; `exp2f` replaces `pow`;
  single envelope across channels (one envelope, uniform gain).

### Rhythm Gate (`RHYTHM_GATE`, `RhythmGateEffect`)

- **Algorithm**: Volume envelope shaper (Gross Beat volume-section style) driven
  by a free-running phase accumulator with a single shared phase across all
  channels (L/R duck in lock-step — fixes the old per-channel-timer stereo
  wobble). Cycle interval `0.05 + rate·9.95` s (0.05–10 s, covers trap BPM
  ranges). The Shape knob morphs between three envelope zones:
  - 0.00–0.33 (tremolo): sine blended to triangle,
    `env = sine + (tri−sine)·t`, `t = shape/0.33`
  - 0.33–0.66 (pump): instant drop + exponential recovery,
    `env = 1 − (1−phase)^exponent`, exponent `1.5 + t·6.0` (1.5 → 7.5)
  - 0.66–1.00 (gate): hard square with variable duty cycle 50% → 80%
  Per-sample gain `gain = 1 − depth·(1−env)` (0 = dry bypass, 1 = full mute at
  trough), then shaped by a 1 ms sample-rate-aware one-pole smoother
  (`m_smoothCoeff = 1 − exp(−1/(sr·0.001))`) — removes the click at hard gate
  transitions.
- **Knobs**: `params[0]` (Rate) → cycle 0.05–10 s; `params[1]` (Shape) → zone
  morph (tremolo → pump → gate); `params[2]` (Depth) → 0–1; `params[3]` (Mix)
  drives the processor wet/dry blend — `mixKnobIndex() == 3`, so the effect must
  NOT read `params[3]`. All 4 knobs wired (the old unwired Shape/Level knobs are
  gone).
- **Guardrails**: `ScopedNoDenormals` at block entry; gain clamped [0,1]; cycle
  clamped ≥ 2 samples; `m_phase %= cycleSamples` on mid-cycle rate changes (no
  phase jump/click when the rate knob moves); `m_smoothEnv` initialized to 1.0.
- **Optimizations**: block-level loop; envelope computed once per sample and
  applied to all channels — one shared `m_phase`, no per-channel state, no
  allocations.

### Random Modulator (`RANDOM_MODULATOR`, `RandomModulatorEffect`)
- **Algorithm**: xorshift32 sample-and-hold LFO with one-pole smoothing.
  A new random value around unity (`1.0 ± depth`) is held every `sr/updateRate`
  samples (0.1–20 Hz); one-pole smoothing (1–20 Hz cutoff) glides between holds.
  Output is bipolar around unity gain and multiplied into the signal — at
  depth = 0 the signal passes untouched.
- **Knobs**: `params[0]` (Depth), `params[1]` (Smooth) → smoothing cutoff
  1–20 Hz, `params[2]` (Rate) → update rate 0.1–20 Hz. Shape(3) is unwired.
- **Guardrails**: RNG is pure bitwise (cannot produce NaN); update interval ≥ 1
  sample; smoothing argument bounded; hold/current initialized to 1.0.
- **Optimizations**: per-sample counter/smoothing inside the block loop
  (fixed: was block-rate, which slowed the modulator ~512×); smoothing
  coefficient hoisted per block.

### Reverse Buffer (`REVERSE_BUFFER`, `ReverseBufferEffect`)

- **Algorithm**: Reverse-slice playback. Records a slice of `sr·grainSec`
  samples, then plays it back backwards with a cosine crossfade at playback
  start from the captured last dry sample (`xfadeFrom`) — was a separate
  record-end fade-out plus fade-from-zero that produced an amplitude dip.
  Up to `1 + (1 − density)·4` repeats.
- **Knobs**: `params[3]` (Density) → grain length 0.05–1.0 s and repeat count.
  Length(1), Dir(2) are unwired. Mix(0) for blend.
- **Guardrails**: input sanitized; slice length clamped [2, bufSize−1];
  32-sample cosine crossfade at playback start.
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

---

## Hip-Hop FX (v0.2 additions)

### Resampler / Bitcrush (`RESAMPLE_BITCRUSH`, `ResamplerEffect`)

- **Algorithm**: Lo-fi sample-rate reduction + bit-depth quantization.
  A per-channel phase accumulator (`delta = targetRate/sr`) drives a
  sample-and-hold capture with linear-interpolated capture position and ZOH
  output. Quantization: `round(x·levels)/levels`, `levels = 2^(bits−1)`.
  Triangular PDF dither (±1 LSB, two LCG draws) precedes quantization; a 2-pole
  anti-alias biquad (cutoff = `targetRate·0.45`) and a 5 Hz DC-blocker
  (prevents DC thumps at low bit depths) run before the S&H.
- **Knobs**: `params[0]` (Rate) → exponential map
  `targetRate = sr·(500/sr)^val` (0 = full rate, 1 = 500 Hz); `params[1]` (Bits)
  → 16→2 bit; `params[2]` (Dither) → TPDF amount 0–1; `params[3]` (Filter) →
  anti-alias cutoff scale.
- **Guardrails**: input sanitized; `double` phase accumulator (float loses
  precision over long runs); cutoff recomputed only when the rate changes >5%;
  LCG per channel (allocation-free); `ScopedNoDenormals`.
- **Optimizations**: block-level; coefficients hoisted per block; no pow/exp in
  the inner loop (bit level curve precomputed per block).

### Tremolo (`TREMOLO`, `TremoloEffect`)

- **Algorithm**: Amplitude modulation LFO. Per-channel phase accumulator
  (`rate/sr`, wrap at 1.0), 90° stereo offset (ch1 starts at phase 0.25).
  Shapes: sine (`0.5 + 0.5·sin(2πφ)`), triangle
  (`1 − 2·|φ − 0.5|`), square (one-pole-smoothed at 60 Hz to kill the
  hard-step click). Gain: `(1 − depth) + depth·lfo` — depth = 0 is a no-op.
- **Knobs**: `params[1]` (Rate) → 0.1–20 Hz; `params[2]` (Depth) → 0–1;
  `params[3]` (Shape) → 0 sine, 1 triangle, 2 square. Mix(0) for blend.
- **Guardrails**: input sanitized; fast-bypass when depth < 0.001; square-wave
  smoothing state per channel; `ScopedNoDenormals`.
- **Optimizations**: block-level; shape switch hoisted outside the sample loop;
  sine/square coefficients precomputed per block.

### Flanger (`FLANGER`, `FlangerEffect`)

- **Algorithm**: LFO-modulated comb filter. Per-channel delay line
  (15 ms capacity) with sinusoidal LFO (180° stereo offset) sweeping the read
  position between 0.5 ms and 10 ms; write
  `in + tanh(delayed·feedback)` (tanh keeps the feedback loop bounded);
  output the flanged tap only — dry/wet blend is external via Mix.
- **Knobs**: `params[1]` (Rate) → 0.1–5 Hz; `params[2]` (Depth) → sweep range
  fraction; `params[3]` (Feed) → 0–0.9. Mix(0) for blend.
- **Guardrails**: input sanitized; read position guard
  (`writePtr + bufSize − delay`, wrapped) keeps `interpolateDelayRead` in
  range; delay clamped to `bufSize−1`; `ScopedNoDenormals`.
- **Optimizations**: block-level; per-channel buffers pre-allocated in
  `prepare` (no audio-thread allocation).

### Analog Octaver (`ANALOG_OCTAVER`, `OctaverEffect`)

- **Algorithm**: Analog-style octave generator (Boss OC-2 lineage — zero
  latency, chord-tolerant). Sub-octave: zero-crossing flip-flop divider
  (Schmitt trigger deadzone 0.005) producing a half-frequency square, shaped
  by a 200 Hz lowpass. Upper octave: 400 Hz bandpass (Q 1.5) → full-wave
  rectifier → 1-pole DC blocker (rectification adds ~0.64·peak DC). Wet mix
  `sub·subLevel + upper·upperLevel` shaped by a tone lowpass (100 Hz–2 kHz).
- **Knobs**: `params[1]` (Sub) → sub level 0–1; `params[2]` (Upper) → upper
  level 0–1; `params[3]` (Tone) → 100 Hz–2 kHz LP. Mix(0) for blend.
- **Guardrails**: input sanitized; flip-flop holds state on silence (no stuck
  false triggers); wet output sanitized after the biquad chain; coefficients
  recomputed only when Tone moves >5%; `ScopedNoDenormals`.
- **Optimizations**: block-level; all filter coefficients precomputed in
  `prepare` (fixed bandpass/sub LP) or on-demand (tone); zero heap.

---

## Bypass

### Bypass (`BYPASS`)

- **Algorithm**: No DSP. The compiler excludes BYPASS slots from the routing
  chain entirely; the signal passes through unchanged (input gain, mix, pedal
  gain, and the output softClip still apply).
- **Guardrails**: n/a.
