# Drawdio State Format

## State Layers

Drawdio keeps persistent behavior separate from editor session context.

`PresetState` contains the reusable configuration:

- 256x256 canvas data
- Six pedal slots
- Manual routing
- Normalized knob values
- Manual override mask
- Automation bar and section settings
- Manual/canvas mode
- Input, output, and per-pedal gains
- Automation-link flags and per-knob link ranges

`EditorSessionState` contains stable project UI context:

- Selected palette colour
- Selected drawing tool
- Selected pedal slot

Transient interaction state is not persisted. This includes hover, focus, active
drags, meter values, playhead position, compiler work, audio buffers, and undo
history.

`ProjectState` contains both layers and is used for host project restoration.

## Documents

The serializer stores a versioned JUCE `ValueTree` document (`StateSerializer::SchemaVersion`, currently `2`) in the host state blob and in `.drawdio` files. The historical `DRD 0x05` preset version noted in older changelogs refers to the pre-ValueTree binary format and is not the current schema version; loading migrates from `0x05` where needed.

The root node is `DrawdioState` with these properties:

- `type`: preset or project document
- `version`: schema version (1..`SchemaVersion`) - documents outside this range are rejected

Preset documents contain a `preset` child. Project documents contain both a
`preset` child and a `session` child.

Binary data properties are used for the fixed-size canvas, pedal IDs, routing,
knob values, gain arrays, and automation link ranges. This keeps the runtime model typed while keeping
the storage boundary independent from UI components.

## Validation

Documents are fully parsed and validated before any live state is changed.
Checks include finite floats (`inputGain`, `outputGain`, `knobValues`, `pedalGains`, `linkRangeMins/Maxs`), pedal ID range (`<= RESERVED_REMOVED_OCTAVER`, 26 migrates to BYPASS), duplicate-free manual routing slots, `overrideMask`/`linkFlags` bit-width (`<= (1u << TotalKnobs) - 1`), `barCount` 1..8, `sectionStartBar` 0..7, `manualMode` 0/1, `linkRange` clamping to [0,1] with at least `0.05` width, and `session` `selectedColour`/`selectedTool`/`selectedPedal` bounds. Invalid documents leave the current state untouched. Valid documents are
applied as one configuration transaction and trigger one compiler update.

## Loading Rules

`.drawdio` loading replaces `PresetState` and preserves the current
`EditorSessionState`. Host project loading replaces both state layers.

## Ownership

`StateSerializer` owns the `ValueTree` representation. `ConfigManager` owns the
live typed state. `EditorProcessorBridge` is the editor-facing facade. UI
components never serialize themselves or access the storage tree.
