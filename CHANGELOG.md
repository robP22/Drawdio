# Changelog

## Unreleased

- **Rhythm Gate replaces the former sidechain pump** — morphs between tremolo, pump, and hard gate shapes; uses a shared stereo phase, click-free smoothing, and the fourth knob as the external mix control.

## v0.2.0 — Hip-Hop FX Expansion + Stability Audit

### Added

- **Resampler / Bitcrusher** — sample-rate reduction, bit quantization, dither, anti-alias filtering, and DC blocking.
- **Tremolo** — sine, triangle, and smoothed-square amplitude modulation with stereo phase offset.
- **Flanger** — short interpolated comb delay with bounded feedback and stereo LFO offset.
- **Analog Octaver** — flip-flop sub-octave, rectified upper octave, and tone filtering.
- **Per-pedal Drift / Unstable modulation API** — dual-rate random-walk modulation for non-mix parameters.
- **Image import** — maps imported image pixels to the 12-color Drawdio canvas palette.

### Changed

- Chain processing now runs per block through `DspEffect::processBlock`, with block-sized dry/crossfade buffers.
- Effect instances are prebuilt and prepared on the UI thread before config publication; the audio thread only swaps atomic config pointers.
- Delay, tape, chorus, granular, reverse, glitch, flanger, random, and octave processing keep per-channel state where stereo behavior depends on channel history.
- Cables render above pedals in a single charcoal color.
- Parameter storage uses `KnobsPerPedal` and `TotalKnobs` constants across the state and automation path.
- Preset serialization is `DRD` version `0x05`, adding automation bar count, automation section start, and manual-routing mode.

### Fixed

- Canvas-only recompiles are consumed on the UI timer path instead of waiting behind the UI notification gate.
- The transparent canvas state now remains visually empty; drawn black and transparent cells serialize distinctly.
- State loading accepts the v0.2 effect IDs through Analog Octaver.
- Automation linking skips each effect's mix knob to avoid zippering dry/wet transitions.
- Output gain, per-pedal gain, and effect outputs pass through a unity soft-knee limiter.
- Delay-line reads use proper wrap and interpolation where needed, avoiding zero-delay gain jumps and zipper noise.
- Compressor, resonator, convolution, granular, reverse, glitch, random modulation, and modulation-rate paths include the current stability and click-reduction fixes.

## v0.1.0 — Initial Release

- 23 DSP modules: 22 effects plus BYPASS.
- 6-slot 2×3 pedalboard with automatic canvas routing and manual cable routing.
- 256×256 drawable canvas with 12 colors plus transparent cells.
- Background canvas compilation with debounced drawing input.
- 20 ms crossfade between pedal configurations.
- DAW-synced automation envelopes with 1/2/4/8 bar lengths.
- VST3, AU, and Standalone build targets.
