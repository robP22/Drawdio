# Drawdio - Visual Audio Effects Pedalboard

Drawdio is a JUCE 8.0.15 VST3, AU, and Standalone audio effect. It converts drawings on a 256x256 canvas into pedalboard routing, effect parameters, and DAW-synchronized automation. Six pedal slots in a 2x3 board hold 25 active DSP effects plus Bypass; allocation-free audio processing, atomic configuration handoff, and a 20 ms crossfade keep the signal path real-time safe while a background compiler turns brush strokes into sound.

<p align="center">
  <img src="images/screenshot-1.png" alt="Drawdio Screenshot 1" width="49%"/>
  <img src="images/screenshot-2.png" alt="Drawdio Screenshot 2" width="49%"/>
</p>

## How to Use

### Canvas and Palette

The canvas is a fixed 256x256 grid (65,536 cells). Twelve drawable colors are available — Black, White, Red, Green, Blue, Yellow, Brown, Purple, Grey, Pink, Orange, Violet — plus `Transparent` for empty cells that show the canvas texture through. Serialized `0` means empty and `5` means drawn Black; both are treated as off by the DSP, while the overlay renders only non-transparent cells.

Four brush sizes (0.75, 1.5, 2.5, 4.0), flood fill, erasing, and rebound drawing are available. Palette wells drawn from `colorwell.png` sit beneath each color blob at 1.5x blob diameter. Undo/redo is shared between the two stacks and capped at 64 levels / 8 MB combined; it persists across editor minimize/maximize, and importing an image or replacing the grid clears history.

The **Import** button opens a file chooser for any image JUCE can load. The image is rescaled to 256x256, pixels with alpha below 128 become transparent, and opaque pixels are quantized to the Drawdio palette with Floyd-Steinberg error diffusion and serpentine scanning.

### Pedals and the Board

Six pedal slots in a `2 x 3` grid mirror a physical board. Each pedal shows four knobs; individual effects may leave positions unlabeled or unwired (the mix knob position varies per effect). Full catalog and knob labels are in [`docs/effects.md`](docs/effects.md). One numeric ID is reserved for a removed effect: ID 26 was Analog Octaver and migrates to Bypass on load, and ID 22 was Random Modulator in older projects and now maps to the HP/LP Filter.

- **Header pills** — `PEDALS | Reset` dual-zone pill centered above the board. `Reset` fills red on hover.
- **Pedal selection** — click a pedal to select it; the mixer and header reflect the selection.
- **Gain** — input gain, output gain, and per-pedal gain (-32 dB to +6 dB) are independent of wet/dry mix. Peak meters on each strip sample the audio thread via atomics.
- **Knobs** — drag to adjust. Several parameters snap to detents (Re-Time Time 5, Bars 4, Sidechain Rate 5, Pitch Shifter 25, Bitcrusher 15, Convolution Reverb Damp 15, Tremolo Shape 3); snapping applies on drag, on compiled display, and on automation. The mix knob never snaps and is excluded from automation-link blending.

### Routing

- **Canvas mode** — active effects are ordered automatically from horizontal pixel scores (graph analysis cached per row, stable ordering for equal scores).
- **Manual mode** — drag cables between pedal jacks (and the DAW Input/Output jacks at the board edges). Each pedal allows one incoming and one outgoing connection; conflicting connections are removed. DAW jack rendering now scales with board height (`Cable::JackHeightRatio * DawJackScale 0.9`, top-flush at all window sizes via `PedalboardGrid::dawJackHeight()`). Magnetic routing and gap-aware lane placement keep cables readable. Invalid or empty manual routing falls back to automatic routing.

Switching between Canvas and Manual is immediate; manual mode seeds its parameter cache from the current compiled values and preserves overridden knobs.

### Automation

Automation is compiled from the canvas Y-position into 128 horizontal slices (two canvas columns per slice, weighted average ignoring transparent cells). Playback follows DAW PPQ when available and falls back to 120 BPM / 0 PPQ.

The bottom-bar automation display shows an eight-bar envelope. Bar-count choices are 1, 2, 4, and 8; shorter windows can be repositioned with `sectionStartBar`. In Manual mode the envelope is directly drawable: left-drag paints values (lerped across slices) and right-drag repositions the section window. Each knob can be linked to automation with an adjustable range (minimum width 0.05); moving a linked knob creates a manual override and removes that link.

### Presets and Session

- **Save / Load** — presets write a versioned JUCE `ValueTree` (SchemaVersion 3, `DrawdioState` root, `type`/`version` properties) to `.drawdio` files containing canvas data, pedal slots, routing, knob values, override mask, link flags/ranges, bar/section settings, mode, gains, and the 128-slice manual envelope. Loading a `.drawdio` replaces the preset layer and preserves the current editor session.
- **Host projects** — DAW save/restore stores both the preset and the editor session (selected colour, tool, pedal, brush size, link-range edit state, and whether the manual envelope has been overridden).

## Installation

Download builds from the [GitHub Releases page](https://github.com/robP22/Drawdio/releases/latest):

- [Drawdio VST3 (macOS)](https://github.com/robP22/Drawdio/releases/latest/download/Drawdio-macOS-VST3.zip)
- [Drawdio Standalone (macOS)](https://github.com/robP22/Drawdio/releases/latest/download/Drawdio-macOS-Standalone.zip)

**macOS VST3:** unzip and copy `Drawdio.vst3` into `~/Library/Audio/Plug-Ins/VST3` (create the folder if needed). The AU build installs the same way into `~/Library/Audio/Plug-Ins/Components`, and the Standalone app can be copied to `Applications`. Downloaded builds are unsigned, so macOS Gatekeeper may block the first launch: right-click (or Control-click) the app or plugin and choose Open. Restart or rescan the DAW after installation.

**Windows VST3:** unzip and copy `Drawdio.vst3` into `C:\Program Files\Common Files\VST3` (requires Administrator rights) or the per-user directory `%LOCALAPPDATA%\Programs\Common\VST3`. Restart or rescan the DAW via `Options -> Manage plugins -> Start scan`.

**Linux VST3:** copy `Drawdio.vst3` into `~/.vst3`.

See [`docs/build.md`](docs/build.md) for source builds, platform details, and the `updater.sh` / `updater.ps1` release helpers.

## Building from Source

### Requirements

- CMake 3.24 or newer
- A C++20 compiler (MSVC 2022, Xcode 14+, or GCC 11+ / Clang 14+)
- JUCE 8.0.15 — fetched automatically by CMake via `FetchContent` on first configure (requires network access; no network use at runtime)
- Catch2 v3.5.2 — fetched automatically when tests are enabled
- Linux only: JUCE Linux dependencies (ALSA, JACK, `libfreetype`, `libx11`, `libxrandr`, `libxinerama`, `libxcursor`, `libgl1-mesa-dev`, `webkit2gtk` for JUCE extras). See [`docs/build.md`](docs/build.md) for the package list.

The product version is **Drawdio v0.2.4** (`project(Drawdio VERSION 0.2.4)` in `CMakeLists.txt`, `package.json` `0.2.4`). The plugin metadata identifies the company as `robP`, bundle ID `com.robp.drawdio`, and preserves manufacturer code `DrDd` / plugin code `Draw` for host compatibility. Default editor size is 1400x900, scalable 0.75x (1050x675) to 1.25x (1750x1125) at a locked 14:9 ratio.

### macOS

```bash
git clone https://github.com/robP22/Drawdio.git
cd Drawdio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Drawdio_Standalone --parallel
```

The AU target is available with `--target Drawdio_AU`.

### Linux

Install the required desktop and audio development packages, then run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Drawdio_VST3 --parallel
```

### Windows

Configure with a Visual Studio generator, select the x64 architecture, and build the Release configuration:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Drawdio_VST3 --parallel
```

Available targets are `Drawdio_VST3`, `Drawdio_AU` on macOS, and `Drawdio_Standalone`.

For a Release VST3 build and installation, use `./updater.sh` on macOS, Linux, or Windows Git Bash, `updater.ps1` from PowerShell, or `updater.cmd` from Command Prompt. See [`docs/build.md`](docs/build.md#release-updater) for installation paths and permission behavior.

To run the headless tests:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target drawdio_tests --parallel
ctest --test-dir build --output-on-failure
```

## Source Layout

```text
Assets/Sprites/         Embedded PNG sprites
Assets/Fonts/           Geist Pixel fonts (TTF, OFL.txt not embedded)
Source/Core/            Constants, DspModuleType, CanvasAnalysis, contracts
Source/Dsp/             Reverb network, DspEffectFactory, shared primitives
Source/Effects/         Active DspEffect implementations (25 + BYPASS = 27 slots)
Source/Compile/         Canvas queue, debounce, graph analyzer, compiler engine/thread
Source/State/           Config, routing, automation, serialization, parameter bank
Source/UI/Canvas/       Pixel canvas + palette
Source/UI/Pedalboard/   Pedals, cable routing, grid layout, jack hit-testing
Source/UI/Controls/     Mixer, bottom bar, automation display
Source/UI/Theme/        Theme header pills and look-and-feel
Source/Resources/       ResourceManager, FontManager, ScaledAssetProvider
Source/PluginProcessor  AudioProcessor entry point
Source/PluginEditor     Editor entry point and UI synchronization
```

## Documentation

- [`docs/effects.md`](docs/effects.md) - active effect catalog and parameter behavior
- [`docs/architecture.md`](docs/architecture.md) - thread, configuration, canvas, and audio pipeline
- [`docs/ui-controls.md`](docs/ui-controls.md) - canvas, routing, automation, and editor behavior
- [`docs/ui-architecture.md`](docs/ui-architecture.md) - state, synchronization, compilation, and layout boundaries
- [`docs/state-format.md`](docs/state-format.md) - preset serialization (SchemaVersion 3)
- [`docs/resources.md`](docs/resources.md) - embedded assets and sprite layouts
- [`docs/build.md`](docs/build.md) - build and deployment instructions
- [`docs/positioning.md`](docs/positioning.md) - UI positioning conventions
- [`docs/audits/`](docs/audits/) - dated technical audits

## Scope and Runtime Notes

The audio path does not use MIDI. Normal processing is allocation-free after preparation (effect instances and buffers are prepared on the UI thread before publication via atomic handoff and drained via `ReleaseQueue`). CMake configuration fetches JUCE if it is not already available, so an initial source configuration requires network access; the plugin runtime does not perform network operations.

## License

MIT License — see [LICENSE](LICENSE). Copyright (c) 2026 robP.

## Contact

Questions, feedback, or bug reports: [drawdio@proton.me](mailto:drawdio@proton.me)
