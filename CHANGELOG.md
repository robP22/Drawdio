# Changelog

## Unreleased

- Documentation consolidation and correction for the current v0.2.0 source tree.
- CMake and JUCE plugin metadata now report version `0.2.0` and company `robP`.
- Re-Time is the active replacement for the former Tape Stop Echo slot.
- Random Modulator and Analog Octaver are retained only as reserved legacy IDs
  that migrate to Bypass.

## v0.2.0 - Hip-Hop FX Expansion and Stability Audit

### Added

- Resampler / Bitcrusher with sample-rate reduction, bit quantization, dither,
  anti-alias filtering, and DC blocking.
- Tremolo with sine, triangle, and smoothed-square amplitude modulation.
- Flanger with a short interpolated comb delay and bounded feedback.
- Per-pedal Drift / Unstable modulation for non-mix parameters.
- Image import into the 12-color Drawdio canvas palette.

### Changed

- Chain processing runs per block through `DspEffect::processBlock`.
- Effect instances are prepared before configuration publication and handed off
  through atomic configuration pointers.
- Delay, chorus, granular, reverse, glitch, and modulation state is maintained
  per channel where stereo behavior depends on channel history.
- Cables render above pedals in a single charcoal color.
- Parameter storage uses `KnobsPerPedal` and `TotalKnobs` constants.
- Preset serialization uses `DRD` version `0x05`.
- The former Tape Stop Echo slot is now Re-Time with transport-synchronized
  variable-speed loop processing.

### Fixed

- Canvas-only recompiles are consumed on the UI timer path.
- Transparent cells remain visually empty while drawn black cells remain distinct
  in DSP analysis and serialization.
- Automation linking skips the external mix knob.
- Output, per-pedal gain, and effect outputs pass through the unity soft clipper.
- Delay-line wrapping and interpolation avoid zero-delay gain jumps and zippering.
- Compressor, resonator, convolution, granular, reverse, glitch, and modulation
  paths include the current stability and click-reduction safeguards.

## v0.1.0 - Initial Release

- 22 effects plus Bypass in the original release catalog.
- 6-slot 2x3 pedalboard with automatic and manual cable routing.
- 256x256 drawable canvas with 12 colors plus transparent cells.
- Background canvas compilation with debounced drawing input.
- 20 ms crossfade between pedal configurations.
- DAW-synchronized automation envelopes with 1/2/4/8 bar lengths.
- VST3, AU, and Standalone build targets.
