# DRAWDIO DEVELOPLOG

> Status: Archived historical backlog plus folded working-notes history. Not a
> current product reference. Current behavior is in `README.md` and `docs/*.md`.

## Canvas
- Import image not converting accurately; implement more colors? Should blend and choose closest?

## Color Palette
- Nicer buttons throughout the program
- Button icon sprites (procedural is ugly)
- Render color palette shadow
- Render button pedestal
- Button fill highlight when hovered; click animation for blobs/buttons

## DAW Jacks
- Highlight the jacks we are able to connect to

## Bottom Control Bar
- Larger font on input/output gain knobs; move font upward along the y-axis
- Bottom control bar beauty panel
- Refine mini mixer aesthetic (label on beauty panel?)
- Make slider handle more realistic; implement slot behind it as if it comes through
- Always render sliders/handles (currently only active pedals are rendered)

## Audio Cables
- How do we get realistic looking cables?

## Guitar Pedals
- Add jack labels
- Male audio jack to render where each cable plugs in? Need sprite

## Control System
- Manual automation point control when in manual mode
- Knobs only controlled manually when in manual mode

## Features to Implement
- LFO?
- Settings system:
  - Custom fonts
  - Guitar pedal skin swapping (what style picker?)
  - Preset directory indicator
  - Custom textures? (must prevent unsupported images)
  - ???

## Bugs
- Glitch stutter parameter names not displaying
- Tape stop -> re-time (halftime inspired)

## History

### 2026-09-01 — FL standalone vs drag-VST split + Direct2D teardown hang

- Standalone routing OK (canvas -> cables recompile). Drag-VST stale was a stale `Drawdio.fst` cache (364 B, 2026-09-01 20:10) at `Installed/Effects/VST3` + `New` + `Effects`; purged. Fresh VST `19967488` `2026-09-01 20:43:49` SHA `0A72E23F` via `updater.ps1` (PDB `42561536` in `build/Drawdio_artefacts/Release` only, not in `C:\Program Files\Common Files\VST3`).
- Delete hang: three `FL64.DMP` (955 M, 949 M, 936 M) all loader-lock `ntdll!LdrUnloadDll -> LdrpProcessDetachNode -> LdrpCallInitRoutine -> Drawdio!GetPluginFactory+0x2bb99d -> ucrtbase!execute_onexit_table -> d3d11!CLayeredObject::Release -> nvwgf2umx!WaitForSingleObjectEx` (`32.0.15.9597`, `FL 26.1.5 [5618]`, `Win11 26200`, `JUCE 8.0.15`). Dump `FL64 (3)` timestamp `6A978765` was `19:18` stale vs `20:43` build.
- Fix: `PluginEditor.h:41` `parentHierarchyChanged` now guarded-once `m_gdiForced` `peer->setCurrentRenderingEngine(0)` when `cur != 0` (`Windowing_windows.cpp:5422` `{GDI=0,D2D=1}` `5426 reset/construct` repeated destroys amplified hang). `HeaderPill.h:101` `FontOptions` + `FontManager::initialise()` moved to editor `PluginEditor.cpp:59`. Chosen over removing the call, `visibilityChanged`, or compile-time `contextDescriptors={GDI}`.
- Drag OK vs mixer `cannot load`: scan `PluginManager v1.8.1.5618.log:327` `[verified from meta data]` `moduleinfo.json VST 3.8.0 ABCDEF019182/011234` `FST 364B` — scan never `LoadLibraryW`. Out-of-host `LoadLibraryW 0x7FFB12D80000 GetPluginFactory 0x7FFB12DCF490 dumpbin 8664 VCRUNTIME140 14.51` OK. Delete hang path `FLEngine_x64!CreateFruityInstance` `FreeLibrary` `FL64+0x6f39 -> PeekMessageW 68 -> KiUserCallbackDispatcherContinue 59 -> _ClientFreeLibrary 58`. Mixer enumeration vs `juce_audio_plugin_client_VST3.cpp:72` `BusesProperties` (`PluginProcessor.cpp:27-31` stereo `"1 1" "2 2"`; `"0 1"` removed).
- Release PDB: `CMakeLists.txt:144-147` MSVC `/Zi /DEBUG /OPT:REF /OPT:ICF` `Drawdio_SharedCode.pdb 40.6M` for `cdb` offset decode `GetPluginFactory+0x7534f/0x102886`.
- Build `cmake --build build --config Release -j 6` `ctest 1.82s 1/1 Passed` `updater.ps1` staged `19967488` / `20241408` Standalone. Working tree `PluginProcessor.h:125 CallbackLockHolder` `m_shutdown` heap atomic `m_processingEnabled/m_audioCallsInFlight` `PluginProcessor.cpp:36-62` `ConfigManager.cpp:13 ChangeBroadcaster` `ReleaseQueue.h:30 ~default` `CompilerThread.h:50 m_stopMutex` remain.
- Diagnostic phases: FST cache purge + `PluginManager` tail + `dumpbin /DEPENDENTS` VST3==Standalone `d2d1/dxgi/d3d11/dcomp` + `vst3_helper` JSON identical; `LoadLibraryW` + `GetFactory` OK, Standalone launch OK; `FL64 (3).DMP` decoded `!analyze -v ~*k lmvm Drawdio` `cdb 10.0.29617` / `WinDbg 1.2606.22001.0`.

### 2026-08-30 — Reverb passes

- `ReverbNetwork` `wetScale 1-feedback*0.92` damping `0.02+0.14*decay clamp 0.7-1.4` `sizeScale 0.3+0.7*size` `ReverbEffects` `sizeKnobIndex`; definitions Plate `Mix,Size,Decay` Diffused `Mix,Size,'',Decay`; `ConvolutionReverb` x1.25. Detune `+-50->+-28`, Size per-sample easing `0.001`, output taps `tailScale 0.9 erGain 0.5`, Plate `0.88->0.86`.

### 2026-08-14 to 2026-08-30 — Durable keys (folded from working-notes Archive section)

- `JUCE 8.0.15` `GIT_TAG 8.0.15` `Catch2 v3.5.2` `MSVC 14.51` `CMake 4.3.3` `Visual Studio 18 2026` `BUNDLE_ID com.robp.drawdio` `DrDd/Draw` `PLUGIN_CHANNEL_CONFIGS "1 1" "2 2"` `JUCE_VST3_CAN_REPLACE_VST2=0` `EditorWidth 1400 Height 900`. DSP: `GranularProcessor` staggered dual-grain `128..bufSize-grainLen*max(1,1/speed)-128`, `Chorus` 30 ms center 10-50 ms LFO 0.05-3 Hz, `SimpleDelay` snap `0.55sr firstBlock`, `ReverseBuffer` per-repeat xfade, `MultiMode 0.45sr`, `SpectralFilter` per-sample lerp, `Comb` tail. Effects: `Resampler/Bitcrusher Tremolo/Flanger`, `DrawdioConstants KnobsPerPedal=4`. Canvas: `Transparent(5) vs Black(0)` `gridValueToPixel(0)->Transparent` `m_pixelOverlay ARGB`. `CanvasGraphAnalyzer` row-summary `DirtyRowMask` `m_canvasRevision`. `ScaledAssetProvider` 24-entry, `EditorDesignMetrics` 14:9 1050x675 1750x1125. `ReleaseQueue` 16+8 overflow `pendingDelete` `droppedCount`. See `git log --oneline` and prior `working-notes.md` revisions for verbatim.
- 2026-08-22 audits: `docs/audits/audio-pipeline-audit-2026-08-22.md`, headless `drawdio_tests` 22->38 cases, import Floyd-Steinberg dithering, Windows Direct2D medium->high cubic.

## Notes
