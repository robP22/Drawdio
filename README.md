# Drawdio — Visual Audio Effects Pedalboard

A JUCE 8.0.4-based VST3/AU plugin that converts drawings on a 256×256 pixel canvas into real-time audio effect parameters. The grid is divided horizontally into row bands — one per active pedal — and each band's columns are subdivided among its 4 knobs. Color-weighted pixel accumulation within each region drives knob values, automation envelopes, and the pedal routing order.

<p align="center">
  <img src="images/screenshot-1.png" alt="Drawdio Screenshot 1" width="49%"/>
  <img src="images/screenshot-2.png" alt="Drawdio Screenshot 2" width="49%"/>
</p>

## Features

- **23 DSP Modules**: Waveshaper, wavefolder, comb resonator, multi-mode filter, formant shifter, spectral freeze, microPitch chorus, simple delay, tape-stop echo, granular delay/pitch/scrubber, glitch stutter, SSB frequency shifter, VCA compressor, sidechain ducker, reverse buffer, spectral filter, convolution space, random modulator, diffused/plate reverb, and bypass
- **6 Pedal Slots** in a 2×3 grid with per-pedal gain, wet/dry mix, and peak metering
- **256×256 Drawable Canvas** with 13 colors (12 drawable + Transparent sentinel), flood fill, undo/redo (32 levels, in-session)
- **Real-time Canvas Compilation** via background compiler thread with 300ms pen debounce
- **Smooth 20ms Crossfade** between old and new pedal configurations
- **Zero-Heap-Allocation Audio Thread** — effects prebuilt on UI thread into the config payload, handed off via atomic pointer exchange
- **DAW-Synced Automation** from canvas Y-position with bar count selection (1/2/4/8) and section repositioning
- **Knob-to-Automation Linking** with per-knob blend strength; manual adjustment auto-removes link
- **Manual Cable Drag-and-Drop** routing between pedals
- **Gap-Aware Bezier Cables** that route between pedal enclosures
- **Preset Save/Load** with binary serialization and versioning
- **Preset File Chooser** with SafePointer-guarded async dialogs
- **Bottom Control Bar** with input/output gain knobs, mixer strips, and automation graph
- **Hardware-Styled UI** with enclosure sprites, 3D-skeuomorphic palette blobs, arc-shaped toolbar buttons, and sprite-sheet LEDs
- **Retina-Safe Rendering** — overlay images cached and rebuilt only on change; no per-frame image rebuilds
- **Cross-platform**: VST3, AU (macOS), and Standalone

## Standalone and VST3 Release
These are provided via link due to the file size restrictions: https://drive.google.com/drive/folders/1afhPdU5GVl5QhDcuX6LSJZy3aKaD8gQk?usp=sharing
Understand that this software is mid-development and is intended for users to have fun. If you find any issues and want to communicate them I will start up a discord, currently it is just in my notes.app LOL.

## Building

Requires CMake 3.24+, C++17, and JUCE 8.0.4 (fetched automatically).

### macOS

```bash
brew install cmake
git clone <repo> && cd Drawdio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Drawdio_Standalone -j$(sysctl -n hw.logicalcpu)
```

### Linux (Debian/Ubuntu)

```bash
sudo apt-get install cmake pkg-config libasound2-dev libx11-dev libxext-dev \
  libxinerama-dev libxcursor-dev libxrandr-dev libgl1-mesa-dev libgtk-3-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Drawdio_VST3 -j$(nproc)
```

### Windows (Visual Studio 2022)

Install CMake 3.24+, open the folder in VS2022, select a configuration, and build `Drawdio_VST3`.

### Build Targets

| Target | Platform |
|--------|----------|
| `Drawdio_VST3` | VST3 (all platforms) |
| `Drawdio_AU` | Audio Unit (macOS only) |
| `Drawdio_Standalone` | Standalone application |

## Architecture

### Source Tree

```
Assets/
└── Sprites/                     PNG sprites (embedded into binary via JUCE BinaryData)
Source/
├── Core/                        Cross-cutting contracts & constants
│   ├── Contracts/               IConfigConsumer, IResourceProvider, IComponentBounds, ProcessorInterfaces
│   ├── CanvasAnalysis.cpp       Grid → score/accumulation analysis
│   ├── CompiledPedalConfig.h    Immutable DSP configuration payloads
│   ├── DrawdioConstants.h       Canvas size, grid constants
│   ├── DspModuleType.h          Effect type enum (22 effects + Bypass)
│   └── ParameterTypes.h         Knob/parameter value types
├── Dsp/                         DSP primitives & effect construction
│   ├── DspEffectFactory.cpp     Creates effect instances from DspModuleType
│   ├── GranularProcessor.h      Overlapping dual-grain engine
│   ├── ReverbNetwork.cpp        Comb+allpass diffused network
│   └── DelayPrimitives.h        Ring buffer primitives
├── Compile/                     Background canvas compilation pipeline
│   ├── CanvasMessageQueue       SPSC lock-free ring buffer (cap 8)
│   ├── CompilerThread           Background compile loop
│   ├── CompilerEngine           Grid → PedalAssetPayload analysis
│   └── PenDebouncer             300ms idle gate
├── Effects/                     The 22 DSP effect modules (DspEffect subclasses)
├── State/                       UI-visible state, serialization, automation
│   ├── ConfigManager / ParameterCache / PedalState / ProcessorState
│   ├── CanvasRoutingManager / ManualConnectionModel / EffectConfigRegistry
│   ├── StateSerializer          Binary DRD format (v5)
│   ├── ReleaseQueue             Deferred deletion of retired audio-thread objects
│   └── Automation{Compiler,Envelope,Player}
├── Resources/                   ResourceManager (sprite image loading)
├── UI/                          All editor-side components
│   ├── Canvas/                  PixelCanvasComponent, ColorPalette, ArcButton
│   ├── Pedalboard/              PedalComponent, CableRenderer/PathBuilder, ManualRoutingController
│   ├── Controls/                SpriteKnob, MixerStrip, BottomControlBar, AutomationDisplay
│   ├── Theme/                   IThemeProvider, ThemeManager
│   └── EditorSyncController / PresetFileController / EditorLayout
├── UnifiedPedalProcessor.cpp    Sample-accurate audio-thread chain + 20ms crossfade
├── PluginProcessor.cpp          AudioProcessor entry point
└── PluginEditor.cpp             Editor entry point, 20Hz UI sync timer
```

### Thread Model

```
UI Thread (20Hz timer)
├── PixelCanvasComponent (drawing, undo/redo)
├── ColorPalette (blobs, arc buttons)
├── PedalboardGrid (pedals, cables)
├── BottomControlBar (mixer, knobs, automation display)
├── CompilerThread notification → consumes compiled payload
└── EditorSyncController::tick() → syncs knobs, automation, routing

Background Thread (Compile)
├── CanvasMessageQueue (SPSC lock-free ring buffer, cap 8)
├── PenDebouncer (300ms idle gate)
└── compileCanvas() → PedalAssetPayload

Audio Thread (processBlock)
├── ScopedNoDenormals
├── processChainBlock() → iterates active routing chain
├── Per-effect processBlock() with batched inner loops
├── Per-pedal wet/dry crossfade with per-sample mix interpolation
├── Output limiting (softClip) and per-pedal gain
├── Peak meter reading (relaxed atomics)
└── Crossfade state machine (20ms transition window)
```

### Data Flow

1. User draws → grid pixels change → `triggerRecompile()` pushes snapshot to `CanvasMessageQueue`
2. Compiler thread wakes (50ms cv wait + pen debounce gate) → pops snapshot → calls `compileCanvas()`
3. `compileCanvas()` builds active chain (filtering BYPASS), auto-scores by horizontal pixel distribution, divides 256 rows among pedals, accumulates color weights per parameter, returns `PedalAssetPayload`
4. `consumeCompiledResultIfAvailable()` (called from UI timer) → `loadPedalConfiguration()` → `prebuildEffects()` creates+prepares new effects on UI thread
5. Audio thread swaps in the new config (and its prebuilt effects) via atomic pointer exchange + crossfade
6. After crossfade completes, old config is pushed to release queue (deleted on UI tick)
7. Automation is compiled separately from Y-axis pixel distribution (64 slices) and played back via DAW-synced one-pole smoothed envelope

### Compilation Pipeline

`compileCanvas()` processes grid data into a `PedalAssetPayload`:
- **Routing**: Non-BYPASS pedals are sorted by horizontal pixel score (left-dominant → earlier in chain) or uses manual routing from drag-drop
- **Parameters**: Each pedal's vertical row-range is subdivided among its 4 knobs; `calculatePixelAccumulation()` maps weighted color sum to [0, 1] using coverage, diversity, and paired-color bias
- **Manual Overrides**: Knob values set by hand are preserved against recompilation via per-parameter override mask

## Automation System

- **Compiler**: 64 time-slices across the 256-column grid; weighted Y-average per slice maps bottom→0, top→1
- **Display**: Always shows 8 bars with full envelope in dimmed grey; active section highlighted in limegreen; inactive bars darkened with overlay
- **Section Control**: When using < 8 bars, user clicks the graph to reposition the active window anywhere along the 8-bar timeline
- **Playback**: `AutomationPlayer::tick()` uses DAW ppqPosition via fmod; audio-rate one-pole smoothing at 12Hz corner frequency applied in the audio thread

## Color System

| Color | Grid Value | Weight | Pair |
|-------|-----------|--------|------|
| Transparent | 5 (0 raw) | — | — |
| Black | 0 | −1.0 | Brown |
| Brown | 7 | +0.9 | Black |
| Purple | 8 | −0.55 | Violet |
| Violet | 12 | +0.6 | Purple |
| Blue | 1 | −0.8 | Green |
| Green | 2 | +0.55 | Blue |
| Grey | 9 | 0.0 | neutral |
| Pink | 10 | −0.6 | Red |
| Yellow | 6 | +0.7 | Orange |
| Orange | 11 | −0.9 | Yellow |
| Red | 3 | +0.8 | Pink |
| White | 4 | −0.7 | — |

`PixelColor::Transparent(5)` is the empty-cell sentinel — never drawn on the overlay, shows canvas texture through. `gridValueToPixel(0)` returns Transparent, so the initial all-zero grid shows the canvas texture. Note: the "Grid Value" column shows enum values; the serialized grid bytes differ (Transparent → 0, Black → 5, others → enum value).

## Effect Catalog

| # | Type | Mix Knob | Notes |
|---|------|----------|-------|
| 0 | Bypass | — | Pass-through |
| 1 | Waveshaper | — | ADAA arctan soft-clip, Drive control |
| 2 | MicroPitch | 0 | Dual-tap detuned LFO, per-channel |
| 3 | Multi Filter | — | Orfanidis SVF (LP/BP/HP), constant-Q |
| 4 | Pitch Shifter | — | Granular, full-wet |
| 5 | VCA Compressor | — | Envelope follower |
| 6 | Glitch Stutter | — | Slice/repeat/gate with crossfade |
| 7 | Diff. Delay Net | 0 | Comb+allpass reverb network |
| 8 | Wavefolder | — | sine-fold, adjustable fold amount |
| 9 | Formant Shifter | — | Envelope-followed biquad resonator |
| 10 | Tape Stop Echo | 0 | Variable-speed tape freeze |
| 11 | Simple Delay | 0 | Feedback with LP filter |
| 12 | Plate Reverb | 0 | Stereo-decorrelated plate |
| 13 | Sidechain Pump | — | Rhythmic ducking oscillator |
| 14 | Granular Delay | 0 | 50% overlap dual-grain |
| 15 | Comb Resonator | — | tanh-saturated feedback |
| 16 | Spectral Freeze | 0 | LFO-modulated frozen buffer |
| 17 | Frequency Shift | — | SSB via Hilbert allpass |
| 18 | Reverse Buffer | 0 | Crossfading reverse playback |
| 19 | Grain Scrubber | — | Position-scrubbed granular |
| 20 | Spectral Filter | — | TDF-II biquad, adjustable Q/BW |
| 21 | Conv Space | 0 | 256-tap procedural IR |
| 22 | Random Modulator | — | xorshift32 S&H with smoothing |

Effects with mixKnobIndex() ≥ 0 support parallel dry/wet blending with per-sample linear mix interpolation across the block.

## State Persistence

Binary serialization format (`DRD` magic, version 0x04):
- Grid data (65,536 bytes: full 256×256 array uncompressed; total serialized state ~65.6 KB)
- 6 pedal type slots
- Manual routing order
- 24 knob values (6 slots × 4 knobs) with per-knob override mask
- Backward compatible with v2 (no knobs) and v3 (no override mask)

## Thread Safety Guarantees

- **Zero heap allocations on the audio thread**: All `std::make_unique`, `std::vector::resize`, and effect construction happen in `prebuildEffects()` which is called from the UI thread path only. Effect instances live inside the published `PedalAssetPayload`; the audio thread hands off via atomic config-pointer exchange (`compare_exchange_strong` / `exchange`).
- **Lock-free synchronization**: All inter-thread state transfers use `std::atomic` with explicit memory ordering (`acquire`/`release`). No mutex is locked on the audio thread.
- **Deferred deletion**: Old config payloads are pushed to a lock-free release queue (16-entry ring + 8 atomic slots) from the audio thread and physically deleted (`drainReleaseQueue()`) on the UI thread.
- **NaN/inf contagion guards**: `std::isfinite()` checks guard input writes to delay lines and the shared interpolated delay read; the reverb comb/allpass writes are protected by tanh-bounded feedback instead. Filter states reset on non-finite output.
- **Denormal flush**: Every DSP processing entry point has `juce::ScopedNoDenormals`.

## Security

This plugin does not require network access, does not store sensitive data, processes audio only (no MIDI), and is safe for sandboxed plugin environments.

## License

MIT
