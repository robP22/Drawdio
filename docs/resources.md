# Drawdio Resources and Sprite Layouts

PNG and TTF assets under `Assets/Sprites/` and `Assets/Fonts/` are embedded into the plugin through JUCE
binary data and loaded by `ResourceManager` and `FontManager`.

## Asset Inventory

| Asset | Role |
|---|---|
| `canvastexture.png` | Canvas background texture |
| `colorwell.png` | Palette well behind color blobs |
| `colorpalette_final.png` | Palette artwork |
| `generictextures.png` | Generic UI textures - embedded, not yet decoded (reserved for future feature) |
| `abstract_textures.png` | Abstract UI textures - embedded, not yet decoded (reserved for future feature) |
| `wood_roundover_alpha_fixed.png` | Wood background/roundover artwork |
| `pedalboard_final.png` | Pedalboard artwork |
| `bottomcontrolbar.png` | Bottom control bar artwork (`BottomControlBarBg`) |
| `pedalenclosure_final.png` | Pedal enclosure artwork |
| `input_jack.png` | Input/output jack artwork |
| `ledonoff.png` | LED sprite sheet |
| `Knob_Generic_alpha_cutout.png` | Generic knob sprite |
| `jap_pedal_sprite_sheet.png` | Pedal face artwork and labels |
| `buttonpedestal.png` | Button pedestal artwork - embedded, not yet decoded (reserved for future feature) |
| `GeistPixel*.ttf` + `OFL.txt` | Geist Pixel fonts (Square variant active; `OFL.txt` not embedded) |

## Sprite Conventions

- `ledonoff.png` is a 1x2 row-major sheet: left is off and right is on.
- Palette wells are drawn beneath their corresponding color blobs.
- The Japanese pedal sheet uses fixed cell geometry in the pedal component.
- The canvas background is rendered first; the colored pixel overlay contains
  only non-transparent drawn cells.

## Asset Changes

CMake discovers PNG and TTF assets recursively (`Assets/Sprites/*.png` + `Assets/Fonts/*.ttf` via `GLOB_RECURSE` in `CMakeLists.txt`; `OFL.txt` is not embedded; `BinaryData` names strip hyphens). After changing the asset inventory,
rerun configuration so `DrawdioAssets` regenerates its binary-data sources.

Procedural textures and missing/placeholder sprite entries are handled by the
resource manager's fallback paths. Reserved embedded textures (`generictextures.png`, `abstract_textures.png`, `buttonpedestal.png`) and placeholder provider IDs (`IResourceProvider::PedalFaceGrain` / `OverlayGloss`) remain for a future skinning feature. Asset naming and loading IDs should remain
consistent with `Source/Resources/ResourceManager.*` and
`Source/Core/Contracts/IResourceProvider.h`.
