// Shim for the plugin's generated JuceHeader.h so the Effects sources can be
// compiled in a headless test target without the full plugin/GUI toolchain.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
