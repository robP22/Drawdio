# Working Notes

This file is temporary project working memory, not a canonical product or
implementation reference. Current documentation is indexed from `README.md` and
the guides in this directory.

## Current state

- JUCE `8.0.15` (`GIT_TAG 8.0.15`), Catch2 `v3.5.2`. MSVC `14.51` / CMake `4.3.3` / `Visual Studio 18 2026`. `BUNDLE_ID com.robp.drawdio` `DrDd/Draw` `EditorWidth 1400 Height 900` channels `1 1 2 2`.
- Docs audit 2026-09-01 folded: `working-notes` history archived to `docs/archive/DEVELOPMENT-LOG.md`. screenshots `screenshot-1.png` 2560x1440 full-screen Windows, `screenshot-2.png` 1400x900 placeholder for macOS (pending FL load fix).

## v0.2.3 — Editor minimization / buffering fixes

### Fixed
- **Brush size persistence** — Selecting a brush size now survives minimize/maximize and save/restore via `EditorSessionState.brushSizeIndex` (0-3 → 0.75/1.5/2.5/4.0). `ColorPalette.setBrushSizeIndex()` no longer recurses into `onBrushSize`.
- **`setBufferedToImage` staleness** — Removed `setBufferedToImage(true)` from `PedalComponent` and the `ColorPalette` root (kept only on `WoodGrainBackground`). Buffered compositing cached `m_pedalTypes`/`m_currentType` and blob/arc transforms across `EditorSyncController` polls until peer destroy/recreate; now reps repaint every `tick` (60Hz) without the +0.5-1ms buffered win.
- **`floodFill` routing stall** — `PixelCanvasComponent::floodFill` now marks `m_dirtyRows` per touched row before `notifySnapshot`, so `CanvasGraphAnalyzer` no longer receives a zero mask → single-instance fill correctly recompiles cables/chain via `m_lastPedalTypes`/`m_lastRoutingOrder`.
- **Pedal type/knob resync** — `EditorSyncController::refreshRoutingFromConfig` polls `getPedalSlot(slot)` vs `m_lastPedalTypes` every tick (before `isManualMode` return) and `ConfigManager::setManualRouting` now `triggerUINotification()` immediately; `setPedalSlot` already triggered. Pedals update without minimize; `isManualMode` early-return no longer skips `pedalChanged`.
- **Undo/redo bookkeeping** — `undo()`/`redo()` decrement/increment `m_undoBytes`; `applyHistoryData` path keeps grid-undo coherent — single `28 tests` headless suite green.
- **FL mix-chain DELETE hang** — Root loader-lock `ntdll!LdrUnloadDll → LdrpCallInitRoutine → Drawdio!GetPluginFactory+offset → d3d11!CDevice::Release → nvwgf2umx!WaitForSingleObjectEx` (last-instance only; 2 instances kept `HMODULE` ref). Mitigations: `ReleaseQueue::drainAsync` posts `delete` via `MessageManager::callAsync` instead of freeing under loader lock, `ConfigManager::~ConfigManager` `scheduleDelete` detaches `PendingDelete/overflow/m_queue` off-lock, `~DrawdioProcessor` quiesces with `getCallbackLock()` + drains, plus heap-leaked small `DirectX 1×1` keep-alive image to hold `Weak` until process exit (fixed after patch encoding `UTF-16 ff fe → LF`). Repro: `FL → Options → Manage plugins → Start scan` → verify `364B` fst regenerated → mixer delete ×20 window open vs minimized.
- **Header pill redesign** — Internal header pill now ships as `PEDALS | Reset` (`PedalboardHeader.cpp:11`) with the right value slot destructive-red on hover (`HeaderPill.h:131`).

### Changed
- Editor layout helpers `EditorDesignMetrics 74-81 8-16` `PedalboardHeader 74 HeightRatio 0.10` etc. shipped after prior `v0.2.2` canvas rework.
- `ColorPalette.setBrushSizeIndex()` is idempotent; `HeaderPill::getPreferredWidth` now respects `m_accent` fill/rim semantics for the toggle arcs.
- Docs audit 2026-09-01 folded above still pending screenshot-2 replacement; verify `JUCE 8.0.15` produce path on macOS after.

## Next steps

- Mac: replace `images/screenshot-2.png` with full-screen capture after FL Studio non-loading issue is resolved.
- Verify JUCE `8.0.15` build on macOS (no Windows migration yet).
- Redo shared-budget persistence next (shared `64/8MB` across undo+redo — last on plan; currently undo persists, redo RAM-only via `m_redoStack`).
