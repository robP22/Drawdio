# Drawdio - Visual Audio Effects Pedalboard

A JUCE-based VST/AU plugin that lets you draw audio effects on a canvas. Each colored stroke controls different effect parameters, creating a unique visual programming interface for audio processing.

## Features

- **19 DSP Effects**: Waveshaper, distortion, filters, delays, reverb, pitch shifting, granular processing, and more
- **Visual Programming**: Draw on a canvas to control effect parameters
- **6 Pedal Slots**: Arrange multiple effects in a chain
- **Real-time Compilation**: Changes to the canvas are compiled and applied with smooth crossfades
- **Cross-platform**: Builds for VST3, AU, and Standalone

## Building

### Prerequisites

**Linux (Debian/Ubuntu):**
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    pkg-config \
    libasound2-dev \
    libx11-dev \
    libxext-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxrandr-dev \
    libgl1-mesa-dev \
    libgtk-3-dev
```

**macOS:**
```bash
brew install cmake
```

**Windows:**
- Install [CMake](https://cmake.org/download/) 3.24+
- Install Visual Studio 2022 with C++ support
- Install JUCE dependencies via vcpkg or manual installation

### Build Steps

```bash
# Clone the repository
git clone https://github.com/yourusername/Drawdio.git
cd Drawdio

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build VST3 (or AU, Standalone)
make Drawdio_VST3 -j$(nproc)

# The plugin will be located at:
# Drawdio_artefacts/VST3/Drawdio.vst3
```

### Build Targets

Available targets:
- `Drawdio_VST3` - VST3 plugin
- `Drawdio_AU` - Audio Unit plugin (macOS only)
- `Drawdio_Standalone` - Standalone application

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     DrawdioPlugin                           │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐     ┌───────────────────────────────┐  │
│  │  PluginEditor   │     │  DrawdioProcessor (DSP Core)  │  │
│  │  (UI Layer)     │────▶│  ┌─────────────────────────┐  │  │
│  │                 │     │  │ UnifiedPedalProcessor   │  │  │
│  │  ┌───────────┐  │     │  │  19 DSP Effects (array) │  │  │
│  │  │CanvasModule│ │     │  └─────────────────────────┘  │  │
│  │  │Pedalboard │  │     │  ┌─────────────────────────┐  │  │
│  │  └───────────┘  │     │  │ CompilerThread         │  │  │
│  └─────────────────┘     │  │ (background compile)   │  │  │
│                          │  └────────────┬────────────┘  │
│                          │               │                │
│                          │  ┌────────────▼────────────┐  │
│                          │  │ CanvasMessageQueue       │  │
│                          │  │ (lock-free ring buffer)  │  │
│                          │  └─────────────────────────┘  │
│                          └───────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

1. User draws on `PixelCanvasComponent` → grid data changes
2. `triggerRecompile()` → pushes snapshot to `CanvasMessageQueue`
3. `CompilerThread` detects change → calls `compileCanvas()`
4. `PedalAssetPayload` created → loaded into `UnifiedPedalProcessor`
5. Audio processing uses payload with wet/dry/volume mixing
6. Smooth 1024-sample crossfade between configurations

## Security Notes

This plugin:
- Does not require network access
- Does not store sensitive data
- Processes audio only (no MIDI, no file system access)
- Safe for sandboxed plugin environments

## License

MIT License - See LICENSE file for details