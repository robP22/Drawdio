# PNG Import to Canvas — Feature Spec

> Status: Archived. Image import is implemented; see `../ui-controls.md` for
> current behavior.

Status: **Archived** - image import was implemented. See `../ui-controls.md` for
the current behavior.
Created: session plan.

## Goal

Allow the user to import any PNG (or JPEG) file, scale it to the 256×256
canvas, quantize each pixel to Drawdio's 12-color palette, write it to the
grid, and recompile the pedal graph from the imported image.

## Color Palette

| Grid value | Color   | RGB (hex)   |
|-----------|---------|-------------|
| 0         | Transparent (off) | sentinel — not matched |
| 1         | Blue    | `0xFF2F73D8` |
| 2         | Green   | `0xFF2BBE65` |
| 3         | Red     | `0xFFE54235` |
| 4         | White   | `0xFFE8E5DC` |
| 5         | Black   | `0xFF121212` |
| 6         | Yellow  | `0xFFFFD700` |
| 7         | Brown   | `0xFF8B4513` |
| 8         | Purple  | `0xFF800080` |
| 9         | Grey    | `0xFF808080` |
| 10        | Pink    | `0xFFFF69B4` |
| 11        | Orange  | `0xFFE67E22` |
| 12        | Violet  | `0xFF8E44AD` |

## Color Matching

Weighted Euclidean distance (green-weighted for perceptual accuracy):

```
distance² = 2·ΔR² + 4·ΔG² + 3·ΔB²
```

Rules:
- Pixel alpha < 128 → grid value 0 (transparent/off).
- Closest match wins; ties between grid value 0 and 5 (same RGB `0xFF121212`)
  resolve to 5 (visual Black).
- All other pixels map to the nearest color index 1–4 or 6–12.

## Import Flow

1. User clicks the **Import** button in the bottom bar (5th in the column stack).
2. `FileChooser` open dialog, filter `*.png;*.jpg;*.jpeg`.
3. `juce::ImageFileFormat::loadFrom(file)`.
4. `img.rescaled(256, 256, juce::Graphics::highResamplingQuality)`.
5. Per-pixel ARGB read → nearest-color mapping → `std::array<uint8_t, TotalCells>`.
6. `audioProcessor.setGridData(grid)`, `m_pixelCanvas.setGridData(grid)`,
   `m_syncController.setAutoEnvelopeDirty()`.
7. Canvas recompiles from the imported image.

## Layout Change

- `GridLayout.h`: `BtnHeightRatio` 0.17 → 0.14 (btnH = 14px; 5 buttons × 14 =
  70px + 4 gaps × 7.5px = 100px usable height).

## Files Changed

| File | Change |
|---|---|
| `GridLayout.h` | `BtnHeightRatio: 0.17 → 0.14` |
| `BottomControlBar.h` | `m_importBtn` + `onPresetImport` callback |
| `BottomControlBar.cpp` | Create/wire/style button, `resized()` for 5 buttons |
| `PluginEditor.h` | `importImage()` method |
| `PluginEditor.cpp` | `importImage()` + color-matching helper (~30 lines) |

## Open Questions (behavior TBD)

- Should very-light or very-dark pixels be treated as transparent, or always
  mapped to White/Black?
- Should transparency be preserved from PNG alpha, or is the whole image
  opaque?
- Should the user be able to pick the sampling mode (nearest-neighbor
  pixelation vs smooth downscale)?
- Should there be a color-count reduction preview before committing?
- Should import replace the current canvas or be added to undo history?
