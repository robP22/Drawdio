# Drawdio Audio Pipeline Audit

> Status: Historical dated audit. Current behavior is specified by
> `../effects.md` and `../architecture.md`. Findings DF-01, GRN-02, and V2-02
> were subsequently corrected or withdrawn; see the correction appendix.

Date: 2026-08-22

## Scope

This audit covers the current audio-processing implementation, all 24 creatable
effects, the configuration and real-time handoff pipeline, and the claims in:

- `docs/effects.md`
- `docs/architecture.md`
- `docs/audits/audio-pipeline-deep-analysis-2026-08-07.md`

The audit was static and report-only. No source fixes, build, or runtime audio
measurements were performed.

The review dimensions were:

- Numerical stability, NaN/Inf containment, denormal behavior, and divide-by-zero guards
- Feedback-loop stability and state decay
- Aliasing, interpolation, resampling, and modulation artifacts
- Stereo state integrity and channel bounds
- Real-time safety and reachable audio-thread allocations
- Parameter and knob-label consistency
- Reset, crossfade, tail reporting, and silent-block behavior
- Documentation accuracy

## Executive Summary

- No P0 crash, race, or unconditional audio-dropout issue was found in the current implementation.
- All reviewed process paths were allocation-free, with effect construction and buffer allocation staged outside the audio thread.
- Four P1 issues can produce audible defects:
  - Granular buffer-base arithmetic can underflow and displace grains.
  - Reverse-buffer repeats can hard-splice to a stale crossfade sample.
  - Spectral-freeze loop crossfades can read the wrong buffer region.
  - The shared reverb network collapses its output to mono.
- The previous AP-01 compiler-slot race is fixed. AP-04 and AP-05 are also fixed.
- AP-03, AP-06, AP-07, AP-08, and AP-11 remain partial or have residual issues.
- Documentation has significant drift: it describes removed Tape Stop Echo and Random Modulator effects, omits Re-Time, describes a nonexistent reverb topology, and contains stale FFT, tail, dither, interpolation, and optimization claims.

## Effect Inventory

The factory currently creates these effects:

| Module | Implementation |
|---|---|
| `WAVESHAPER_DISTORTION` | `WaveshaperEffect` |
| `MICROPITCH_CHORUS` | `MicroPitchChorusEffect` |
| `MULTI_MODE_FILTER` | `MultiModeFilterEffect` |
| `PITCH_SHIFTER_GRANULAR` | `GranularPitchEffect` |
| `ENVELOPE_VCA_COMPRESSOR` | `VcaCompressorEffect` |
| `GLITCH_STUTTER` | `GlitchStutterEffect` |
| `DIFFUSED_DELAY_NETWORK` | `DiffusedReverbEffect` |
| `MATHEMATICAL_WAVEFOLDER` | `WavefolderEffect` |
| `FORMANT_VOCAL_SHIFTER` | `DynamicResonantFilter` |
| `RETIME` | `ReTimeEffect` |
| `SIMPLE_DELAY` | `SimpleDelayEffect` |
| `PLATE_REVERB` | `PlateReverbEffect` |
| `RHYTHM_GATE` | `RhythmGateEffect` |
| `GRANULAR_DELAY` | `GranularDelayEffect` |
| `COMB_RESONATOR` | `CombResonatorEffect` |
| `SPECTRAL_FREEZE` | `TimeDomainFreezeEffect` |
| `FREQ_SHIFTER` | `FrequencyShifterEffect` |
| `REVERSE_BUFFER` | `ReverseBufferEffect` |
| `GRAIN_SCRUBBER` | `GrainScrubberEffect` |
| `SPECTRAL_FILTER` | `SpectralFilterEffect` |
| `CONVOLUTION_SPACE` | `ConvolutionSpaceEffect` |
| `RESAMPLE_BITCRUSH` | `ResamplerEffect` |
| `TREMOLO` | `TremoloEffect` |
| `FLANGER` | `FlangerEffect` |

Enum slots 22 and 26 are reserved for removed Random Modulator and Octaver
effects. The factory returns `nullptr` for those slots.

## Previous Analysis Re-Verdicts

| Finding | Status | Current evidence |
|---|---|---|
| AP-01 compiler result ownership race | FIXED | `CompilerThread.cpp:62,96-98` uses atomic raw-pointer exchange; producer-side replacement is deleted off the audio thread |
| AP-02 silence gate drops valid audio | GONE BY DESIGN | `PluginProcessor.cpp:50-98` always invokes audio processing; `fastPeak` is metering-only |
| AP-03 buffered tails truncated | PARTIAL | `UnifiedPedalProcessor.cpp:175-177` still skips fully dry effects without `hasActiveTail()`; multiple buffered effects lack overrides |
| AP-04 reset after crossfade | FIXED | `UnifiedPedalProcessor.cpp:289-304` swaps configs without resetting the new current payload |
| AP-05 granular phase/spray | FIXED | `GranularProcessor.h:68,73,81-82` resets the read pointer, uses bipolar spray, and starts grain two near half-grain offset |
| AP-06 ReleaseQueue overflow leak | PARTIAL | `ReleaseQueue.h:21-23` still drops and leaks an overflow pointer; count is surfaced diagnostically |
| AP-07 smoothing alpha and multiplicity | PARTIAL | Automation smoothing uses actual block size; 40 Hz parameter smoothing still uses prepared maximum block size, and crossfade dual passes advance transient state twice |
| AP-08 convolution work on audio thread | LARGELY RESOLVED | Damp spectra are precomputed in `prepare()`; residual brute-force fallback remains for blocks over 1024 samples |
| AP-09 reverb timing units | FIXED IN CODE | `fdnTimes` and sample-rate scaling are implemented; documentation still describes stale units and topology |
| AP-10 UI state leads audio publication | FIXED | Deferred payloads return before UI synchronization in `ConfigManager.cpp:184-188` |
| AP-11 unwired labeled knobs | PARTIAL | Most labels now match wired parameters; Convolution Space and Spectral Filter retain misleading labels |

## P1 Findings

### GRN-01: Granular buffer-base underflow

**Effects:** Granular Delay, Granular Pitch, Grain Scrubber

**Location:** `Source/Dsp/GranularProcessor.h:78`

The expression calculating `grainBase` performs subtraction in `size_t`. When
`off` exceeds `writePtr + bufSize - grainLen`, the subtraction underflows before
the modulo operation. The resulting base is displaced by the unsigned wrap
remainder rather than by a valid buffer distance. The issue is most likely after
effect construction, when `writePtr` is zero, and is aggravated by Grain
Scrubber's full-buffer position range.

**Impact:** Spurious grain locations, displaced echoes, and discontinuity bursts.
The deterministic random seed makes the artifact repeat after equivalent canvas
rebuilds.

**Recommended remediation:** Perform the arithmetic in signed 64-bit space or
guard the subtraction before converting back to `size_t`.

### REV-01: Reverse repeat hard splice

**Effect:** Reverse Buffer

**Location:** `Source/Effects/ReverseBufferEffect.cpp:92-97`

The repeat boundary ends at full gain on the slice start. The next sample uses a
zero crossfade weight and therefore outputs `xfadeFrom`, captured once at episode
start. This is unrelated to the current repeat's final sample and creates a butt
splice before the 32-sample ramp returns to the new slice.

**Impact:** Clicks or glitches at every repeat boundary.

**Recommended remediation:** Recapture the crossfade source at each playback reset,
or crossfade the previous pass tail directly into the next pass head.

### DF-01: Spectral-freeze old-head wrap

**Effect:** Spectral Freeze / `TimeDomainFreezeEffect`

**Location:** `Source/Effects/FilterEffects.cpp:106-108`

The old loop-head position can reach approximately `3.5 * sampleRate` while the
buffer is approximately `1.5 * sampleRate`. A single subtraction does not always
bring the absolute position into range. The shared interpolation helper then
wraps the invalid position into an unrelated buffer region.

**Impact:** Intermittent, potentially high-level ticks during freeze-loop
crossfades.

**Recommended remediation:** Apply explicit modulo wrapping to the old head before
interpolation, or keep all positions relative to the valid freeze region.

### RVN-01: Reverb output collapses to mono

**Effects:** Diffused Reverb, Plate Reverb

**Location:** `Source/Dsp/ReverbNetwork.cpp`

The network uses independent internal states but emits a mono network result rather
than preserving independent stereo output. This conflicts with the architecture
claim that no mono summing occurs and with the project's stereo-preservation
decision for delay-based effects.

**Impact:** Stereo input and stereo processing lose width through both reverb
variants.

**Recommended remediation:** Preserve independent left/right network outputs or
introduce an explicit, documented stereo decorrelation/output mix stage.

## P2 Findings

| ID | Effect/component | Location | Finding | Recommended remediation |
|---|---|---|---|---|
| MIC-01 | MicroPitch Chorus | `DelayEffects.cpp:59-69` | Free-running taps start at zero delay and eventually sweep through a comb-like zero-delay region before reaching the full buffer. | Anchor taps behind the write head and constrain modulation to a chorus-delay window. |
| GRN-02 | Granular engine | `GranularProcessor.h:64,117-130` | At playback speeds below 1.0, grain one reaches the end of its fixed window early and outputs a silent tail while grain two continues. | Stretch window traversal with playback speed or restart at the speed-adjusted grain duration. |
| GRN-03 | Granular engine | `GranularProcessor.h:22` | Every channel and instance starts with the same RNG state, correlating stereo grain placement. | Seed each channel and instance independently. |
| DF-02 | Multi Mode Filter | `FilterEffects.cpp:224-247` | No finite-input/state recovery; a NaN bypasses the absolute-value clamp and persists. | Sanitize input and reset state on non-finite values; cap cutoff below Nyquist. |
| DF-03 | Dynamic Resonant Filter | `FilterEffects.cpp:303-314` | Biquad coefficients are recomputed every sample despite documentation claiming every-eight-sample updates. | Implement the documented sub-block update or correct the documentation. |
| DF-04 | Comb Resonator | `DistortionEffects.cpp:209` | `m_hasTail` is overwritten once per channel, so the final channel determines the result. | OR channel activity and assign the flag once after the loop. |
| DF-05 | Comb Resonator | `DistortionEffects.cpp:38` | Reported 0.8-second tail is shorter than the approximately 2.1-second audible 20 Hz ring. | Report a conservative tail or derive it from current frequency and feedback. |
| RV-02 | Reverb network | `ReverbNetwork.cpp` | Delay-line write-head wrapping can create a boundary tick in interpolated reads. | Use a boundary-safe read layout or explicit cross-boundary interpolation. |
| PL-01 | Metering | `PluginProcessor.cpp:70-79` | `fastPeak` inspects only the first four samples, under-reporting blocks with quiet starts. | Compute a full-block peak or maintain a running peak latch. |
| PL-02 | Parameter smoothing | `UnifiedPedalProcessor.cpp:31,122` | The 40 Hz smoother uses `maxSamplesPerBlock`, not the actual block length. | Derive alpha from the current block duration. |
| PL-03 | Canvas queue | `CanvasMessageQueue.cpp:15-16` | A full queue drops the newest snapshot, contrary to the documented last-write-wins behavior. | Overwrite the oldest entry or requeue the latest snapshot after debounce. |
| PL-05 | Silent-block skip | `UnifiedPedalProcessor.cpp:175-177` | Fully dry buffered effects without tail overrides stop updating and resume stale internal state. | Remove the skip or add a conservative processing-on-silence capability. |
| PL-06 | ReleaseQueue | `ReleaseQueue.h:21-23` | Overflow replacement leaks the displaced payload. | Retain a last-resort pending payload or use a larger bounded queue. |
| V2-01 | Resampler | `ResamplerEffect.cpp:63-77` | A single two-pole anti-alias filter has only a 12 dB/octave slope and weak attenuation near the reduced Nyquist frequency. | Cascade another section or document the filter as alias reduction rather than strong suppression. |
| V2-02 | Resampler | `ResamplerEffect.cpp:107-112` | One dither draw is held for the entire sample-and-hold interval, correlating dither with the reduction rate. | Generate independent dither at each quantization event. |
| V2-03 | Resampler | `ResamplerEffect.cpp:88` | Pole `0.9999` produces a corner near 0.7 Hz, not the documented 5 Hz. | Use a sample-rate-derived 5 Hz coefficient or correct the documentation. |
| V2-04 | Resampler | `ResamplerEffect.cpp:65` | Recompute hysteresis is keyed on cutoff, not rate, and is small near the low-frequency floor. | Smooth coefficients or use an absolute hysteresis floor. |
| V2-08 | Tremolo | `UnifiedPedalProcessor.cpp:113-115` | Drift modulation can cross the discrete Shape zones and abruptly change waveform family. | Exempt discrete Shape parameters from drift or add hysteresis. |

## P3 Findings

| ID | Component | Finding |
|---|---|---|
| DF-06 | Waveshaper | Drive bypass freezes the ADAA previous sample, creating a possible one-sample re-engagement transient. |
| DF-07 | Multi Mode Filter | The unused sample fallback diverges from the block path and reads the wrong parameter index. |
| DF-08 | Multi Mode Filter | Cutoff glide starts from stale state after reset or fully dry skip. |
| DF-09 | Spectral Filter | Block-rate coefficient changes can produce small zipper artifacts during fast modulation. |
| DF-10 | Distortion/filter fallbacks | Several dead `processSample` paths encode parameter mappings different from the block path. |
| GLI-01 | Glitch Stutter | Exit fade includes a dry-term hump rather than a monotonic dry restore. |
| GLI-02 | Glitch Stutter | Crossfade length is duplicated in the header and implementation. |
| DSP-01 | Shared delay read | `interpolateDelayRead` has undefined behavior for negative or non-finite positions if callers violate its implicit contract. |
| RET-01 | Re-Time/granular fallback | Dead sample fallbacks interpret arguments as block lengths or wrong parameter indices. |
| FSH-01 | Frequency Shifter | Six-allpass Hilbert approximation has finite design bandwidth; out-of-band image leakage is undocumented. |
| TAIL-01 | Buffered effects | Glitch, Reverse, and the granular family lack tail metadata; MicroPitch and Flanger also need explicit tail decisions. |
| V2-05 | Resampler | Drift can repeatedly cross filter hysteresis boundaries and step coefficients. |
| V2-07 | Tremolo | Fast bypass freezes square-wave smoothing state. |
| V2-09 | Tremolo | Float phase precision is adequate currently but inconsistent with the resampler's long-run double accumulator. |
| V2-10 | Flanger | Buffer capacity has little margin above the current maximum delay. |
| V2-11 | Flanger | Feedback tail metadata remains at the base default. |
| V2-12 | Flanger | Through-zero flanging is not implemented or documented. |
| PL-07 | Crossfade | Dual processing advances smoothing, drift, and related transient state twice per block. |
| PL-08 | Crossfade | `m_prevMix` survives config swaps and can seed a new effect with the previous effect's mix value. |
| PL-10 | Host tail hints | `silenceInProducesSilenceOut()` is true for delay/reverb chains, allowing hosts to skip blocks and truncate tails. |
| PL-11 | ReleaseQueue | UI-side delete-on-full is safe today but is an audio-thread hazard if call ownership changes. |
| PL-12 | Compiler overrides | Manual parameter overrides keyed by chain position can migrate to another physical pedal after rerouting. |
| PL-13 | Parameter snapshots | Relaxed snapshot copying can produce a mixed-revision saved-state snapshot. |
| PL-14 | Re-preparation | Live effect-buffer reallocation relies on JUCE's no-callback-during-prepare contract. |
| PL-15 | Config lifecycle | An edge-case direct-publish path ignores the `prebuildEffects` deferred flag. |

## Per-Effect Matrix

| Effect family | Stability | Aliasing/CPU | Stereo | RT-safe | Parameters | Tail/reset |
|---|---|---|---|---|---|---|
| Waveshaper | OK | ADAA correct; first-order residual aliasing | OK | OK | Match | Minor bypass-state issue |
| Wavefolder | OK | ADAA correct; high fold remains alias-prone | OK | OK | Match | OK |
| Comb Resonator | OK | Fractional reads correct | OK | OK | Match | WARN: flag and duration |
| Multi Mode Filter | WARN | Linear; Nyquist and NaN edge cases | OK | OK | Match | WARN: stale cutoff state |
| Dynamic Resonant | OK | Linear; coefficient CPU higher than documented | OK | OK | Match | OK |
| Spectral Freeze | WARN | Loop-head wrap can tick | OK | OK | Match | OK |
| Spectral Filter | OK | Linear; block coefficient steps | OK | OK | Match | OK |
| Simple Delay | OK | Cubic interpolation, bounded feedback | OK | OK | Match | OK |
| MicroPitch Chorus | OK | Free-running chorus taps can sweep through zero delay | OK | OK | Match | WARN: no explicit tail |
| Tape/Re-Time | OK | Interpolated, bounded, sample-rate-aware | OK | OK | Match | OK |
| Granular Delay | FAIL | Grain-base underflow; slow-speed window mismatch | WARN: identical seeds | OK | Match | WARN: no tail override |
| Granular Pitch | FAIL | Same granular underflow | WARN: identical seeds | OK | Match | WARN: host tail |
| Grain Scrubber | FAIL | Underflow most likely across full position range | WARN: identical seeds | OK | Match | WARN: host tail |
| Frequency Shifter | OK | Hilbert approximation has finite band | OK | OK | Match | OK |
| Glitch Stutter | OK | Repeat fade shape issue | OK | OK | Match | WARN: no tail override |
| Reverse Buffer | OK | Repeat splice issue | OK | OK | Match | WARN: no tail override |
| Diffused/Plate Reverb | OK | Feedback norm safely below one | FAIL: mono output | OK | Mostly match | OK |
| Convolution Space | OK | FFT path; fallback for n>1024 | OK | OK | WARN: labels | OK |
| VCA Compressor | OK | No nonlinear aliasing mechanism | OK | OK | Match | OK |
| Rhythm Gate | OK | Envelope math stable | OK | OK | Match | OK |
| Resampler/Bitcrush | OK | WARN: weak anti-alias slope | OK | OK | Match | OK |
| Tremolo | OK | Low CPU; square smoothing | OK | OK | Match | OK |
| Flanger | OK | Interpolated comb; no through-zero mode | OK | OK | Match | WARN: tail metadata |

## Pipeline Matrix

| Component | Ordering | Audio alloc-free | Crossfade | Parameters | Notes |
|---|---|---|---|---|---|
| `UnifiedPedalProcessor` | OK | OK | WARN | WARN | PL-02, PL-05, PL-07, PL-08 |
| `PluginProcessor` | OK | OK | n/a | OK | PL-01 and PL-10 |
| `ConfigManager` | OK | n/a | OK | OK | AP-10 fixed; PL-14/PL-15 residuals |
| `ReleaseQueue` | OK | WARN | n/a | n/a | AP-06 overflow ownership leak |
| `ParameterCache` | OK | OK | n/a | WARN | PL-13 possible mixed-age snapshot |
| `CompilerThread` | OK | n/a | n/a | n/a | AP-01 fixed; latest replacement is intentionally dropped |
| `CanvasMessageQueue` | OK | n/a | n/a | WARN | Explicit fences correct; newest message is dropped on full queue |
| `CompilerEngine` | OK | n/a | n/a | WARN | Stable sort correct; overrides remain position-keyed |
| `CrossfadeState` | OK | OK | OK | n/a | Equal-power math and variable block bounds verified |

## Verified Design Claims

- First-order ADAA uses the correct antiderivative-difference form and midpoint derivative fallback.
- The arctangent and sine-fold antiderivatives match their documented equations.
- Comb feedback paths are bounded by `tanh`; the Comb Resonator's nominal loop gain is below one.
- Delay reads use fractional interpolation with non-finite-result containment.
- TPT/SVF cutoff prewarping with `tan(pi * fc / sampleRate)` is standard practice.
- TDF-II filter states reset on non-finite output in the dynamic and spectral filters.
- Rhythm Gate formulas, shared phase, cycle guard, and mix index match the documentation.
- TPDF amplitude scaling is canonical: two independent uniform draws produce a triangular distribution with 2 LSB peak-to-peak range.
- Reverb feedback matrix analysis is stable: the N=8 Householder matrix has eigenvalues `-1` and `+1` repeated seven times; the applied decay factor keeps the spectral radius below one.
- Equal-power crossfade gains satisfy `gOld^2 + gNew^2 = 1`.
- `softClip` is continuous with matching first derivative at its knee.
- Effect buffers and FFT objects are allocated during preparation, not in process paths.
- Removed enum types 22 and 26 are migrated to BYPASS by state deserialization.

## Web Cross-Reference

### ADAA

Parker, Zavalishin, and Le Bivic's DAFx-16 paper and Bilbao et al.'s later
antiderivative work define first-order ADAA as the difference quotient of the
first antiderivative, with a tolerance fallback to the nonlinear function at the
midpoint. Drawdio's implementation follows this form. The literature also
confirms that first-order ADAA reduces, but does not eliminate, aliasing; higher
order ADAA or modest oversampling is the standard upgrade for aggressive drive.

References:

- https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf
- https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html

### State-variable filters

TPT/SVF references use the prewarped coefficient `tan(pi * fc / fs)` and warn
that high-frequency operation and high resonance require careful stability and
output handling. The current R clamps and state limits are useful, but the
Multi Mode Filter still needs explicit non-finite recovery and a cutoff limit
below Nyquist for low sample-rate hosts.

References:

- https://kokkinizita.linuxaudio.org/papers/digsvfilt.pdf
- https://arxiv.org/pdf/2111.05592

### Hilbert frequency shifting

IIR Hilbert transformers built from parallel allpass branches have a finite
quadrature-accuracy band determined by their coefficient design. Six-section
designs are common and efficient, but phase error and image leakage increase
outside the intended band. The Frequency Shifter's behavior is therefore
expected, but its operating band should be documented.

References:

- https://www.csounds.com/docs/manual/hilbert.html
- https://www.cct.lsu.edu/~eberdahl/Papers/Harris_Berdahl_Abel_IIRHilbertDesignTechnique.pdf

### Dither and resampling

Quantization references confirm that two independent rectangular distributions
produce canonical TPDF dither with a 2 LSB peak-to-peak range. Decimation
references require a lowpass filter before rate reduction; Drawdio's filter
placement is correct, although a single two-pole section gives limited rejection.
Commercial lo-fi processors commonly provide both pre- and post-filters.

References:

- https://www.mwrf.com/technologies/components/article/21846556/reducing-quantization-noise
- https://www.mathworks.com/help/dsp/ug/design-of-decimatorsinterpolators.html
- https://native-instruments.com/ni-tech-manuals/crush-pack-manual/en/bite

### Feedback delay networks

FDN stability is guaranteed when the feedback matrix contracts a vector norm.
Orthogonal matrices are lossless prototypes; scaling them below unity provides
decay. The current N=8 Householder form and decay scaling satisfy this condition.
That mathematical result does not address the separate stereo-output defect in
the implementation.

References:

- https://ccrma.stanford.edu/~jos/pasp/Choice_Lossless_Feedback_Matrix.html
- https://dsprelated.com/freebooks/pasp/Feedback_Delay_Networks_FDN.html
- https://ccrma.stanford.edu/~jos/book2000/Householder_Feedback_Matrix.html

## Documentation Errata

1. `docs/effects.md:90-101` documents removed Tape Stop Echo. Slot 10 is Re-Time.
2. `docs/effects.md` has no Re-Time section.
3. `docs/effects.md:324-336` documents removed Random Modulator DSP.
4. `docs/effects.md:240-250` describes a reverb topology not present in `ReverbNetwork.cpp`.
5. Reverb timing values are presented as milliseconds but are scaled sample-count values.
6. `docs/effects.md:264` and `docs/architecture.md:334` state a 512-tap/1024-point convolution design. Current code uses `kFftOrder=11`, `kFftSize=2048`, and a 1024-tap cap.
7. `docs/effects.md:144-146` and `docs/architecture.md:340` claim Dynamic Resonant coefficients update every eight samples. Current code updates every sample.
8. `docs/architecture.md:335` calls the canvas queue behavior last-write-wins. A full queue currently drops the newest message.
9. `docs/architecture.md:330` claims no mono summing; ReverbNetwork currently has a mono output path.
10. `docs/effects.md:65` calls Simple Delay interpolation linear; the shared path uses four-point cubic interpolation for normal buffer sizes.
11. `docs/effects.md:107-109` describes one-sided granular spray; current code uses bipolar spray.
12. `docs/effects.md:205` calls Glitch Stutter fades equal-power Hann fades; current fades are complementary sinusoidal equal-gain fades.
13. `docs/architecture.md:332` claims exact zero amplitude modulation from dual-grain windows; spray jitter makes the sum only approximately unity.
14. `docs/effects.md:362-363` calls the Resampler DC blocker 5 Hz; the current fixed pole is near 0.7 Hz at 44.1 kHz.
15. `docs/effects.md:369-370` says filter recomputation follows rate changes; the implementation keys it to cutoff changes.
16. `docs/architecture.md:381-382` describes a fixed 5-second tail; current reporting is the maximum active effect tail, with 5 seconds only as the no-config fallback.
17. `docs/architecture.md:104-111` omits the current ReleaseQueue overflow slot and its leak behavior.
18. `docs/architecture.md:203-205` describes a CAS-failure path that is currently prevented by config deferral and is defensive only.
19. `docs/effects.md:388-401` does not document the Flanger's absent through-zero mode or default tail metadata.
20. The project working summary in `AGENTS.md` still lists Random Modulator and Analog Octaver as active v0.2 effects, although current enum slots and factory behavior mark both removed.

## Recommended Fix Order

1. Fix `GRN-01` unsigned grain-base arithmetic.
2. Fix `RVN-01` stereo reverb output.
3. Fix `REV-01` and `DF-01` repeat/loop crossfade boundaries.
4. Remove or correct fully-dry skipping for stateful buffered effects; add tail metadata where appropriate.
5. Add Multi Mode Filter non-finite recovery and a below-Nyquist cutoff cap.
6. Resolve ReleaseQueue overflow ownership without leaking payloads.
7. Make parameter smoothing depend on actual block size and avoid advancing transient state twice during crossfade.
8. Replace or partition the convolution fallback for large host blocks.
9. Seed granular channels independently and correct slow-speed window traversal.
10. Correct the documentation, beginning with removed effects, Re-Time, reverb topology, and FFT/tail claims.

## Verification Limitations

This report is based on source inspection and literature cross-reference. It does
not include:

- A compiler or linker run
- Host-specific callback scheduling tests
- Rendered impulse, sweep, or null-test measurements
- CPU profiling at large host block sizes
- Listening tests for the reported clicks, aliasing, or stereo collapse

No project source files were changed as part of this audit.

## Subsequent Corrections

- **DF-01 withdrawn:** later source review did not reproduce the claimed
  approximately `3.5 * sampleRate` freeze position from the current control flow.
- **GRN-02 corrected:** the reported silent-window behavior was assigned to the
  wrong playback-speed direction; `readPtr` advances by `playbackSpeed`, so
  speeds below 1.0 reach a fixed window later, not earlier.
- **V2-02 withdrawn:** the current implementation generates dither inside the
  per-sample loop rather than holding one dither value for a sample-and-hold
  interval.
