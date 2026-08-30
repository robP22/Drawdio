# Drawdio Project Custom Rules (AI Agent Configuration)

## PREFIX CACHING & TOKEN OPTIMIZATION
- **Context Maintenance:** Assume the project blueprint and context prefixes are cached. Do not re-verify, re-explain, or summarize the system architecture.
- **Incremental Diff Execution:** Provide only the targeted code edits or specific functional additions requested. Never rewrite unchanged surrounding functions or boilerplate classes.
- **Zero Recapitulation:** Do not restate user goals or create "Plan of Action" lists before writing code. Jump straight into the syntax implementation.

## CRITICAL: TOKEN WASTE GUARDRAILS & FORMATTING CONSTRAINTS
- **Raw Artifact Output Only:** Return only the requested code block modifications or structural artifacts. Do not append explanatory text, summaries of logic changes, code walkthroughs, bullet points, or implementation commentary.
- **Strict No-Documentation Rule:** Do not generate any code documentation, inline comments, docstrings, Doxygen blocks, or file headers. Code blocks must consist exclusively of clean, operational syntax without text annotations.
- **Zero Conversational Filler:** Omit all introductory pleasantries, meta-commentary, system updates, success confirmations, greeting lines, or transition words. Begin the response immediately with the raw technical payload to optimize server-side prefix caching.

## Codebase Architecture Blueprint
- **Product:** Drawdio - Visual Audio Effects Pedalboard (26 DSP modules + BYPASS).
- **Core Pipelines:**
  - `PixelCanvasComponent` -> `CanvasMessageQueue` (SPSC lock-free ring buffer).
  - `CompilerThread` invokes `compileCanvas()` to process pixel data without blocking.
  - `UnifiedPedalProcessor` handles sample-accurate execution on the Audio Thread via a 20ms crossfade window (~882 samples at 44.1kHz).

## Thread Safety & Engineering Directives

### 1. Real-Time Audio Constraints (`UnifiedPedalProcessor`, `PedalAssetPayload`)
- **Zero Allocations & Deallocations:** Never inject heap allocation operators (`new`, `malloc`, `std::vector::resize`) or hidden deallocations inside any code branch reachable by `processBlock`.
- **Shared Pointer Destruction Hazard:** Modifying or assigning `std::shared_ptr` references within the audio execution thread is strictly prohibited. Because reference-drop triggers invoke implicit heap `delete` commands, all background updates must utilize raw atomic pointers (`std::atomic<T*>`) or a deferred release pool (`juce::ReleasePool`) to offload object cleanup entirely to the message or background thread loops.
- **Denormal Flush Protection:** Ensure `juce::ScopedNoDenormals` blocks sit securely at the absolute entry points of all performance-critical mathematical DSP processing functions to eliminate CPU subnormal spikes during signal tail decay.

### 2. Multi-Threaded State Hand-Offs (`CompilerThread`, `CanvasMessageQueue`)
- Keep `CanvasMessageQueue` purely single-producer single-consumer (SPSC) using explicit memory fences (`std::memory_order_release` / `std::memory_order_acquire`).

## Build & Install Rule
- **To build and install the Release VST3 into the correct location, ALWAYS run `./updater.sh`** (project root) — it configures, builds the `Drawdio_VST3` target, validates the bundle, and installs to the platform default (`~/Library/Audio/Plug-Ins/VST3` on macOS). On Windows use `updater.cmd`/`updater.ps1`. Do not hand-copy build artifacts.

<!-- ANCHORED_SUMMARY -->
## Goal
- Complete sprite canvas refactoring and address all identified architectural/audio thread safety issues in priority order.

## Constraints & Preferences
- Knobs display-only: preset depends entirely on canvas drawing, not knob adjustments.
- LED sprite sheet is 1×2 row-major grid (left=off, right=on), rendered per pedal.
- Canvas has 12 drawable colors (Black, White, Red, Green, Blue, Yellow, Brown, Purple, Grey, Pink, Orange, Violet). `PixelColor::Transparent(5)` = empty cell — never drawn, shows texture through.
- No canvas shadow, no black fill, no `canvasSurface()` fill. Sprite background + colored pixel overlay.
- Stereo preservation: delay-based effects use per-channel delay lines (confirmed user preference).
- Color wells: `colorwell.png` sprite rendered beneath each palette blob, centered, at 1.5× blob diameter.

## Progress
### Completed
- **C1 (parameter cache):** `readParam()` consults `m_parameterCache` via `m_paramCacheValidMask` bitmask before falling back to compiled config.
- **C2 (audio thread allocations):** `m_channelBuffer` pre-allocated in `prepareToPlay()`; `drainReleaseQueue()` moved from audio-thread to UI-thread path.
- **C3 (data race):** `m_crossfadeCounter` changed to `std::atomic<int>`; `reset()` deferred via `m_pendingReset` flag consumed by audio thread.
- **C4 (use-after-free):** `getStateInformation()` uses `getSnapshot()` (atomic cache copy).
- **C5 (index mismatch):** Determined not a bug — `targetDspNodeRegister` indexes `sortedChain` correctly.
- **C6 (shared effect objects):** Replaced per-type singleton with per-chain-position effect instances via `createDspEffect()` factory (now owned by `PedalAssetPayload::effects`).
- **C7 (granular discontinuity):** `readPtr` reset to 0 on grain restart.
- **C9 (unnecessary repaints):** `repaint()` gated behind `needsRepaint` flag.
- **H6 (integer division):** `static_cast<double>(xSum) / totalPainted` in auto-routing score.
- **Rendering anomaly fixes:** Removed `setRepaintsOnMouseActivity(true)`, added value-change guard in `PedalComponent::setKnobValue()`, added no-change guard in `PedalboardGrid::updateRouting()`, fixed `refreshRoutingFromConfig()` to update only on change, removed dead `m_mouseInside`, removed redundant `repaint()` in `mouseUp()`, guarded `clearCanvas()` against blank-canvas recompile.
- **H1 (stale m_knobValues):** Removed; `getKnobValues()` returns `getSnapshot().values`.
- **M7 (store reordering):** Removed `m_hasMessage` flag from `CanvasMessageQueue`; `hasMessage()` compares indices directly.
- **L5 (bezier caching):** Added `rebuildCableCache()` called from `updateRouting()`.
- **L10 (sparse stroke lookup):** Replaced 64KB `std::array` with sparse per-stroke change list (unordered_map variant later removed).
- **L2 (stable sort):** `std::stable_sort` in `CompilerEngine`.
- **LED sprite fix:** Frame rectangles computed from `ledonoff.png` 1×2 grid dimensions.
- **`gridValueToPixel(0)` → Transparent:** Fixed "black square" bug where initial grid data (all 0s) rendered as solid black, hiding the canvas texture. Now `gridValueToPixel(0)` returns `PixelColor::Transparent` instead of `Black`.
- **Color well sprites:** Added `ColorWell` to `ResourceManager::ImageId`/`TextureId`; loads `colorwell.png`. In `ColorPalette::paint()`, draws well sprite beneath each blob centered at blob center, sized at 1.5× blob diameter. Rendering order: palette background → well sprites → color blobs.
- **P0 (audio thread allocations):** Effect instances are embedded in `PedalAssetPayload` (F1 refactor). `ConfigManager::prebuildEffects()` creates+prepares effects into the payload on the UI thread (via `consumeCompiledResultIfAvailable()`); handoff to the audio thread is an atomic config-pointer exchange (`m_nextConfig`/`m_currentConfig`) with deferred deletion via `ReleaseQueue` — zero heap alloc/dealloc on the audio thread. Removed `consumeCompiledResultIfAvailable()` from `PluginProcessor::processBlock()` — config loading only happens on UI timer path.
- **P1 (per-channel stereo):** All delay-based effects now use per-channel delay lines/held values:
  - `MicroPitchChorusEffect`: `std::vector<MicropitchState> m_channels`
  - `SimpleDelayEffect`: `std::vector<SimpleDelayState> m_delays`
  - `TapeStopEchoEffect`: `std::vector<TapeStopChannel> m_channels` (struct with buf/writePtr/readHead)
  - `GranularBaseEffect`: `std::vector<GranularProcessorState> m_states`
  - Each writes/reads its own channel's delay line or held value; no cross-channel broadcast.
- **P2 (TapeStopEcho clamp):** Added `std::fmax(0.001f, ...)` lower bound on `readSpeed`; removed dead `if (readSpeed >= 0)` / `else` branch (both computed same value). Included in P1 changes.
- **P3 (Wavefolder aliasing):** Fold multiplier capped from 9.0f to 4.0f in `DistortionEffects.cpp:21`.
- **P4 (Waveshaper drive staging):** Drive gain applied before the clipper (not after), output normalized (no drive gain multiplication on output). `DistortionEffects.cpp:31-38`.
- **P5 (dead code removal):** Deleted `DspState.h/.cpp`, `DspGraph.h/.cpp`, `PedalSkinManager.h/.cpp`, `SimdUtils.h` (8 files/~500 lines), and `HeadlessDspTest.h/.cpp`. Cleaned `PedalComponent` (removed PedalSkinManager include, skin parameter, setSkin method, m_skin member). Updated `CMakeLists.txt` (removed source entries).
- **P6 (ring buffer float drift):** Replaced `if (readIdx >= bufSize) readIdx -= bufSize` with `readIdx %= bufSize` wrap in delay/ring read paths (`DelayPrimitives.h:11`, `DelayEffects.cpp:122,163,280`).
- **P7 (FormantShifter bandwidth):** Replaced fixed 10% bandwidth with `max(50Hz, centerHz/5)` Q-based bandwidth. Envelope now uses max across all channels instead of only channel 0. Replaced 1-pole filter with clean 2nd-order biquad resonator (TDF-II) for proper formant resonance (`DynamicResonantFilter`). `FilterEffects.cpp:94-106`.
- **P8 (colourForPixel singleton):** Determined already resolved — `canvasPixelColour()` is the actual method, theme is injected via `IThemeProvider&`, no `ThemeManager::getDefault()` exists in source.
- **P9 (output limiter):** Added unity-gain soft-knee `softClip()` (knee 0.85) after gain multiplication — per-pedal and master output. `UnifiedPedalProcessor.cpp:45-54, 162, 224`.
- **SimpleDelay max-delay wrap:** Fixed zero-delay + gain-boost artifact when `delaySec` equals buffer duration — clamped `delaySamples` to `bufSize - 1`. `DelayEffects.cpp:88`.
- **VcaCompressor domain guards:** Attack/release denominators floored, envelope floored at 1e-8 before log10, gain clamped ≤ 1.0. `MiscEffects.cpp:62-100`.
- **TapeStopEcho per-channel readHead:** Moved `readSpeed` calculation inside loop using each channel's own `readHead` instead of hardcoded `m_channels[0]`. `DelayEffects.cpp:165`.
- **Undo routing restore bug:** Fixed `m_pendingRoutingRestore` — flag now resets unconditionally to prevent stale `m_preClearRouting` from incorrectly restoring routing on normal stroke undos after a clear. Added `syncPedals()` to the restore path. Replaced by `m_lastRestoredManualRouting` comparison in `EditorSyncController.cpp:163-175`.
- **Canvas-only compile never consumed:** `triggerRecompile()` pushed grid data to the compiler but never called `triggerUINotification()`. Since `consumeCompiledResultIfAvailable()` was gated behind `consumeUINotification()`, the compiled result sat forever in the compiler slot. Audio thread never received the config → no output. Fixed by moving `consumeCompiledResultIfAvailable()` before the UI notification gate (`EditorSyncController.cpp:80`).
- **Granular clicking/tapping:** Single-grain design with 100% amplitude modulation at grain boundaries produced audible "tapping" at certain pitch ratios. Replaced with 50% overlapping dual-grain system — secondary grain head offset by `grainLen * (0.5f + spray*0.1f)` (spray = ±0.1 jitter) with independent Hann window. Hann windows sum to unity at 50% overlap, eliminating amplitude modulation entirely. `GranularProcessor.h:263-325`.
- **Minimize/restore rendering (FL Studio):** Added `repaint()` to `PixelCanvasComponent::resized()` (was setting `m_overlayDirty=true` but never queuing repaint). Repaint is gated behind `needsRepaint` in `EditorSyncController::tick()` — handles FL Studio window hide/show cycles that don't trigger OS expose events. `PixelCanvasComponent.cpp:32-35`, `PluginEditor.cpp:195-201`.
- **Knob labels:** Added parameter labels (Wet, Dry, Vol, Drive/Rate/etc.) rendered centered beneath each knob at `pedalH * 0.10` below knob center (~30px at design resolution). Text drawn after knobs, white 75% opacity, font `pedalH * 0.04`. `GridLayout.h:42`, `PedalComponent.cpp:92-114`.
- **Phase 5 (block processing):** Restructured `processAudioBlock` from per-sample to per-block processing. Added `processBlock` virtual to `DspEffect` with default sample-by-sample fallback. Waveshaper/Wavefolder override with tight inner loops. `processWithConfig` renamed to `processChainBlock` — processes the full sample block per node instead of one sample per call. `m_dryBuffer` and `m_crossfadeOldOut` changed from per-channel scalar to per-channel block buffers (`vector<vector<float>>`). Crossfade path processes both configs as full blocks then per-sample mixes. Reduces virtual dispatch from 18,432 calls/block (6 nodes × 3 params + 6 processSample × 512 samples) to ~42 (6× processBlock + batched preamble). `DspEffect.h:17-21`, `DistortionEffects.cpp:20-37`, `UnifiedPedalProcessor.cpp:307-354`.
- **Unconditional LFO removals:** SimpleDelay ±1.5% delay-time LFO, TapeStopEcho ±1.8-sample wow, SpectralFreeze ±5% pitch wobble, SpectralFilter ±10% center LFO — all removed (inaudible-but-costly artifacts, no user control). `DelayEffects.cpp`, `FilterEffects.cpp`, `SpectralFilterEffect.cpp`.
- **Comb resonator interpolated reads:** Integer `buf[readPtr]` → fractional `interpolateDelayRead()` — fixes zipper noise on frequency sweeps. `DistortionEffects.cpp:163,196`.
- **Glitch/Reverse crossfades:** GlitchStutter entry xfade blends from captured dry sample (was fade-from-zero click); ReverseBuffer replaced record-end fade-out + fade-in-zero with a single cosine crossfade from captured dry. `PitchEffects.cpp:166-171`, `ReverseBufferEffect.cpp:61-95`.
- **MicroPitch depth fix:** stray `* 0.001f` scalar cancelled LFO depth — removed; chorus modulation now audible. `DelayEffects.cpp:60`.
- **Sidechain ducker phase reset:** interval change used `timer = 0` (abrupt phase jump/click) — now `timer %= interval`, assigned once per block. `MiscEffects.cpp:133,165-166`.
- **VCA makeup clamp:** `gain > 1.0` clamp removed (defeated makeup gain knob) — now only `gain < 0.0` blocked. `MiscEffects.cpp:53,100`.
- **Convolution IR cap:** 256 → `kFftSize/2` (512); `kFftOrder`/`kFftSize` made public. `ConvolutionSpaceEffect.h/.cpp`.
- **RandomModulator bipolar:** output mapped `1.0 + noise·depth` (modulates around unity) — was unipolar/zero-centered which silenced output at depth 0. `RandomModulatorEffect.cpp:92,101-102`.
- **4 new effects (v0.2):** Resampler/Bitcrusher (`ResamplerEffect.h/.cpp`), Tremolo + Flanger (`ModulationEffects.h/.cpp`), Analog Octaver (`OctaverEffect.h/.cpp`). Registered in `DspModuleType.h` (23–26), `PedalDefinition.cpp` (27 entries + static_assert), `DspEffectFactory.cpp`, `PedalComponent.cpp` popup, `CMakeLists.txt`.
- **DRIFT/UNSTABLE modulators:** per-slot dual-rate random-walk in `UnifiedPedalProcessor` — slow drift (0.3–3 Hz, τ 0.5–2 s, ±2%) + fast unstable (6–15 Hz, τ 80 ms, ±0.8%), applied to non-mix params after smoothing. Atomic per-slot amounts (`setDriftAmount`/`getDriftAmount`), per-slot RNG seeds. `UnifiedPedalProcessor.h:88-107`.
- **Cable z-order + solid color:** `paint()` → `paintOverChildren()` (cables above pedals); two-tone in/out colors → single `cableColour()` (0xFF4A4A4A); shadow alpha 0.45→0.30, edge 6.2→5.8. `PedalboardGrid.h/.cpp`, `CableRenderer.h/.cpp`, `IThemeProvider.h`, `ThemeManager.h/.cpp`.
- **updater.sh path fix:** `Drawdio_artefacts/VST3/` → `Drawdio_artefacts/Release/VST3/` (Release build layout). `updater.sh:11`.
- **StateSerializer maxPedalType:** `RANDOM_MODULATOR` → `ANALOG_OCTAVER` — presets with the 4 new effects were silently clamped to BYPASS on load. `StateSerializer.cpp:120`.
- **KnobsPerPedal constant:** `constexpr KnobsPerPedal = 4`/`TotalKnobs` in `DrawdioConstants.h`; all parameter-pipeline `4`s replaced; `static_assert(TotalKnobs <= 32)` in `ParameterCache.h`.
- **Mix knob excluded from automation link:** linked automation no longer jumps the mix knob (zipper); `mi` computed before the link loop. `UnifiedPedalProcessor.cpp:109-117`.
- **Crossfade identical-config guard:** `next == current` clears `nextConfig` (protects the completion exchange from self-freeing a live config). `UnifiedPedalProcessor.cpp:183-186`.
- **Rhythm Gate rewrite:** `SidechainDuckerEffect` → `RhythmGateEffect` (`DspModuleType::SIDECHAIN_DUCKER` → `RHYTHM_GATE`, integer value 13 unchanged — serialized presets unaffected). Replaced the old back-loaded cubic recovery envelope (`1 − duck·(1−phase)³`), dead Shape/Level knobs, per-channel timers (stereo wobble), and cycle-boundary click with a Gross Beat-style volume envelope shaper: 3 morphing zones (sine→triangle blend, pump `1 − (1−phase)^exp` with exponent 1.5→7.5, hard square gate with duty 50%→80%), `gain = 1 − depth·(1−env)` clamped [0,1], click-free 1ms smoothing (`m_smoothCoeff = 1 − exp(−1/(sr·0.001))`), single shared `m_phase` (L/R synced — no per-channel state), `m_phase %= cycleSamples` mid-rate-change guard, cycle expanded to 0.05–10s, `DspEffect(3)` so the Mix knob is the processor's external wet/dry blend (effect never reads params[3]). All 4 knobs wired: Rate[0], Shape[1], Depth[2], Mix[3]. `MiscEffects.h/.cpp`, `PedalDefinition.cpp`, `DspEffectFactory.cpp`.
- **Granular engine rewrite (2026-08-29):** `GranularProcessor.h` dual-grain system replaced with two independent grain heads whose restarts are staggered by half a grain length — each head's Hann window is zero at its own restart, so restarts are click-free (old design reset `grainPhase2 = 0.5L` at every restart, jumping the second window 0→1 → clicks at 6–25 Hz). Grain-base selection now clamps the total offset (position + spray) to `[128, bufSize − grainLen·max(1, 1/speed) − 128]` with signed 64-bit math — the old `(position+spray)·bufSize` mapping let the window wrap past the write head near the top of the Position range (reads stale-cycle/future audio → crushed signal). Fixes Grain Scrubber/Granular Delay/Pitch Shifter simultaneously.
- **Chorus replaces MicroPitchChorus (2026-08-29):** `MICROPITCH_CHORUS` → `CHORUS` (integer 2 unchanged, presets migrate). New `ChorusEffect` in `ModulationEffects.{h,cpp}`: fixed 30ms center delay, LFO read modulation bounded to 10–50ms behind the write head (depth clamp `< centerDelay−1`), rate 0.05–3Hz, per-channel LFO phase. Old free-running detuned taps drifted through the write head every ~17–34s (docs: "pass through zero-delay comb-like regions") — the reported "pitch on acid" in delay effects.
- **SimpleDelay no load glide:** `m_smoothedDelaySamples` now seeds 0.55s in prepare and the first block snaps to the compiled delay time (`m_firstBlock`), removing the ~230ms delay ramp (pitch glide) on every config load/reset.
- **ReverseBuffer repeat splice:** `xfadeFrom` re-captured from the last played sample at each repeat (`ReverseBufferEffect.cpp:97`) — the old capture-once-from-entry crossfade caused a hard splice click at later repeats.
- **MultiModeFilter hardening:** cutoff capped at `0.45·sr` (block + sample paths) and non-finite state reset added.
- **SpectralFilter per-sample coefficients:** center/width/Q interpolated per sample within each block (kills block-boundary coefficient steps when knobs/automation move); prev-state members `m_prevCenter/m_prevWidth/m_prevQ`.
- **CombResonator tail:** `m_hasTail` peak now aggregated across channels (`DistortionEffects.cpp`).
- **Effect audit 2026-08-29:** `docs/audits/effects-audit-2026-08-29.md` — per-effect algorithm/edge-case analysis with research references; 5 new test cases (granular DC soak + write-head invariant, Chorus depth + 60s drift soak, SimpleDelay no-glide, ReverseBuffer splice, MultiMode extreme cutoff) — suite at 28 cases / 4.76M assertions.
- **ReverseBuffer freeze buffer + exit fade:** per-channel `freeze` buffer (allocated in `prepare`) captures the slice wrap-aware at record→play; playback reads `freeze` so the advancing record head can never overwrite the slice mid-cycle (cycles exceed the 1.5s ring at most densities). `exitFadePos`/`exitFadeFrom` blend the last played sample into the live input over 32 samples at play→record (was an instant jump → click at every cycle end). 29 test cases.
- **Knob layout staleness fix:** `PedalComponent::syncFromProcessor()` and the type popup changed `m_definition` without re-running `updateKnobBounds()` — knob bounds/visibility stayed from the previous definition (labels painted at stale/empty bounds → all labels stacked top-left, one knob centered). Extracted `applyKnobLayout()` (updateKnobBounds + knob setBounds) called from `resized()`, the popup handler, and the sync path; `m_knobBounds{}` default-init; bounds cleared on the empty-definition early return.
- **Knob schema (5-knob correction):** `knobLayoutForCount(5)` changed from die-with-center-pip to square + 5th knob below, centered at `(0.50, 0.685)` (slot index 4). Schema now: 1 = centered, 2 = centered column, 3 = triangle (third centered below a 2-row), 4 = square, 5 = square + below-centered.
- **UI audit 2026-08-29:** `docs/audits/ui-audit-2026-08-29.md` — six analyses (rendering, layout, interaction, threading, dead code, accessibility). Fixes: overlay staleness on large strokes (`PixelCanvasComponent.cpp:576-578` — 4096-pending cap now sets `m_overlayDirty` instead of dropping cells), automation display width clamp (`BottomControlBar.cpp`), palette tooltips (JUCE 8: `ArcButton` inherits `SettableTooltipClient`, `TooltipWindow` added to the editor), ArcButton keyboard-focus ring, dead `m_changedCellCount` removed. Deferred: keyboard drawing (arrows/Ctrl+Z, knob fine-tune); theme interface kept as a future seam.
- **Positioning unification 2026-08-29:** `docs/positioning.md` — four conventions (local-bounds ratios; parent-space only via `IComponentBounds`; pixels only via `GridLayout::scaledCap`/`scaleFromHeight`; sprite alignment only via `EditorLayout`). Fixed-px caps converted to scaled caps (bottom-bar 50/70/18px, `JackHitMap` radius → `clamp(12, min(w,h)·24/770, 24)` member refreshed per grid size, DAW jack y → `CableJackHeightRatio`). Sprite coupling removed: `ColorPalette` self-scans its sprite, `setImageBottomShift`/`setContentCenterOffset`/`setCanvasTopOffset` replaced by semantic `setPedalBottomRatio(ratio)` + `setContentTop(ratio, columnHeightPx)` (formulas proven pixel-identical at design size). Fonts: mixer/bottombar 9px → `clamp(7, h·0.09, 12)`. Tune constants documented; dead `PaletteButtonGapRatio`/`ButtonSquareSizeRatio` removed. Cable px constants left to the cable agent.
- **HP/LP Filter distortion fix + Pitch semitone snap + Bitcrusher dither gate (2026-08-29):** `HpLpFilterEffect` had a latent TDF-II bug — `y` was computed from `x` instead of the state `v` (`y = b0·x + ...` vs the canonical `y = b0·v + ...`), giving the high-pass an effective numerator `b0·A(z) + b1z⁻¹ + b2z⁻²` with a near-DC pole (A(1)≈0 for the HP) → ~900x low-frequency gain → constant distortion/crushing "regardless of parameter settings" (reproduced: 0.5-amp sine → 379-398 peak; corrected form → 1.9/0.52). Fixed to the canonical v-form; Q range capped 0.5-9.5 → 0.5-2.0 (peak ≤ ~6 dB/section); defaults now transparent (High 0, Low 1, Reso 0 via `makeDefinition` default-value args); per-sample coefficient interpolation with prev-state (`m_prevHigh/m_prevLow/m_prevQ`). Pitch Shifter: `snapSteps 25` on knob 2 (the `exp2(rate·2−1)` formula is semitone-linear — no DSP change). Bitcrusher: TPDF dither scaled by `min(1, |hold|·10)` — fades to zero at digital silence (was ±1 LSB white noise on silence, full-scale at 2 bits). Tests: +5 (filter bounded peak, filter transparency, bitcrusher silence + signal, pitch grid) — suite at 38 cases / 4.76M assertions. NOTE: test file include updated for the mid-session `ReverseBufferEffect` → `ReverseEffect` rename.
- **Knob snapping + Sidechain rework (2026-08-29):** `PedalParameterDefinition::snapSteps` (even grid, 0 = off) + `PedalDefinitions::snapValue/snapSteps`. Snap applied on drag (`PedalComponent::onKnobValueChanged`), compiled display + override (`EditorSyncController::applyParameterToPedal` via `pedal->snapValue`), linked/canvas automation display (`syncKnobAutomation`), and the final DSP param value in `processChainBlock` after smoothing (per-node `snapSteps` precomputed into `PedalAssetPayload` by `ConfigManager::loadPedalConfiguration`; mix knob excluded). Grids: Re-Time Time 5 detents → {0.25, 0.5, 0.75, 1, 2}x (double time), Bars 4 → {½, 1, 2, 4}; Sidechain Rate 5 → {⅙, ¼, ⅓, ½, 1} beat; Bitcrusher Bits 15 (2-16); Convolution Reverb Damp 15 (16 rows); Tremolo Shape 3. `RhythmGateEffect` → `SidechainEffect` (`RHYTHM_GATE` → `SIDECHAIN`, integer 13 unchanged, presets migrate); cycle length from transport BPM via the existing `setTransport` virtual (`DspEffect.h:21`, forwarded per block at `UnifiedPedalProcessor.cpp:89`), divisions `{⅙,¼,⅓,½,1}` beat, floor 1.5ms. ReTime DSP indexing realigned: `ratioIdx = lround(t·4)` (5 bins), `barsIdx = lround(t·3)` (4 bins incl. half bar) — the old `lround((t−.25)/.25)` put the new ⅓ detent in the wrong bin. Tests: `tests/test_SnapUtils.cpp` (grid math, detent→DSP-index consistency, Sidechain gate periods at 120 BPM) + `PedalDefinition.cpp` added to the test target — suite at 33 cases / 4.76M assertions.

### Pending
- (none)

## Key Decisions
- Sprite canvas model: `canvastexture.png` is the static background; `m_pixelOverlay` (ARGB, `clear=true`) renders only drawn colored cells on top. `gridValueToPixel(0)` → `Transparent` (not `Black`), so initial all-zero preset shows texture through. Black pixels only appear when user draws with Black color.
- `PixelColor::Transparent(5)` separates "empty" from "drawn Black(0)". Grid cache maps Transparent → 0 and Black → 5 (serialized byte), both treated as "off" by DSP.
- No shadow, no `drawCanvasShadow()`, no `canvasSurface()` — just texture + colored overlay.
- Full-resolution overlay (canvasW × canvasH) rebuilt only on resize, clear, or grid data load.
- Color wells: per-blob sprite drawn behind each blob, centered at blob center, 1.5× blob diameter.
- Effect prebuild: heap allocations for effect creation/preparation moved to UI thread via `prebuildEffects()`; audio thread only does atomic config-pointer exchange (no alloc, no `free`).
- Stereo delay lines: per-channel instead of mono-summed — preserves stereo image through all delay/ring/tape/granular effects.
- Dead scaffolding removed: `DspState`, `DspGraph`, `PedalSkinManager`, `SimdUtils` served no purpose in current architecture.
- Block processing (Phase 5): `processBlock` virtual on `DspEffect` with stateless overrides; `processChainBlock` operates on full blocks; `m_dryBuffer` and `m_crossfadeOldOut` are block-sized (`vector<vector<float>>`). Crossfade processes both configs as full blocks then mixes per-sample. Reduces virtual dispatch 18,432→~42 calls per block.

## Critical Context
- `BinaryData::canvastexture_png` (1MB) and `BinaryData::colorwell_png` (7.9KB) generated from `Assets/Sprites/` — cmake must re-run to pick up new assets.
- `Image::ARGB` with `clearImage = true` ensures overlay starts fully transparent. `Transparent(5)` cells are never drawn.
- `m_gridCache[i] = (pixels[i] == Transparent) ? 0 : uint8_t(pixels[i])` — Transparent emits 0, Black emits 5, both treated as "off" for DSP.
- P0 eliminates all heap operations from `loadPedalConfiguration` on the audio thread path (removed from `processBlock()`; only runs on UI timer).
- P1 added `#include <vector>` to `MiscEffects.h` which was previously missing it.
- `m_changedCellCount` counts non-Transparent cells (same semantics as previously counting non-Black cells).
- Old `m_pixelImage` (128×128 RGB) and `m_renderCache`/`m_cacheDirty` removed — replaced by `m_pixelOverlay`.

## Relevant Files
- **Source/PixelCanvasComponent.h/.cpp**: Sprite canvas implementation — `m_pixelOverlay` (ARGB overlay), `gridValueToPixel(0)` → `Transparent`, no shadow.
- **Source/Resources/ResourceManager.h/.cpp**: Added `ColorWell` to `ImageId`/`TextureId`; loads `canvastexture.png` and `colorwell.png` from BinaryData.
- **Source/UI/Canvas/ColorPalette.cpp**: Draws `ColorWell` sprite beneath each blob before blob rendering.
- **Source/UnifiedPedalProcessor.h/.cpp**: P0 — config-pointer handoff via `ConfigManager` (`prebuildEffects()` into payload, atomic `m_nextConfig`/`m_currentConfig`), atomic `m_crossfadeCounter`, `m_pendingReset`. Phase 5 — `processChainBlock`, block-sized `m_dryBuffer`/`m_crossfadeOldOut`.
- **Source/State/ConfigManager.h/.cpp**: P0 — `prebuildEffects()`, `loadPedalConfiguration()`, config lifecycle (current/next/deferred), ReleaseQueue drain.
- **Source/PluginProcessor.h/.cpp**: P0 — `consumeCompiledResultIfAvailable()` removed from `processBlock()`.
- **Source/PluginEditor.cpp**: UI update loop — `consumeCompiledResultIfAvailable()` runs on timer (UI thread).
- **Source/Effects/DelayEffects.h/.cpp**: P1 — per-channel delay lines for MicroPitch, SimpleDelay, TapeStopEcho. P2 — clamp in TapeStop.
- **Source/Effects/MiscEffects.h/.cpp**: Rhythm Gate (`RhythmGateEffect`) — 3-zone morphable envelope shaper, single shared phase (no per-channel state), 1ms smoothing, mixKnobIndex=3 handled externally by the processor; VCA compressor uses a single shared envelope (max across channels).
- **Source/Effects/GranularBaseEffect.h/.cpp**: P1 — per-channel `GranularProcessorState`.
- **Source/Effects/DistortionEffects.h/.cpp**: P3 — Wavefolder multiplier 9.0f→4.0f. P4 — Waveshaper drive before clipper. Phase 5 — `processBlock` overrides for Waveshaper/Wavefolder.
- **Source/Effects/FilterEffects.cpp**: P7 target (DynamicResonantFilter bandwidth).
- **Source/Effects/DspEffect.h**: Phase 5 — `processBlock` virtual with default sample-by-sample fallback.
- **Source/PedalComponent.h/.cpp**: Cleaned of PedalSkinManager references.
- **Source/Effects/ResamplerEffect.h/.cpp**: v0.2 — S&H resampler + bitcrusher with TPDF dither, anti-alias biquad, DC blocker.
- **Source/Effects/ModulationEffects.h/.cpp**: v0.2 — Tremolo (3 shapes, smoothed square) + Flanger (0.5–10 ms comb, tanh feedback).
- **Source/Effects/OctaverEffect.h/.cpp**: v0.2 — flip-flop sub-octave + rectified upper octave, tone LP.
- **Source/Core/DrawdioConstants.h**: `KnobsPerPedal`/`TotalKnobs` constants for the parameter pipeline.
- **Assets/Sprites/canvastexture.png**: Canvas background texture.
- **Assets/Sprites/colorwell.png**: Color palette well sprite (7.9KB).
