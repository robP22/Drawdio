# Drawdio Effect Reference

This is the current reference for the 25 factory-backed effects plus BYPASS.
Names and display labels follow `Source/State/PedalDefinition.cpp`; algorithms
and parameter wiring are verified against `Source/Effects/` and `Source/Dsp/`.

## Conventions

- Effect parameters are normalized `params[0..3]` values in the range [0, 1].
- Every effect has an external mix knob: `mixKnobIndex()` tells
  `UnifiedPedalProcessor` which parameter index is the wet/dry blend; the
  effect never consumes that parameter itself. Unlabeled knob slots are hidden
  by the pedal UI, so the mix position varies per pedal.
- The knob labels below are listed in pedal order (top-left, top-right,
  bottom-left, bottom-right); "-" marks a hidden/unwired slot.
- Effects are prepared before publication and process blocks without allocating.
- BYPASS is excluded from the compiled routing chain.

## Knob snapping

Several parameters snap to evenly spaced detents (HalfTime/ShaperBox-style);
the grid is defined per knob in `PedalDefinitions` and applied on drag, on
compiled display, on linked/canvas automation (display and final DSP value).
Current grids: Re-Time Time (5: 0.25/0.5/0.75/1/2x), Re-Time Bars (4:
half/1/2/4 bars), Sidechain Rate (5: 1/6, 1/4, 1/3, 1/2, 1 beat at transport
BPM), Pitch Shifter Pitch (25: -12 to +12 semitones in 100-cent steps),
Bitcrusher Bits (15: 2-16 bits), Convolution Reverb Damp (15: 16 IR rows),
Tremolo Shape (3: sine/triangle/square). The mix knob never snaps.

## Active Catalog

| ID | Module | Display name | Knob labels | Mix index |
|---:|---|---|---|---|
| 1 | `WAVESHAPER_DISTORTION` | Wave Shaper | Mix / - / Drive / - | 0 |
| 2 | `CHORUS` | Chorus | Mix / Depth / - / Rate | 0 |
| 3 | `MULTI_MODE_FILTER` | Multi-Mode Filter | Mode / Mix / Cutoff / - | 1 |
| 4 | `PITCH_SHIFTER` | Pitch Shifter | Mix / - / Pitch / - | 0 |
| 5 | `ENVELOPE_VCA_COMPRESSOR` | VCA Compressor | Attack / Mix / Thresh / Level | 1 |
| 6 | `GLITCH_STUTTER` | Glitch Stutter | Intens / Mix / Random / Smooth | 1 |
| 7 | `DIFFUSED_DELAY_NETWORK` | Diffused Reverb | Mix / Size / - / Decay | 0 |
| 8 | `MATHEMATICAL_WAVEFOLDER` | Wave Folder | Mix / Fold / - / - | 0 |
| 9 | `FORMANT_VOCAL_SHIFTER` | Formant Shifter | Mix / Shift / Formant / Q | 0 |
| 10 | `RETIME` | Re-Time | Mix / Time / Bars / Shift | 0 |
| 11 | `SIMPLE_DELAY` | Delay | Mix / Time / Feed / Damp | 0 |
| 12 | `PLATE_REVERB` | Plate Reverb | Mix / Size / Decay / - | 0 |
| 13 | `SIDECHAIN` | Sidechain | Rate / Shape / Depth / Mix | 3 |
| 14 | `GRANULAR_DELAY` | Granular Delay | Mix / Spread / Size / Delay | 0 |
| 15 | `COMB_RESONATOR` | Comb Resonator | Freq / Mix / - / - | 1 |
| 16 | `SPECTRAL_FREEZE` | Time Freeze | Freeze / Mix / Offset / - | 1 |
| 17 | `FREQ_SHIFTER` | Frequency Shifter | Shift / Mix / - / - | 1 |
| 18 | `REVERSE_BUFFER` | Reverse Buffer | Mix / - / Smooth / Density | 0 |
| 19 | `GRAIN_SCRUBBER` | Grain Scrubber | Position / Mix / - / Rate | 1 |
| 20 | `SPECTRAL_FILTER` | Resonant Filter | Width / Center / Q / Mix | 3 |
| 21 | `CONVOLUTION_SPACE` | Convolution Space | Mix / Size / Width / Damp | 0 |
| 22 | `HP_LP_FILTER` | HP/LP Filter | Mix / High / Low / Reso | 0 |
| 23 | `BITCRUSHER` | Bitcrusher | Mix / Rate / Bits / Filter | 0 |
| 24 | `TREMOLO` | Tremolo | Mix / Rate / Depth / Shape | 0 |
| 25 | `FLANGER` | Flanger | Mix / Rate / Depth / Feed | 0 |

Slot 26 is reserved for the removed Analog Octaver; state loading migrates it
to Bypass. Slot 2 previously held MicroPitch Chorus; integer 2 is unchanged so
old presets now load the Chorus.

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
bounded and the tail flag aggregates the peak across channels.

## Chorus and Delay

### Chorus

`CHORUS` (slot 2, formerly MicroPitch Chorus) is a classic single-tap chorus:
a fixed 30 ms center delay with a sinusoidal LFO read-position modulation.
Depth (`params[1]`) spans 0-20 ms and Rate (`params[3]`) spans 0.05-3 Hz.
The read position is bounded between 10 ms and 50 ms behind the write head by
construction, so the tap can never pass through zero delay or read un-written
buffer territory. Per-channel LFO phases provide stereo width. The previous
free-running detuned-tap design (which drifted through the write head on long
runs) is replaced by this implementation.

### Simple Delay

`SIMPLE_DELAY` uses per-channel feedback delay lines with four-point cubic
interpolation for normal buffer sizes and a linear fallback for very small
buffers. Time, feedback, and damping are read from `params[1..3]`; mix is
external at `params[0]`. Delay time is smoothed once per block and ramped within
the block; the first block after prepare/reset snaps directly to the compiled
time (no load-time pitch glide). Feedback is bounded by a maximum of 0.9,
damping, and `tanh`.

### Re-Time

`RETIME` is a transport-synced loop player. It records a 16-second per-channel
ring and copies the last `loopLength` samples into a dedicated freeze buffer at
sync (transport start/jump, loop-length change, or loop-end wrap). Playback
reads the freeze buffer at a variable speed, so the played loop is never
overwritten by the advancing record head. On a cold transport start, Re-Time
passes the input through until enough current-session history exists for a valid
capture. Time (`params[1]`) selects the
playback ratio from 0.25x / 0.5x / 0.75x / 1x / 2x (snapped to five detents;
2x is double time), Bars (`params[2]`) selects a half / 1 / 2 / 4-bar capture
(snapped to four detents, quantized from BPM/PPQ transport), and Shift
(`params[3]`) offsets the playback start within the captured loop. The read
uses linear interpolation; recaptures (timing changes, transport sync, loop
wrap, or host seek) crossfade from the last output sample into the new capture
over 40 ms (a held-value cosine blend — no old-loop seam is ever read, so loop
switches are click-free). When transport stops, the captured output releases to
silence over 50 ms instead of continuing to loop. A conservative input-silence
timeout also releases the loop if the host remains marked playing without
providing audio.

### Granular Delay

`GRANULAR_DELAY` uses the shared dual-grain engine with a nominal 0.15-second
grain and 2-second buffer. Two independent Hann-windowed grain heads are
staggered by half a grain length, so the summed window is continuous and grain
restarts are click-free. Rate is read from `params[3]` and maps through `exp2`
to 0.5x-2x; the other non-mix positions are currently unwired. Spray jitter
shifts each head's read window by up to approximately 10% of the buffer.
Grain windows are selected with a write-head-safe clamp: the read window always
ends at least 128 samples behind the record head, with a tighter bound at slow
rates where the read head is slower than the write head.

### Granular Pitch

`PITCH_SHIFTER` (slot 4, formerly the granular pitch variant) is a classic two-read crossfade pitch shifter (Eventide
H910/H3000-class), not a granular engine. Per-channel 1s delay line (allocated in
`prepare`); the primary read advances at `speed = exp2(pitch·2−1)` (0.5x–2x,
±12 semitones, semitone-snapped in 25 detents) with Catmull-Rom interpolation.
300ms initial delay. Two fixed latency windows keep the read inside the ring:
at speed > 1 the gap-to-write-head shrinks and a 120ms equal-power raised-cosine
crossfade to a secondary read 120ms behind triggers below 180ms (gap cycles
180–300ms); at speed < 1 the gap grows and the mirror trigger fires above 400ms,
jumping the read 120ms toward the write head (gap cycles 280–400ms). Latency is
bounded to ≤400ms in both directions; the read never laps the write head at any
speed. Back-to-back fade density at octave shifts (~4–8 fades/s) is the accepted
classic dual-read characteristic; mild comb during fades on dense material is
inherent to the two-head design (a multi-head or granular-OLA upgrade would be
the "transparent" option). Unity pitch is a clean fixed-delay copy.

### Glitch Stutter

`GLITCH_STUTTER` captures a slice of the 1s ring into a per-channel freeze
buffer and repeats it with a trailing gate. Intensity (`params[0]`) maps
inverted: low = one 0.5s repeat, high = five 0.05s repeats (dense stutter).
Random (`params[2]`) pulls the capture source from a random window of the ring
(0 = always the most recent window; stereo-coherent, one draw per capture).
Smooth (`params[3]`) maps the entry/loop-wrap/exit/gate fade length
exponentially from ~2ms to ~80ms, clamped to a quarter of the slice length.
All fades read the frozen slice, so repeats replay the exact captured material;
the exit fades cleanly to silence (no live bleed-through).

### Grain Scrubber

`GRAIN_SCRUBBER` uses the shared dual-grain engine with position from `params[0]`
and playback rate from `params[3]`. Position selects a read window in the
prepared buffer (monotonic: low position reads the most recent audio, high
position reads the oldest reachable audio); rate maps to 0.5x-2x. The shared
engine's write-head clamp keeps the full position range safe: the window can
never wrap past the record head, which previously produced crushed/garbled
output near the top of the Position range.

### Reverse Buffer

`REVERSE_BUFFER` records and plays back per-channel slices in reverse. Density
controls slice duration and repeat count through `params[3]` (low = sparse:
long slices, single playback; high = dense: short slices, multiple repeats);
mix is external. Smooth (`params[2]`) maps the entry/repeat/exit crossfade
length exponentially from ~2ms (percussive) to ~80ms (swell), clamped to a
quarter of the slice length. Entry playback transitions from the captured dry
signal; repeat transitions re-capture the crossfade source from the last played
sample, so every splice stays click-free.

## Filters and Pitch

### Multi-Mode Filter

`MULTI_MODE_FILTER` is a state-variable LP/BP/HP filter with mode morphing.
Mode is `params[0]`; cutoff is `params[2]` and follows a squared mapping from
20 Hz toward 20 kHz, capped at 45% of the sample rate. The digital cutoff
coefficient uses tangent prewarping and resonance is bounded. Both process
paths clamp state magnitude and reset non-finite state.

### Formant Shifter

`FORMANT_VOCAL_SHIFTER` follows the input envelope across channels and drives a
TDF-II resonant biquad. Formant frequency is controlled by `params[2]`; Shift
(`params[1]`) adds up to 2.4 kHz to the center, and Q (`params[3]`) narrows the
bandwidth from 0.5 to 10. Bandwidth is bounded to at least 50 Hz or one fifth
of the center frequency, divided by Q, and the pole radius is capped at 0.995.
Coefficients are recomputed per sample. Non-finite biquad output resets the
filter state.

### Time Freeze

`SPECTRAL_FREEZE` continuously records a 1.5-second per-channel buffer and,
when Freeze is enabled through `params[0]`, loops a one-second window at
natural pitch (1.0x). Mix is external at `params[1]`. Offset (`params[2]`)
sets the playback start inside the captured loop (`readPos = offset *
freezeLen` at each freeze) with a short entry/exit/offset crossfade that
avoids clicks. The path should be treated as a time-domain freeze, not an FFT
spectral freeze.

### Resonant Filter

`SPECTRAL_FILTER` is a TDF-II bandpass resonator. Width, Center, and Q use
`params[0..2]`; mix is external at `params[3]`. Center spans roughly 100 Hz-
8.1 kHz and width roughly 20 Hz-4.02 kHz. Pole radius is bounded below one and
filter state is reset on non-finite output. Coefficients are recomputed per
sample from parameters interpolated within each block, so fast knob and
automation changes do not step at block boundaries.

### Frequency Shifter

`FREQ_SHIFTER` uses two four-section allpass cascades (the classic 8-section
90-degree phase splitter, coefficients in the `(z^-1 - a)/(1 - a z^-1)` form)
to approximate a Hilbert quadrature pair, then performs single-sideband
frequency shifting with a continuous phase accumulator. Shift is read from
`params[0]` with a linear 0-2 kHz map. Mix is external at `params[1]`.

## Reverb

### Diffused Reverb and Plate Reverb

`DIFFUSED_DELAY_NETWORK` and `PLATE_REVERB` share `ReverbNetworkEffect`.
The network has five early-reflection taps, an eight-line feedback delay network,
per-line damping, and a decorrelation output stage. It is not a four-comb,
two-allpass Schroeder implementation. Diffused Reverb reads Decay from
`params[3]`; Plate Reverb reads Decay from `params[2]`.

The current network forms a mono input feed for the shared FDN and derives both
outputs from the shared network result with decorrelation state. This means the
effect does not preserve independent stereo input through the network.
Feedback is bounded by damping, decay scaling, `tanh`, and the Householder-like
mixing matrix. The matrix is stable at the documented parameter ranges.

### Convolution Space

`CONVOLUTION_SPACE` is a uniform partitioned convolution reverb. At prepare
time a seeded synthetic IR is generated (direct impulse + three early
reflections + an exponentially-decaying diffusive tail), split into 512-sample
partitions, and each partition's spectrum is FFT'd once (all prepare-time,
~0.8s tail, 69 partitions at 44.1 kHz). The tail is a one-pole lowpassed noise
whose cutoff rolls 8 kHz to 1 kHz across the IR (a correlated, darkening wash
rather than raw white noise, which convolved to a static-like haze); the fixed
envelope is mild (-7 dB over the IR) so the Size knob's RT60 scale governs the
decay. Per block the input is FFT'd once and each active partition (decay
scale > 1e-4, so CPU scales with the decay) is multiplied by its spectrum,
inverse-FFT'd, and cascade overlap-added with a piecewise-linear decay ramp.
All four knobs are wired: Mix is external at `params[0]`; Size (`params[1]`)
maps the tail RT60 from 0.15 s to 1.5 s via per-partition decay scales; Width
(`params[2]`) decorrelates the stereo tails by blending a second
(different-seed) IR spectrum set into the right channel (width 0 = identical
tails); Damp (`params[3]`) is a biquad lowpass on the input (20 kHz to
1.2 kHz, per-sample coefficient interpolation). No allocations on the audio
thread.

### HP/LP Filter

`HP_LP_FILTER` (slot 22, formerly the dead Random Modulator slot) is a series
pair of TDF-II biquads: a high-pass with a logarithmic 30 Hz-2 kHz sweep
(`params[1]`, "High") and a low-pass with a logarithmic 500 Hz-16 kHz sweep
(`params[2]`, "Low"), sharing a Q of 0.5-2.0 (`params[3]`, "Reso"). Mix is
external at `params[0]`. Defaults are full-range and flat (High 0, Low 1,
Reso 0) so the pedal is transparent until moved. Coefficients are computed per
sample from parameters interpolated within each block (no block-boundary
steps). The biquads use the canonical TDF-II v-state form - an earlier
x-formulation created a near-DC pole for the high-pass (enormous low-frequency
gain) that produced the reported distortion/crushing; state is also reset on
non-finite output.

## Dynamics and Modulation

### VCA Compressor

`ENVELOPE_VCA_COMPRESSOR` uses a channel-linked peak envelope, attack/release
one-pole smoothing, soft-knee 4:1 gain reduction, and makeup gain. Attack
(`params[0]`, 0.5-50 ms), Threshold (`params[2]`, -45 to -5 dB), and Level
(`params[3]`, 0.5-2.0x makeup) are user-controlled; Release is fixed at 120 ms.
The envelope is floored before log conversion, attack/release denominators are
bounded, and gain is clamped non-negative.

### Sidechain

`SIDECHAIN` (slot 13, formerly Rhythm Gate) is a volume-envelope shaper with a
shared phase across channels. Rate (`params[0]`) snaps to five beat divisions:
1/6, 1/4, 1/3, 1/2, and 1 beat (low knob = 1 beat, high knob = 1/6 beat),
converted to cycle length from the transport
BPM (default 120 until the host supplies transport). Shape morphs between
sine/triangle tremolo, an exponential pump, and a variable-duty square gate -
this is the attack/decay character control. Depth controls the gain range.
Mix is external at `params[3]`; the effect does not read that parameter.
A sample-rate-aware 1 ms smoother reduces hard-gate clicks. Phase is preserved
when the cycle length changes and gain is clamped to [0, 1].

### Resampler

`BITCRUSHER` (display name Bitcrusher) applies a two-pole pre-sample-and-hold filter, sample-rate
reduction, bit quantization with TPDF dither, and DC blocking. Rate (`params[1]`)
maps exponentially from the host rate toward 500 Hz, Bits (`params[2]`) selects
2-16 bits, and Filter (`params[3]`) sets the anti-alias biquad cutoff. Dither
is always enabled, scaled by the signal envelope so silent input stays silent.
The anti-alias filter is deliberately modest rather than a steep decimator
filter; the sample-rate reduction retains intentional lo-fi aliasing.

### Tremolo

`TREMOLO` uses per-channel phase accumulators and a 90-degree stereo offset.
Rate (`params[1]`, 0.1-20 Hz), Depth (`params[2]`), and Shape (`params[3]`)
are available as sine, triangle, and smoothed-square waveforms. Depth zero is a
bypass of the gain modulation.

### Flanger

`FLANGER` uses per-channel interpolated comb delay lines with a sinusoidal LFO,
180-degree stereo offset, and `tanh`-bounded feedback. Rate (`params[1]`,
0.1-5 Hz), Depth (`params[2]`), and Feed (`params[3]`) sweep the delay roughly
0.5-10 ms inside a 15 ms prepared buffer. Through-zero flanging is not
implemented.

## Bypass

`BYPASS` performs no effect processing. The compiler excludes it from the active
chain, while input gain, pedal gain, wet/dry routing, and output soft clipping
remain part of the surrounding processor path.

## Related Documentation

- [`architecture.md`](./architecture.md) - processing and lifecycle details
- [`ui-controls.md`](./ui-controls.md) - user-facing control behavior
- [`effects-audit-2026-08-29.md`](./audits/effects-audit-2026-08-29.md) - dated per-effect analysis and edge-case findings
