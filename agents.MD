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
- **Product:** Drawdio - Visual Audio Effects Pedalboard (19 DSP modules).
- **Core Pipelines:**
  - `PixelCanvasComponent` -> `CanvasMessageQueue` (SPSC lock-free ring buffer).
  - `CompilerThread` invokes `compileCanvas()` to process pixel data without blocking.
  - `UnifiedPedalProcessor` handles sample-accurate execution on the Audio Thread via a 1,024-sample crossfade window.

## Thread Safety & Engineering Directives

### 1. Real-Time Audio Constraints (`UnifiedPedalProcessor`, `DspGraph`)
- **Zero Allocations & Deallocations:** Never inject heap allocation operators (`new`, `malloc`, `std::vector::resize`) or hidden deallocations inside any code branch reachable by `processBlock`.
- **Shared Pointer Destruction Hazard:** Modifying or assigning `std::shared_ptr` references within the audio execution thread is strictly prohibited. Because reference-drop triggers invoke implicit heap `delete` commands, all background updates must utilize raw atomic pointers (`std::atomic<T*>`) or a deferred release pool (`juce::ReleasePool`) to offload object cleanup entirely to the message or background thread loops.
- **Denormal Flush Protection:** Ensure `juce::ScopedNoDenormals` blocks sit securely at the absolute entry points of all performance-critical mathematical DSP processing functions to eliminate CPU subnormal spikes during signal tail decay.

### 2. Multi-Threaded State Hand-Offs (`CompilerThread`, `CanvasMessageQueue`)
- Keep `CanvasMessageQueue` purely single-producer single-consumer (SPSC) using explicit memory fences (`std::memory_order_release` / `std::memory_order_acquire`).

<!-- ANCHORED_SUMMARY -->
## Goal
- Complete sprite canvas refactoring and address all identified architectural/audio thread safety issues in priority order.

## Constraints & Preferences
- Knobs display-only: preset depends entirely on canvas drawing, not knob adjustments.
- LED sprite sheet is 1×2 row-major grid (left=off, right=on), rendered per pedal.
- Canvas has 5 drawable colors (Black, White, Red, Green, Blue). `PixelColor::Transparent(5)` = empty cell — never drawn, shows texture through.
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
- **C6 (shared effect objects):** Replaced per-type singleton with per-chain-position `m_chainEffects[6]` + factory.
- **C7 (granular discontinuity):** `readPtr` reset to 0 on grain restart.
- **C9 (unnecessary repaints):** `repaint()` gated behind `needsRepaint` flag.
- **H6 (integer division):** `static_cast<double>(xSum) / totalPainted` in auto-routing score.
- **Rendering anomaly fixes:** Removed `setRepaintsOnMouseActivity(true)`, added value-change guard in `PedalComponent::setKnobValue()`, added no-change guard in `PedalboardGrid::updateRouting()`, fixed `refreshRoutingFromConfig()` to update only on change, removed dead `m_mouseInside`, removed redundant `repaint()` in `mouseUp()`, guarded `clearCanvas()` against blank-canvas recompile.
- **H1 (stale m_knobValues):** Removed; `getKnobValues()` returns `getSnapshot().values`.
- **M7 (store reordering):** Removed `m_hasMessage` flag from `CanvasMessageQueue`; `hasMessage()` compares indices directly.
- **L5 (bezier caching):** Added `rebuildCableCache()` called from `updateRouting()`.
- **L10 (sparse stroke lookup):** Replaced 64KB `std::array` with `std::unordered_map<int, int>`.
- **L2 (stable sort):** `std::stable_sort` in `CompilerEngine`.
- **LED sprite fix:** Frame rectangles computed from `ledonoff.png` 1×2 grid dimensions.
- **`pixelFromRaw(0)` → Transparent:** Fixed "black square" bug where initial grid data (all 0s) rendered as solid black, hiding the canvas texture. Now `pixelFromRaw(0)` returns `PixelColor::Transparent` instead of `Black`.
- **Color well sprites:** Added `ColorWell` to `ResourceManager::ImageId`/`TextureId`; loads `colorwell.png`. In `ColorPalette::paint()`, draws well sprite beneath each blob centered at blob center, sized at 1.5× blob diameter. Rendering order: palette background → well sprites → color blobs.
- **P0 (audio thread allocations):** Added `m_spareEffects` array to `UnifiedPedalProcessor`. `prebuildEffects()` creates+prepares effects on the calling thread (UI thread via `consumeCompiledResultIfAvailable()`). `loadPedalConfiguration()` swaps from spare pool via `std::swap` — zero heap alloc/dealloc on audio thread. Removed `consumeCompiledResultIfAvailable()` from `PluginProcessor::processBlock()` — config loading only happens on UI timer path.
- **P1 (per-channel stereo):** All 6 delay-based effects now use per-channel delay lines/held values:
  - `ModulatedDelayEffect`: `std::vector<SimpleDelayState> m_delays`
  - `SimpleDelayEffect`: `std::vector<SimpleDelayState> m_delays`
  - `DynamicRingBufferEffect`: `std::vector<RingBufferState> m_buffers`
  - `TapeStopEchoEffect`: `std::vector<TapeStopChannel> m_channels` (struct with buf/writePtr/readHead)
  - `SampleRateDegraderEffect`: `std::vector<float> m_heldValues`
  - `GranularBaseEffect`: `std::vector<GranularProcessorState> m_states`
  - Each writes/reads its own channel's delay line or held value; no cross-channel broadcast.
- **P2 (TapeStopEcho clamp):** Added `std::fmax(0.001f, ...)` lower bound on `readSpeed`; removed dead `if (readSpeed >= 0)` / `else` branch (both computed same value). Included in P1 changes.
- **P3 (Wavefolder aliasing):** Fold multiplier capped from 9.0f to 4.0f in `DistortionEffects.cpp:21`.
- **P4 (SoftDistortion drive):** Drive gain applied before clipper (not after), output normalized (no drive gain multiplication on output). `DistortionEffects.cpp:111-120`.
- **P5 (dead code removal):** Deleted `DspState.h/.cpp`, `DspGraph.h/.cpp`, `PedalSkinManager.h/.cpp`, `SimdUtils.h` (8 files/~500 lines). Cleaned `HeadlessDspTest` (removed DspState include, member, and orphaned methods). Cleaned `PedalComponent` (removed PedalSkinManager include, skin parameter, setSkin method, m_skin member). Updated `CMakeLists.txt` (removed 3 source entries).
- **P6 (DynamicRingBuffer float drift):** Replaced `if (readIdx >= bufSize) readIdx -= bufSize` with `readIdx %= bufSize` in `DelayEffects.cpp:129`.
- **P7 (FormantShifter bandwidth):** Replaced fixed 10% bandwidth with `max(50Hz, centerHz/5)` Q-based bandwidth. Envelope now uses max across all channels instead of only channel 0. Replaced 1-pole filter with clean 2nd-order biquad resonator (TDF-II) for proper formant resonance. `FilterEffects.cpp:94-106`.
- **P8 (colourForPixel singleton):** Determined already resolved — `canvasPixelColour()` is the actual method, theme is injected via `IThemeProvider&`, no `ThemeManager::getDefault()` exists in source.
- **P9 (output limiter):** Added `juce::jlimit(-1.0f, 1.0f, x)` after gain multiplication in `UnifiedPedalProcessor.cpp:274`.
- **SimpleDelay max-delay wrap:** Fixed zero-delay + gain-boost artifact when `delaySec` equals buffer duration — clamped `delaySamples` to `bufSize - 1`. `DelayEffects.cpp:88`.
- **SampleRateDegrader UB:** Added `bitDepth` clamp `[1, 24]` and `holdLen` floor to prevent `1 << bitDepth` UB and division-by-zero. `MiscEffects.cpp:65-66`.
- **AllpassCascade channel count:** Changed from hardcoded `2` channels to `numChannels` parameter. `FilterEffects.cpp:33-35`.
- **TapeStopEcho per-channel readHead:** Moved `readSpeed` calculation inside loop using each channel's own `readHead` instead of hardcoded `m_channels[0]`. `DelayEffects.cpp:165`.
- **Undo routing restore bug:** Fixed `m_pendingRoutingRestore` — flag now resets unconditionally to prevent stale `m_preClearRouting` from incorrectly restoring routing on normal stroke undos after a clear. Added `syncPedals()` to the restore path. `PluginEditor.cpp:164-174`.
- **Canvas-only compile never consumed:** `triggerRecompile()` pushed grid data to the compiler but never called `triggerUINotification()`. Since `consumeCompiledResultIfAvailable()` was gated behind `consumeUINotification()`, the compiled result sat forever in the compiler slot. Audio thread never received the config → no output. Fixed by moving `consumeCompiledResultIfAvailable()` before the UI notification gate in `checkForUpdates()`. `PluginEditor.cpp:182-188`.
- **Granular clicking/tapping:** Single-grain design with 100% amplitude modulation at grain boundaries produced audible "tapping" at certain pitch ratios. Replaced with 50% overlapping dual-grain system — secondary grain head offset by `grainLen/2` with independent Hann window. Hann windows sum to unity at 50% overlap, eliminating amplitude modulation entirely. `PedalStructures.h:263-325`.
- **Minimize/restore rendering (FL Studio):** Added `repaint()` to `PixelCanvasComponent::resized()` (was setting `m_overlayDirty=true` but never queuing repaint). Added `repaint()` to `timerCallback()` after `checkForUpdates()` so every 20Hz tick forces a paint — handles FL Studio window hide/show cycles that don't trigger OS expose events. `PixelCanvasComponent.cpp:32-35`, `PluginEditor.cpp:224-227`.
- **Knob labels:** Added parameter labels (Wet, Dry, Vol, Drive/Rate/etc.) rendered centered beneath each knob at `pedalH * 0.10` below knob center (~30px at design resolution). Text drawn after knobs, white 75% opacity, font `pedalH * 0.04`. `GridLayout.h:42`, `PedalComponent.cpp:92-114`.
- **Phase 5 (block processing):** Restructured `processAudioBlock` from per-sample to per-block processing. Added `processBlock` virtual to `DspEffect` with default sample-by-sample fallback. Waveshaper/Wavefolder override with tight inner loops. `processWithConfig` renamed to `processChainBlock` — processes the full sample block per node instead of one sample per call. `m_dryBuffer` and `m_crossfadeOldOut` changed from per-channel scalar to per-channel block buffers (`vector<vector<float>>`). Crossfade path processes both configs as full blocks then per-sample mixes. Reduces virtual dispatch from 18,432 calls/block (6 nodes × 3 params + 6 processSample × 512 samples) to ~42 (6× processBlock + batched preamble). `DspEffect.h:17-21`, `DistortionEffects.cpp:20-37`, `UnifiedPedalProcessor.cpp:307-354`.

### Pending
- (none)

## Key Decisions
- Sprite canvas model: `canvastexture.png` is the static background; `m_pixelOverlay` (ARGB, `clear=true`) renders only drawn colored cells on top. `pixelFromRaw(0)` → `Transparent` (not `Black`), so initial all-zero preset shows texture through. Black pixels only appear when user draws with Black color.
- `PixelColor::Transparent(5)` separates "empty" from "drawn Black(0)". Grid cache maps both Transparent and Black to DSP value 0 (off).
- No shadow, no `drawCanvasShadow()`, no `canvasSurface()` — just texture + colored overlay.
- Full-resolution overlay (canvasW × canvasH) rebuilt only on resize, clear, or grid data load.
- Color wells: per-blob sprite drawn behind each blob, centered at blob center, 1.5× blob diameter.
- Effect prebuild: heap allocations for effect creation/preparation moved to UI thread via `prebuildEffects()`; audio thread only does `std::swap` (no alloc, no `free`).
- Stereo delay lines: per-channel instead of mono-summed — preserves stereo image through all delay/ring/tape/granular effects.
- Dead scaffolding removed: `DspState`, `DspGraph`, `PedalSkinManager`, `SimdUtils` served no purpose in current architecture.
- Block processing (Phase 5): `processBlock` virtual on `DspEffect` with stateless overrides; `processChainBlock` operates on full blocks; `m_dryBuffer` and `m_crossfadeOldOut` are block-sized (`vector<vector<float>>`). Crossfade processes both configs as full blocks then mixes per-sample. Reduces virtual dispatch 18,432→~42 calls per block.

## Critical Context
- `BinaryData::canvastexture_png` (1MB) and `BinaryData::colorwell_png` (7.9KB) generated from `Assets/Sprites/` — cmake must re-run to pick up new assets.
- `Image::ARGB` with `clearImage = true` ensures overlay starts fully transparent. `Transparent(5)` cells are never drawn.
- `m_gridCache[i] = (pixels[i] == Transparent) ? 0 : uint8_t(pixels[i])` — both Transparent and Black emit 0 for DSP.
- P0 eliminates all heap operations from `loadPedalConfiguration` on the audio thread path (removed from `processBlock()`; only runs on UI timer).
- P1 added `#include <vector>` to `MiscEffects.h` which was previously missing it.
- `m_changedCellCount` counts non-Transparent cells (same semantics as previously counting non-Black cells).
- Old `m_pixelImage` (128×128 RGB) and `m_renderCache`/`m_cacheDirty` removed — replaced by `m_pixelOverlay`.

## Relevant Files
- **Source/PixelCanvasComponent.h/.cpp**: Sprite canvas implementation — `m_pixelOverlay` (ARGB overlay), `pixelFromRaw(0)` → `Transparent`, no shadow.
- **Source/ResourceManager.h/.cpp**: Added `ColorWell` to `ImageId`/`TextureId`; loads `canvastexture.png` and `colorwell.png` from BinaryData.
- **Source/CanvasModule.cpp**: Passes `resources` to `PixelCanvasComponent`.
- **Source/ColorPalette.cpp**: Draws `ColorWell` sprite beneath each blob before blob rendering.
- **Source/UnifiedPedalProcessor.h/.cpp**: P0 — `prebuildEffects()`, `m_spareEffects`, swap-based `loadPedalConfiguration()`, atomic `m_crossfadeCounter`, `m_pendingReset`, `m_chainEffects[6]`. Phase 5 — `processChainBlock`, block-sized `m_dryBuffer`/`m_crossfadeOldOut`.
- **Source/PluginProcessor.h/.cpp**: P0 — `consumeCompiledResultIfAvailable()` removed from `processBlock()`.
- **Source/PluginEditor.cpp**: UI update loop — `consumeCompiledResultIfAvailable()` runs on timer (UI thread).
- **Source/Effects/DelayEffects.h/.cpp**: P1 — per-channel delay lines for ModulatedDelay, SimpleDelay, DynamicRingBuffer, TapeStopEcho. P2 — clamp in TapeStop.
- **Source/Effects/MiscEffects.h/.cpp**: P1 — `SampleRateDegraderEffect` per-channel `m_heldValues`.
- **Source/Effects/GranularBaseEffect.h/.cpp**: P1 — per-channel `GranularProcessorState`.
- **Source/Effects/DistortionEffects.h/.cpp**: P3 — Wavefolder multiplier 9.0f→4.0f. P4 — SoftDistortion drive before clipper. Phase 5 — `processBlock` overrides for Waveshaper/Wavefolder.
- **Source/Effects/FilterEffects.cpp**: P7 target (FormantShifter bandwidth).
- **Source/Effects/DspEffect.h**: Phase 5 — `processBlock` virtual with default sample-by-sample fallback.
- **Source/HeadlessDspTest.h/.cpp**: Cleaned of DspState references.
- **Source/PedalComponent.h/.cpp**: Cleaned of PedalSkinManager references.
- **Assets/Sprites/canvastexture.png**: Canvas background texture.
- **Assets/Sprites/colorwell.png**: Color palette well sprite (7.9KB).
