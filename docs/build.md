# Drawdio Build and Deployment

## Requirements

- CMake 3.24 or newer
- A C++20 compiler
- JUCE 8.0.15
- Platform-specific GUI and audio development packages

JUCE is fetched by CMake through `FetchContent` when it is not already
available. Initial configuration therefore requires network access.

The product and plugin version are Drawdio v0.2.5. The installed CMake tool
version is independent; `cmake_minimum_required(VERSION 3.24)` specifies only
the oldest supported CMake release. Any current CMake 4.x release, including
CMake 4.4.2, satisfies this requirement.

The generated plugin metadata identifies the company as `robP`, uses the stable
bundle ID `com.robp.drawdio`, and preserves the manufacturer code `DrDd` and
plugin code `Draw` for host compatibility.

## Configure and Build

### macOS

```bash
git clone https://github.com/robP22/Drawdio.git
cd Drawdio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Drawdio_Standalone --parallel
```

The AU target is available with `--target Drawdio_AU`.

### Linux

Install CMake, a C++20 compiler, and the JUCE Linux dependencies, then run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Drawdio_VST3 --parallel
```

### Windows

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Drawdio_VST3 --parallel
```

The Standalone target is `Drawdio_Standalone`. Use a matching Visual Studio
generator if a different installed version is required.

## Targets and Formats

| Target | Platform |
|---|---|
| `Drawdio_VST3` | Windows, macOS, Linux |
| `Drawdio_AU` | macOS |
| `Drawdio_Standalone` | Windows, macOS, Linux |

The plugin declares mono-input/mono-output and stereo-input/stereo-output
configurations. MIDI input and output are disabled.

## Assets

PNG and TTF files under `Assets/` are discovered recursively (glob with `CONFIGURE_DEPENDS`, so the build re-globs automatically) and embedded through the `DrawdioAssets` binary-data target. The recursive glob covers `Assets/Sprites/*.png` and `Assets/Fonts/*.ttf`; `BinaryData` resource names strip hyphens, so for example `Assets/Fonts/GeistPixel-Square.ttf` becomes `GeistPixelSquare_ttf`. `Assets/Fonts/OFL.txt` is not embedded.

## Release Updater

The updater builds Release VST3 + Standalone (+ AU on macOS) and installs the VST3 bundle. VST3 and Standalone are hard-validated; the macOS AU check is warning-only. Only VST3 is installed. The generated bundle is copied as a complete
`Drawdio.vst3` directory; the internal platform binary must not be flattened.

### macOS, Linux, and Windows Git Bash

Run the Bash helper by path (it resolves the repo root from its own location,
so it works from any working directory when invoked with a path):

```bash
./updater.sh            # from the repository root
/path/to/drawdio/updater.sh --no-install

The helper detects the shell platform, configures the build when
`build/CMakeCache.txt` is absent, builds `Drawdio_VST3` and `Drawdio_Standalone` (plus `Drawdio_AU` on macOS) with
`--config Release`, validates each resulting artefact, and installs the VST3. It does not
delete the build tree for `--reconfigure`.

Options:

```bash
./updater.sh --help
./updater.sh --reconfigure
./updater.sh --parallel 8
./updater.sh --install-dir "/custom/VST3"
./updater.sh --no-install
```

When `--parallel` (alias `--jobs`) is omitted, CMake uses its default or the
`CMAKE_BUILD_PARALLEL_LEVEL` environment variable. The script does not call
platform-specific CPU-count commands. `--help` also accepts `-h`.

Default installation directories:

| Shell platform | Default directory |
|---|---|
| macOS | `$HOME/Library/Audio/Plug-Ins/VST3` |
| Windows Git Bash | `/c/Program Files/Common Files/VST3` or the equivalent native path |
| Linux/WSL | `$HOME/.vst3` |

WSL builds Linux binaries. They are not usable by a native Windows DAW; use
Git Bash/MSYS2 or native PowerShell for Windows builds.

### Native Windows PowerShell

Use the native helper from PowerShell:

```powershell
.\updater.ps1
.\updater.ps1 -Reconfigure
.\updater.ps1 -Parallel 8
.\updater.ps1 -InstallDir "C:\Program Files\Common Files\VST3"
.\updater.ps1 -NoInstall
```

`updater.cmd` is a Command Prompt/Explorer launcher for the same PowerShell
helper. Flags and the exit code pass through:

```text
updater.cmd -NoInstall -Parallel 8
```

The default Windows destination is `%ProgramFiles%\Common Files\VST3`, matching
the existing system-wide installation. If that location is not writable, the
PowerShell and Git Bash helpers automatically retry the per-user directory:

```text
%LOCALAPPDATA%\Programs\Common\VST3
```

To force a particular location, pass `-InstallDir` or `--install-dir`. An
explicit destination does not silently fall back. System installation may still
require Administrator rights. A DAW holding the plugin open can prevent
replacement; close the DAW and retry.

Both helpers stage and validate the replacement before moving the existing
bundle. If replacement fails, the previous bundle is restored when possible.
Neither helper deletes the existing installation before a valid replacement is
available.

The project has no Drawdio-specific CMake `install()` rule, so the updater
manually copies the JUCE-generated VST3 bundle. Hosts may require a restart or
plugin rescan after installation.

## Verification

The repository ships a headless Catch2 test suite. Configure with
`DRAWDIO_BUILD_TESTS=ON` (the default), build the `drawdio_tests` target, and run
it with `ctest`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target drawdio_tests --parallel
ctest --test-dir build --output-on-failure
```

(`--config Release` matters on multi-config generators such as Visual Studio,
where the build would otherwise default to Debug.)

At minimum, verify a Release build of the intended target and launch the
Standalone target or scan the generated plugin with the host's plugin
validator. There is no CI configuration yet.
