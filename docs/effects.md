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
  delayed tap. Feedback fixed at 0.85, damped by a 3 kHz one-pole lowpass.
- **Knobs**: `params[0]` (Freq) → `freq = 20·pow(66.666, val)` (20 Hz–1.33 kHz).
  Feed(1), Decay(2), Level(3) are unwired (feedback/damp hardcoded).
- **Guardrails**: input sanitized; **delayed read sanitized** (prevents NaN
  feedback loop); `delaySamples` clamped to `bufSize−1`; `bufSize == 0` guard.
- **Optimizations**: block-level loop; delay read pointer precomputed per sample
  with a single modulo.

---

## Delay

### Simple Delay (`SIMPLE_DELAY`, `SimpleDelayEffect`)

- **Algorithm**: Interpolated delay line (linear interpolation) with damped
  feedback and a slow LFO on the read position. Write
  `in + tanh(fbLp·feedback)`; output interpolated delayed tap.
  Delay time one-pole-smoothed per block (`+= 0.05·(target − current)`) to avoid
  pitch jumps when the Time knob moves.
- **Knobs**: `params[1]` (Time) → 0.1–1.0 s; `params[2]` (Feed) → 0.3–0.9;
  `params[3]` (Damp) → one-pole LP cutoff 500 Hz–15.5 kHz. Mix(0) for blend.
- **Guardrails**: input sanitized; `delaySamples` clamped to `bufSize−1`
  (fixed zero-delay wrap bug); `bufSize == 0` guard; feedback ≤ 0.9 + tanh clamp.
- **Optimizations**: block-level; LFO sin per sample inside the block loop;
  dampCoeff hoisted per block.

### MicroPitch Chorus (`MICROPITCH_CHORUS`, `MicroPitchChorusEffect`)

- **Algorithm**: Dual-tap detuned chorus. Two reads from a delay line at
  `±detune` cents with per-channel LFO-modulated positions.
  Detune: `effectParam·50` cents.
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
  crossfades over 32 samples to hide the speed discontinuity. Wow/flutter
  modulation: ±1.8-sample sine wobble on the read head.
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
  buffer; when frozen, reads a fixed-length window (1.0 s) with an LFO-modulated
  pitch ratio (0.25–2.0×) and Hann crossfades at the loop point. Entry
  (dry→frozen) and exit (frozen→dry) transitions are 32-sample crossfades.
- **Knobs**: `params[1]` (Freeze) → ≥ 0.05 freezes, and scales pitch ratio.
  Drift(2), Window(3) are unwired. Mix(0) for blend.
- **Guardrails**: input sanitized; exit crossfade added (was an instant jump
  → click); entry crossfade linear, loop crossfade Hann (equal-power).
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

### Spectral Filter (`SPECTRAL_FILTER`, `SpectralFilterEffect`)

- **Algorithm**: TDF-II biquad bandpass/notch resonator with an LFO on the
  center frequency. `R = 1 − π·bw/(Q·sr)`, pole radius clamped ≤ 0.995.
- **Knobs**: `params[0]` (Width) → bw = 20–4,020 Hz; `params[1]` (Center) →
  center = 100–8,100 Hz (+ ±10% LFO wobble); `params[2]` (Q) → 0.5–10.0.
  Level(3) is unwired.
- **Guardrails**: R clamped to [0, 0.995] (prevents self-oscillation boundary);
  biquad states sanitized on non-finite output; channel count bounded.
- **Optimizations**: block-level loop; coefficients hoisted per block; LFO sin
  once per block.

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
  `1 + (1 − intens)·4` times, then gates to silence with fades.
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
  (256 taps, seeded `std::mt19937(42)` noise × exp(−4t), normalized to peak
  0.833). Damp modifies the IR in the frequency domain (scaled decay);
  `recomputeIrFreq` re-FFTs only when damp changes. Falls back to brute-force
  per-sample convolution if the block + IR exceeds the FFT size.
- **Knobs**: `params[3]` (Damp) → IR decay slope. Space(1), Size(2) are unwired.
  Mix(0) for blend.
- **Guardrails**: brute-force path sanitizes input; IR length clamped [16, 256];
  damp scale bounded.
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
  gain clamped ≤ 1.0 (no makeup overshoot); attack/release denominators floored.
- **Optimizations**: makeup_dB hoisted per block; `exp2f` replaces `pow`;
  single envelope across channels (one envelope, uniform gain).

### Sidechain Pump / Ducker (`SIDECHAIN_DUCKER`, `SidechainDuckerEffect`)

- **Algorithm**: Rhythmic ducking oscillator. Periodic gain window
  `1 − duck·(1−phase)³` over an interval of 0.05–2.0 s. Per-channel timers.
- **Knobs**: `params[0]` (Rate) → interval 0.05–2.0 s; `params[2]` (Amount) →
  duck depth 0–1. Shape(1), Level(3) are unwired.
- **Guardrails**: interval ≥ 1 sample; duck clamped [0,1]; per-channel timers
  wrapped.
- **Optimizations**: block-level loop with per-channel state.

### Random Modulator (`RANDOM_MODULATOR`, `RandomModulatorEffect`)

- **Algorithm**: xorshift32 sample-and-hold LFO with one-pole smoothing.
  A new random value (±depth) is held every `sr/updateRate` samples
  (0.1–20 Hz); one-pole smoothing (1–20 Hz cutoff) glides between holds.
  Output mapped unipolar (0–1) and multiplied into the signal.
- **Knobs**: `params[0]` (Depth), `params[1]` (Smooth) → smoothing cutoff
  1–20 Hz, `params[2]` (Rate) → update rate 0.1–20 Hz. Shape(3) is unwired.
- **Guardrails**: RNG is pure bitwise (cannot produce NaN); update interval ≥ 1
  sample; smoothing argument bounded.
- **Optimizations**: per-sample counter/smoothing inside the block loop
  (fixed: was block-rate, which slowed the modulator ~512×); smoothing
  coefficient hoisted per block.

### Reverse Buffer (`REVERSE_BUFFER`, `ReverseBufferEffect`)

- **Algorithm**: Reverse-slice playback. Records a slice of `sr·grainSec`
  samples, then plays it back backwards with a Hann fade-in at playback start,
  a linear fade-out at record end, and up to `1 + (1 − density)·4` repeats.
- **Knobs**: `params[3]` (Density) → grain length 0.05–1.0 s and repeat count.
  Length(1), Dir(2) are unwired. Mix(0) for blend.
- **Guardrails**: input sanitized; slice length clamped [2, bufSize−1];
  32-sample fades at playback/record boundaries.
- **Optimizations**: block-level wrapper; per-sample denormal guard removed.

---

## Bypass

### Bypass (`BYPASS`)

- **Algorithm**: No DSP. The compiler excludes BYPASS slots from the routing
  chain entirely; the signal passes through unchanged (input gain, mix, pedal
  gain, and the output softClip still apply).
- **Guardrails**: n/a.
