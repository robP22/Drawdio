# Drawdio Effect Reference

This is the current reference for the 24 factory-backed effects. Names and
display labels follow `Source/State/PedalDefinition.cpp`; algorithms and
parameter wiring are verified against `Source/Effects/` and `Source/Dsp/`.

## Conventions

- Effect parameters are normalized `params[0..3]` values in the range [0, 1].
- A mix knob is handled by `UnifiedPedalProcessor` when
  `mixKnobIndex() >= 0`; the effect does not consume that parameter itself.
- Unlabeled knobs are retained for the uniform four-knob pedal layout but are
  not necessarily wired to DSP behavior.
- Effects are prepared before publication and process blocks without allocating.
- BYPASS is excluded from the compiled routing chain.

## Active Catalog

| ID | Module | Display name | Knob labels | External mix |
|---:|---|---|---|---|
| 1 | `WAVESHAPER_DISTORTION` | Wave Shaper | - / - / Drive / - | No |
| 2 | `MICROPITCH_CHORUS` | MicroPitch | Mix / Depth / Detune / Rate | 0 |
| 3 | `MULTI_MODE_FILTER` | Multi-Mode Filter | Mode / - / Cutoff / - | No |
| 4 | `PITCH_SHIFTER_GRANULAR` | Pitch Shifter | - / - / Pitch / - | No |
| 5 | `ENVELOPE_VCA_COMPRESSOR` | VCA Compressor | Attack / Release / Thresh / Level | No |
| 6 | `GLITCH_STUTTER` | Glitch Stutter | Intens / - / - / - | No |
| 7 | `DIFFUSED_DELAY_NETWORK` | Diffused Reverb | Mix / - / - / Decay | 0 |
| 8 | `MATHEMATICAL_WAVEFOLDER` | Wave Folder | - / Fold / - / - | No |
| 9 | `FORMANT_VOCAL_SHIFTER` | Formant Shifter | Q / Shift / Formant / Level | No |
| 10 | `RETIME` | Re-Time | Mix / Speed / Loop / Smooth | 0 |
| 11 | `SIMPLE_DELAY` | Simple Delay | Mix / Time / Feed / Damp | 0 |
| 12 | `PLATE_REVERB` | Plate Reverb | Mix / - / Decay / - | 0 |
| 13 | `RHYTHM_GATE` | Rhythm Gate | Rate / Shape / Depth / Mix | 3 |
| 14 | `GRANULAR_DELAY` | Granular Delay | Mix / Spread / Size / Rate | 0 |
| 15 | `COMB_RESONATOR` | Comb Resonator | Freq / - / - / - | No |
| 16 | `SPECTRAL_FREEZE` | Time Freeze | Mix / Freeze / - / - | 0 |
| 17 | `FREQ_SHIFTER` | Frequency Shifter | Shift / - / - / - | No |
| 18 | `REVERSE_BUFFER` | Reverse Buffer | Mix / - / - / Density | 0 |
| 19 | `GRAIN_SCRUBBER` | Grain Scrubber | Position / - / - / Rate | No |
| 20 | `SPECTRAL_FILTER` | Resonant Filter | Width / Center / Q / Level | No |
| 21 | `CONVOLUTION_SPACE` | Convolution Space | Mix / Size / Width / Damp | 0 |
| 23 | `RESAMPLE_BITCRUSH` | Resampler | Rate / Bits / Dither / Filter | No |
| 24 | `TREMOLO` | Tremolo | Mix / Rate / Depth / Shape | 0 |
| 25 | `FLANGER` | Flanger | Mix / Rate / Depth / Feed | 0 |

IDs 22 and 26 are reserved for removed Random Modulator and Analog Octaver
implementations. State loading migrates those IDs to Bypass.

## Distortion

### Wave Shaper

`WAVESHAPER_DISTORTION` uses a first-order antiderivative anti-aliased
arctangent soft clipper. The antiderivative is evaluated between consecutive
input samples, with a derivative fallback when the sample difference is near
zero. Drive is read from `params[2]`; the other displayed positions are not
wired. Input samples are sanitized and the drive bypass window avoids the
nonlinearity at very low drive. First-order ADAA reduces aliasing but does not
eliminate it at extreme drive levels.

### Wave Folder

`MATHEMATICAL_WAVEFOLDER` uses a sine-fold curve with first-order ADAA. Fold is
read from `params[1]`, with the fold multiplier capped at 4.0. Input and sample
difference guards protect the antiderivative calculation. Higher fold settings
remain intentionally colored and can generate aliasing.

### Comb Resonator

`COMB_RESONATOR` is a Karplus-Strong-style fractional delay resonator. Frequency
is read from `params[0]` and maps approximately from 20 Hz to 1.33 kHz. Feedback
is fixed in the implementation, passed through a 3 kHz damping filter and
`tanh`, then written back into the delay line. Fractional interpolation avoids
the zippering of integer delay changes. Delay length and input reads are
bounded, but the reported tail flag is currently channel-order dependent and
the fixed host tail estimate is conservative only for shorter settings.

## Delay and Granular

### MicroPitch

`MICROPITCH_CHORUS` uses per-channel delay state and two detuned read taps.
Depth, detune, and rate are read from `params[1]`, `params[2]`, and `params[3]`.
Detune spans approximately +/-50 cents and rate spans 0.05 to 3 Hz. The taps
are free-running delay positions rather than a fixed delay window, so long
continuous operation can pass through zero-delay comb-like regions.

### Simple Delay

`SIMPLE_DELAY` uses per-channel feedback delay lines with four-point cubic
interpolation for normal buffer sizes and a linear fallback for very small
buffers. Time, feedback, and damping are read from `params[1..3]`; mix is
external at `params[0]`. Delay time is smoothed once per block and ramped within
the block. Feedback is bounded by a maximum of 0.9, damping, and `tanh`.

### Re-Time

`RETIME` is the active implementation for slot 10. It records per-channel
buffers and reads them at a transport-synchronized variable speed. The Speed,
Loop, and Smooth controls use `params[1..3]`; `params[0]` is the external mix.
Loop length is derived from BPM/PPQ transport information when available and
is bounded to the prepared buffer. Linear interpolation and output smoothing
reduce transport and loop discontinuities. The effect reports an 8-second
maximum tail.

### Granular Delay

`GRANULAR_DELAY` uses the shared dual-grain engine with a nominal 0.15-second
grain and 2-second buffer. Two Hann-windowed grains are offset near 50% of a
grain length. Rate is read from `params[3]` and maps through `exp2` to 0.5x-2x;
the other non-mix positions are currently unwired. Spray is bipolar and can
shift the second grain by up to approximately 10% of the grain length. The
window sum is therefore approximately, not exactly, unity. Grain-base
arithmetic remains a known unsigned-underflow risk near buffer boundaries.

### Granular Pitch

`PITCH_SHIFTER_GRANULAR` uses the same dual-grain engine as Granular Delay with
an approximately 0.11-second grain and 1-second buffer. Pitch/rate is read from
`params[2]` and maps to 0.5x-2x. It is a full-wet effect; the remaining displayed
positions are unwired. The shared granular boundary and seed limitations apply.

### Grain Scrubber

`GRAIN_SCRUBBER` uses the shared dual-grain engine with position from `params[0]`
and playback rate from `params[3]`. Position selects a read window in the
prepared buffer; rate maps to 0.5x-2x. Density and Size are currently unwired.
The full position range makes the shared unsigned grain-base boundary case more
likely than in the other granular effects.

### Reverse Buffer

`REVERSE_BUFFER` records and plays back per-channel slices in reverse. Density
controls slice duration and repeat count through `params[3]`; mix is external.
Slice length is bounded to the prepared buffer and entry playback uses a
32-sample cosine transition from the captured dry signal. Repeat transitions
currently reuse the original crossfade source, which can create a hard splice
at later repeat boundaries.

## Filters and Pitch

### Multi-Mode Filter

`MULTI_MODE_FILTER` is a state-variable LP/BP/HP filter with mode morphing.
Mode is `params[0]`; cutoff is `params[2]` and follows a squared mapping from
20 Hz toward 20 kHz. The digital cutoff coefficient uses tangent prewarping and
resonance is bounded. The block path clamps state magnitude, but non-finite
state recovery and an explicit below-Nyquist cutoff cap are not complete.

### Formant Shifter

`FORMANT_VOCAL_SHIFTER` follows the input envelope across channels and drives a
TDF-II resonant biquad. Formant frequency is controlled by `params[2]`; Q,
Shift, and Level are displayed but not wired. Bandwidth is bounded to at least
50 Hz or one fifth of the center frequency. Coefficients are currently
recomputed per sample, despite older documentation claiming eight-sample
updates. Non-finite biquad output resets the filter state.

### Time Freeze

`SPECTRAL_FREEZE` continuously records a 1.5-second per-channel buffer and,
when Freeze is enabled through `params[1]`, loops a one-second window at a
0.25x-2x pitch ratio. Mix is external at `params[0]`. Entry, exit, and loop
transitions use short cosine/raised-cosine fades. Drift and Window positions
are currently unwired. The freeze path should be treated as a time-domain
freeze, not an FFT spectral freeze.

### Resonant Filter

`SPECTRAL_FILTER` is a TDF-II bandpass resonator. Width, Center, and Q use
`params[0..2]`; Level is not wired. Center spans roughly 100 Hz-8.1 kHz and
width roughly 20 Hz-4.02 kHz. Pole radius is bounded below one and filter state
is reset on non-finite output. Coefficients are hoisted per block, so fast
parameter changes can step at block boundaries.

### Frequency Shifter

`FREQ_SHIFTER` uses two three-section allpass cascades to approximate a Hilbert
quadrature pair, then performs single-sideband frequency shifting. Shift is read
from `params[0]` with a quadratic map to 0-2 kHz. The allpass coefficients are
stable, but quadrature accuracy is limited outside the coefficient design band;
image-sideband leakage is expected near the extreme low and high frequencies.

## Reverb

### Diffused Reverb and Plate Reverb

`DIFFUSED_DELAY_NETWORK` and `PLATE_REVERB` share `ReverbNetworkEffect`.
The network has five early-reflection taps, an eight-line feedback delay network,
per-line damping, and a decorrelation output stage. It is not a four-comb,
two-allpass Schroeder implementation. Diffused Reverb reads Decay from
`params[3]`; Plate Reverb reads Decay from `params[2]`. Their other displayed
non-mix positions are currently unwired.

The current network forms a mono input feed for the shared FDN and derives both
outputs from the shared network result with decorrelation state. This means the
effect does not preserve independent stereo input through the network.
Feedback is bounded by the damping, decay scaling, and Householder-like mixing
matrix. The matrix is stable at the documented parameter ranges.

### Convolution Space

`CONVOLUTION_SPACE` uses a seeded synthetic exponential-decay impulse response
and overlap-add FFT convolution. The FFT order is 11, giving a 2048-point FFT;
the IR is clamped between 16 and 1024 samples. Sixteen damped frequency-domain
variants are prepared before publication. Damp selects among/interpolates these
variants. For a host block where `n + irLen > 2048`, the implementation uses a
brute-force fallback; normal blocks use FFT sub-block processing. Mix is
external at `params[0]`; Size and Width are currently unwired, and Damp is read
from `params[3]`.

## Dynamics and Modulation

### VCA Compressor

`ENVELOPE_VCA_COMPRESSOR` uses a channel-linked peak envelope, attack/release
one-pole smoothing, soft-knee 4:1 gain reduction, and makeup gain. Attack,
Release, Threshold, and Level use `params[0..3]`. The envelope is floored before
log conversion, attack/release denominators are bounded, and makeup gain is
allowed above unity.

### Rhythm Gate

`RHYTHM_GATE` is a volume-envelope shaper with a shared phase across channels.
Rate selects a 0.05-10 second cycle. Shape morphs between sine/triangle tremolo,
an exponential pump, and a variable-duty square gate. Depth controls the gain
range. Mix is external at `params[3]`; the effect does not read that parameter.
A sample-rate-aware 1 ms smoother reduces hard-gate clicks. Phase is preserved
when cycle length changes and gain is clamped to [0, 1].

### Resampler

`RESAMPLE_BITCRUSH` applies a two-pole pre-sample-and-hold filter, sample-rate
reduction, bit quantization, optional TPDF dither, and DC blocking. Rate,
Bits, Dither, and Filter use `params[0..3]`. The rate target maps exponentially
from the host rate toward 500 Hz. Quantization uses round-to-nearest with a
normalized level count. The TPDF amplitude is based on two independent LCG
draws and is canonical in normalized quantizer units. The anti-alias filter is
deliberately modest rather than a steep decimator filter; the sample-rate
reduction retains intentional lo-fi aliasing.

### Tremolo

`TREMOLO` uses per-channel phase accumulators and a 90-degree stereo offset.
Rate, Depth, and Shape use `params[1..3]`; Mix is external. Sine, triangle, and
smoothed-square waveforms are available. Depth zero is a bypass of the gain
modulation. The waveform selection and smoothing state are processed inside the
block loop.

### Flanger

`FLANGER` uses per-channel interpolated comb delay lines with a sinusoidal LFO,
180-degree stereo offset, and `tanh`-bounded feedback. Rate, Depth, and Feed use
`params[1..3]`; Mix is external. Delay sweeps approximately 0.5-10 ms inside a
15 ms prepared buffer. Through-zero flanging is not implemented. The effect has
no dedicated tail override beyond the base effect behavior.

## Bypass

`BYPASS` performs no effect processing. The compiler excludes it from the active
chain, while input gain, pedal gain, wet/dry routing, and output soft clipping
remain part of the surrounding processor path.

## Related Documentation

- [`architecture.md`](./architecture.md) - processing and lifecycle details
- [`ui-controls.md`](./ui-controls.md) - user-facing control behavior
- [`audio-pipeline-audit-2026-08-22.md`](./audits/audio-pipeline-audit-2026-08-22.md) - dated findings and limitations
