# Drawdio State Format

## Current Format

Preset state uses the three-byte `DRD` magic followed by format version `0x05`.
The serialized state contains:

- 65,536 grid bytes for the 256x256 canvas
- Six pedal-module slots
- Variable-length manual-routing data
- 24 normalized knob values, four per slot
- A 32-bit manual-override mask
- Automation bar count
- Automation section start bar
- Manual-routing mode flag

The current serializer does not persist input gain, output gain, per-pedal gain,
undo history, compiled effect payloads, or a separate automation-render cache.
Those values are reconstructed or remain runtime state.

## Compatibility

Older state versions are accepted when their known fields are present:

- v2: no knob values
- v3: no manual-override mask
- v4: no automation and manual-routing flags

Removed effect IDs are not active implementations. Legacy Analog Octaver ID 26
is converted to Bypass during validation. Random Modulator ID 22 was converted
to the HP/LP Filter in v0.2.1 and now loads that filter. Slot 2 is `CHORUS`
(formerly MicroPitch Chorus; the integer value 2 is unchanged, so older presets
load the new Chorus). Current factory-backed IDs are 1-25, where 22 is the
HP/LP Filter and 26 is reserved.

## Serialization Ownership

`StateSerializer` handles the binary representation. `ConfigManager` and the
processor state own the live values that are passed into serialization. Saved
state should be treated as a versioned interchange format rather than a dump of
the in-memory compiled audio graph.

## Related Documents

- [`effects.md`](./effects.md) - current effect IDs and parameters
- [`architecture.md`](./architecture.md) - configuration publication and runtime state
