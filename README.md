# Drawdio - Visual Audio Effects Pedalboard

Drawdio is a JUCE 8.0.4 VST3, AU, and Standalone audio effect. It converts
drawings on a 256x256 canvas into pedalboard routing, effect parameters, and
DAW-synchronized automation.

<p align="center">
  <img src="images/screenshot-1.png" alt="Drawdio Screenshot 1" width="49%"/>
  <img src="images/screenshot-2.png" alt="Drawdio Screenshot 2" width="49%"/>
</p>

## Features

- 24 active DSP effects plus Bypass
- 6 pedal slots in a 2x3 board
- 256x256 canvas with 12 drawable colors and transparent cells
- Flood fill, rebound drawing, four brush sizes, and undo/redo
- Background canvas compilation with 300 ms drawing debounce
- 20 ms crossfade between active configurations
- Audio-thread processing with prebuilt effect payloads and atomic handoff
- DAW-synchronized automation from canvas Y-position
- Manual cable routing between pedal slots
- Image import into the Drawdio color palette
- Per-pedal gain, effect-dependent wet/dry mix, and peak metering
- VST3, AU on macOS, and Standalone targets

Two numeric effect IDs are reserved for removed legacy effects: ID 22 was
Random Modulator and ID 26 was Analog Octaver. Loading those IDs migrates them
to Bypass; they are not active catalog entries.

## Product Version

The product is labeled **Drawdio v0.2.2**. The preset format is independently
identified as `DRD` version `0x05`.

## Releases

Download builds from the [GitHub Releases page](https://github.com/robP22/Drawdio/releases/latest):

- [Drawdio VST3 (macOS)](https://github.com/robP22/Drawdio/releases/latest/download/Drawdio-macOS-VST3.zip)
- [Drawdio Standalone (macOS)](https://github.com/robP22/Drawdio/releases/latest/download/Drawdio-macOS-Standalone.zip)

### Installing

**macOS VST3:** unzip and copy `Drawdio.vst3` into
`~/Library/Audio/Plug-Ins/VST3` (create the folder if needed). The AU build
installs the same way into `~/Library/Audio/Plug-Ins/Components`, and the
Standalone app can be copied to `Applications`. Downloaded builds are unsigned,
so macOS Gatekeeper may block the first launch: right-click (or
Control-click) the app or plugin and choose Open. Restart or rescan the DAW
after installation.

**Windows VST3:** unzip and copy `Drawdio.vst3` into
`C:\Program Files\Common Files\VST3` (requires Administrator rights) or the
per-user directory `%LOCALAPPDATA%\Programs\Common\VST3`. Restart or rescan the
DAW.

**Linux VST3:** copy `Drawdio.vst3` into `~/.vst3`.

See [`docs/build.md`](docs/build.md) for source builds, platform details, and
the `updater.sh` / `updater.ps1` release helpers.

## Documentation

- [`docs/effects.md`](docs/effects.md) - active effect catalog and parameter behavior
- [`docs/architecture.md`](docs/architecture.md) - thread, configuration, canvas, and audio pipeline
- [`docs/ui-controls.md`](docs/ui-controls.md) - canvas, routing, automation, and editor behavior
- [`docs/state-format.md`](docs/state-format.md) - preset serialization
- [`docs/resources.md`](docs/resources.md) - embedded assets and sprite layouts
- [`docs/build.md`](docs/build.md) - build and deployment instructions
- [`docs/audits/`](docs/audits/) - dated technical audits

## Building

Requirements:

- CMake 3.24 or newer
- A C++20 compiler
- JUCE 8.0.4, fetched by CMake during configuration

### macOS

```bash
git clone https://github.com/robP22/Drawdio.git
cd Drawdio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Drawdio_Standalone --parallel
```

### Linux

Install the required desktop and audio development packages, then run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Drawdio_VST3 --parallel
```

### Windows

Configure with a Visual Studio generator, select the x64 architecture, and
build the Release configuration:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Drawdio_VST3 --parallel
```

Available targets are `Drawdio_VST3`, `Drawdio_AU` on macOS, and
`Drawdio_Standalone`.

For a Release VST3 build and installation, use `./updater.sh` on macOS, Linux,
or Windows Git Bash, `updater.ps1` from PowerShell, or `updater.cmd` from
Command Prompt. See [`docs/build.md`](docs/build.md#release-updater) for
installation paths and permission behavior.

## Source Layout

```text
Assets/                 Embedded PNG sprites
Source/Core/            Contracts, constants, canvas analysis, module IDs
Source/Dsp/             DSP primitives, reverb network, effect factory
Source/Effects/         Active DspEffect implementations
Source/Compile/         Canvas queue, debounce, and compiler thread
Source/State/           Config, routing, automation, and serialization
Source/UI/              Canvas, controls, pedalboard, and theme components
Source/PluginProcessor  AudioProcessor entry point
Source/PluginEditor     Editor entry point and UI synchronization
```

## Scope and Runtime Notes

The audio path does not use MIDI. Normal processing is allocation-free after
preparation. CMake configuration fetches JUCE if it is not already available,
so an initial source configuration requires network access; the plugin runtime
does not perform network operations.

There is currently no repository license file.

## Contact

Questions, feedback, or bug reports: [drawdio@proton.me](mailto:drawdio@proton.me)
