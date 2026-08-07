# Changelog

## Unreleased

- **Rhythm Gate replaces Sidechain Pump** — 3-zone morphable volume envelope (tremolo → pump → hard gate), click-free 1 ms smoothing, single-phase stereo sync, all 4 knobs wired (Rate 0.05–10 s, Shape, Depth, Mix).

## v0.2.0 — Hip-Hop FX Expansion + Stability Audit

### New Effects (4)
- **Resampler / Bitcrusher** — sample-rate reduction (44.1 kHz → 500 Hz) with linear-interpolated S&H capture, 16→2-bit quantization, triangular dither, 2-pole anti-alias filter, DC blocker.
- **Tremolo** — 0.1–20 Hz amplitude modulation, sine/triangle/smoothed-square shapes, 90° stereo offset.
- **Flanger** — 0.5–10 ms LFO-modulated comb with tanh-bounded feedback, 180° stereo LFO.
- **Analog Octaver** — zero-crossing flip-flop sub-octave + bandpass-rectified upper octave with DC block and tone LP.

### New System Features
- **Per-Pedal DRIFT/UNSTABLE modulation** — dual-rate random-walk parameter wobble (±2% slow drift + ±0.8% fast chaos) applied to non-mix parameters in `processChainBlock`. API: `setDriftAmount`/`getDriftAmount` per slot.
- **Cables render above pedals** (`paintOverChildren`) with a single solid charcoal color (two-tone in/out removed).
- **`KnobsPerPedal`/`TotalKnobs` constants** replace magic `4`s across the parameter pipeline; `static_assert` guards the 32-bit cache mask.

### Sound Quality Fixes
- Removed unconditional LFO modulation from SimpleDelay, TapeStopEcho, SpectralFreeze, SpectralFilter (inaudible wobble artifacts).
- Comb resonator fractional interpolated reads (no zipper noise on sweeps).
- GlitchStutter + ReverseBuffer entry crossfades blend from captured dry samples (no clicks/dips).
- RandomModulator modulates around unity gain (was silent at depth 0).
- VCA compressor makeup gain can now boost above unity.
- ConvolutionSpace IR tail extended 256 → 512 taps.
- Sidechain ducker phase preserved on rate changes.
- MicroPitchChorus modulation depth restored (stray scalar removed).
- StateSerializer accepts the 4 new effect types (was clamping to BYPASS).
- Mix knob excluded from automation linking (no zipper).

## v0.1.0 — Initial Release

### Features
- 27 DSP modules (26 effects + BYPASS)
- 6-slot 2×3 pedalboard with drag-and-drop manual routing
- 256×256 drawable canvas with 12 colors + transparent sentinel
- Real-time canvas-to-audio compilation (background thread)
- 20 ms equal-gain crossfade between preset changes
- DAW-synced automation envelopes (1/2/4/8 bar)
- VST3, AU, Standalone builds

### Sound Quality
- Unity-gain soft-knee output limiter
- ADAA antiderivative waveshaper/wavefolder
- Per-channel stereo preservation through all delay effects
- Correct VCA compressor gain curve (dB-to-linear via exp2f)

### Performance
- Block-level processing (6 processBlock calls per cycle)
- Zero heap allocations on audio thread
- Lock-free config handoff via atomic pointer exchange
- Deferred deletion via ReleaseQueue
- SPSC lock-free canvas message queue
