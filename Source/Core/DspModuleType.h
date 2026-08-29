#pragma once

#include <cstdint>

enum class DspModuleType : uint8_t
{
    BYPASS = 0,
    WAVESHAPER,
    // Slot 2 was MicroPitch Chorus in older saved projects; now a classic chorus.
    CHORUS,
    MULTI_MODE_FILTER,
    PITCH_SHIFTER,
    VCA_COMPRESSOR,
    GLITCH_STUTTER,
    DIFFUSED_REVERB,
    WAVEFOLDER,
    FORMANT_SHIFTER,
    // ReTime reuses the old Tape Stop slot so existing projects migrate.
    RETIME = 10,
    DELAY,
    PLATE_REVERB,
    // Slot 13 was Rhythm Gate in older saved projects; now a BPM-synced sidechain.
    SIDECHAIN,
    GRANULAR_DELAY,
    COMB_RESONATOR,
    SPECTRAL_FREEZE,
    FREQ_SHIFTER,
    REVERSE,
    GRAIN_SCRUBBER,
    SPECTRAL_FILTER,
    CONVOLUTION_REVERB,
    // Slot 22 was Random Modulator in older saved projects.
    HP_LP_FILTER = 22,
    BITCRUSHER = 23,
    TREMOLO = 24,
    FLANGER = 25,
    // Slot 26 was Octaver in older saved projects.
    RESERVED_REMOVED_OCTAVER = 26
};
