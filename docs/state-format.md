# Drawdio State Format

## State Layers

Drawdio keeps persistent behavior separate from editor session context.

`PresetState` contains the reusable configuration:

- 512x512 canvas data
- Six pedal slots
- Manual routing
- Normalized knob values
- Manual override mask
- Automation bar and section settings
- Manual/canvas mode
- Input, output, and per-pedal gains
- Automation-link flags and per-knob link ranges (minimum 0.05 width, clamped to [0,1])
- `hasManualEnvelope` and `manualEnvelope[128]` (Manual-mode drawable envelope; Canvas mode uses the compiled envelope)

`EditorSessionState` contains stable project UI context:

- Selected palette colour
- Selected drawing tool
- Selected pedal slot
- Brush size index (0..4)
- Link-range edit enabled flag
- Manual-envelope overridden flag (in-memory only, never written to disk; host save/load drops it)

Transient interaction state is not written to disk. This includes hover, focus, active
drags, meter values, playhead position, compiler work, and audio buffers.
Undo history is also excluded from the disk/host blob, but it persists for the
processor lifetime in memory (`m_undoData`): it survives editor close/reopen and
is synced on the editor tick. The undo wire format starts with magic `0x44553230`,
then undo/redo transaction counts, then 6 bytes per change (U32 index + previous +
current) for new data with a 4-byte (U16 index) legacy fallback; the combined
undo+redo budget is 64 levels / 12 MB.

`ProjectState` contains both layers and is used for host project restoration.

## Documents

The serializer stores a versioned JUCE `ValueTree` document (`StateSerializer::SchemaVersion`, currently `4`) in the host state blob and in `.drawdio` files. Version 4 upscales legacy 256x256 (65536 B) grid blobs via `2x2` replication and remaps old brush indices `0..3 -> 1..4` (new index 0 is the 0.375 finest). Both migrations apply to any accepted document version (1..4) based on blob size and property presence, not on the version number itself.

The root node is `DrawdioState` with these properties:

- `type`: preset or project document
- `version`: schema version (1..`SchemaVersion`) - documents outside this range are rejected

Preset documents contain a `preset` child. Project documents contain both a
`preset` child and a `session` child.

Binary data properties are used for the fixed-size canvas, pedal IDs, knob
values, pedal gain array, automation link ranges, and the 128-float manual envelope. `inputGain`/`outputGain` are plain float properties. `routing` is a variable-length blob of 0..6 slot bytes. This keeps the runtime model typed while keeping
the storage boundary independent from UI components.

## Validation

Documents are fully parsed and validated before any live state is changed.
Checks include finite floats (`inputGain`, `outputGain`, `knobValues`, `pedalGains`, `linkRangeMins/Maxs`, `manualEnvelope`), pedal ID range (`<= RESERVED_REMOVED_OCTAVER`, 26 migrates to BYPASS), manual routing size 0..6 with duplicate-free slots in range 0..5, `overrideMask`/`linkFlags` bit-width (`<= (1u << TotalKnobs) - 1`), `barCount` 1..8, `sectionStartBar` 0..7, `manualMode` 0/1, `linkRange` clamping to [0,1] with at least `0.05` width, and `session` `selectedColour`/`selectedTool`/`selectedPedal`/`brushSizeIndex` (0..4). `hasManualEnvelope` and `linkRangeEditEnabled` are coerced (any nonzero counts as true), not range-rejected. Invalid documents leave the current state untouched. Valid documents are
applied as one configuration transaction and trigger one compiler update.

## Loading Rules

`.drawdio` loading replaces `PresetState` and preserves the current
`EditorSessionState`. Host project loading replaces both state layers.

## Ownership

`StateSerializer` owns the `ValueTree` representation. `ConfigManager` owns the
live typed state. `EditorProcessorBridge` is the editor-facing facade. UI
components never serialize themselves or access the storage tree.
