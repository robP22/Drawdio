#pragma once

#include <cstdint>

enum class DspModuleType : uint8_t
{
    BYPASS = 0,
    WAVESHAPER_DISTORTION,
    MICROPITCH_CHORUS,
    MULTI_MODE_FILTER,
    PITCH_SHIFTER_GRANULAR,
    ENVELOPE_VCA_COMPRESSOR,
    GLITCH_STUTTER,
    DIFFUSED_DELAY_NETWORK,
    MATHEMATICAL_WAVEFOLDER,
    FORMANT_VOCAL_SHIFTER,
    // Re-Time reuses the old Tape Stop slot so existing projects migrate.
    RETIME = 10,
    SIMPLE_DELAY,
    PLATE_REVERB,
    RHYTHM_GATE,
    GRANULAR_DELAY,
    COMB_RESONATOR,
    SPECTRAL_FREEZE,
    FREQ_SHIFTER,
    REVERSE_BUFFER,
    GRAIN_SCRUBBER,
    SPECTRAL_FILTER,
    CONVOLUTION_SPACE,
    // Numeric slots 22 and 26 are intentionally reserved. They were
    // Random Modulator and Octaver in older saved projects.
    RESERVED_REMOVED_RANDOM_MODULATOR = 22,
    RESAMPLE_BITCRUSH = 23,
    TREMOLO = 24,
    FLANGER = 25,
    RESERVED_REMOVED_OCTAVER = 26
};
