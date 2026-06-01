# Drawdio Assets

This directory contains textures, images, and skins for the Drawdio plugin.

## Directory Structure

```
Assets/
├── Skins/
│   └── Pedals/          # Pedal skin textures
│       ├── pedal_blue.png
│       ├── pedal_red.png
│       ├── pedal_green.png
│       └── ...
│   └── Backgrounds/     # Workspace background textures
│       └── wood_desk.png
└── Icons/              # UI icons (if needed)
```

## Usage

### Pedal Skins
Pedal skins are PNG images that replace the procedural paint texture on pedals.
They should be:
- Proportions: Match pedal chassis aspect ratio (~180×240)
- Format: PNG with alpha channel (RGBA)
- Naming: `pedal_<color>_<variant>.png`

### Backgrounds
Workspace background textures are used by `WorkspaceBackground` component.
They should be:
- Resolution: ~1400×800 or scalable
- Format: PNG or JPEG
- Named appropriately (e.g., `wood_desk.png`)

## Loading Images

Use JUCE's `ImageCache` for efficient loading:

```cpp
#include <juce_graphics/juce_graphics.h>

juce::Image skinImage = juce::ImageCache::getFromFile(
    juce::File::getCurrentWorkingDirectory()
        .getChildFile("Assets/Skins/Pedals/pedal_blue.png")
);
```

## Adding New Assets

1. Place the image file in the appropriate subdirectory
2. Update the file listing in CMakeLists.txt if needed
3. Commit the asset file to version control