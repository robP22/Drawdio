#include <catch2/catch_test_macros.hpp>
#include "Dsp/DspEffectFactory.h"
#include "Effects/FilterEffects.h"
#include "Effects/DelayEffects.h"
#include "Effects/DistortionEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/ModulationEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/ReTimeEffect.h"

#include <memory>

TEST_CASE("DspEffectFactory returns non-null for known effect ids", "[factory]")
{
    auto filter = createDspEffect(DspModuleType::MULTI_MODE_FILTER);
    REQUIRE(filter != nullptr);

    auto delay = createDspEffect(DspModuleType::DELAY);
    REQUIRE(delay != nullptr);

    auto dist = createDspEffect(DspModuleType::WAVESHAPER);
    REQUIRE(dist != nullptr);
}

TEST_CASE("DspEffectFactory returns the correct concrete type per id", "[factory]")
{
    auto filter = createDspEffect(DspModuleType::MULTI_MODE_FILTER);
    REQUIRE(dynamic_cast<MultiModeFilterEffect*>(filter.get()) != nullptr);

    auto delay = createDspEffect(DspModuleType::DELAY);
    REQUIRE(dynamic_cast<DelayEffect*>(delay.get()) != nullptr);

    auto chorus = createDspEffect(DspModuleType::CHORUS);
    REQUIRE(dynamic_cast<ChorusEffect*>(chorus.get()) != nullptr);
}

TEST_CASE("DspEffectFactory returns correct types for expanded effects", "[factory]")
{
    auto dist = createDspEffect(DspModuleType::WAVESHAPER);
    REQUIRE(dynamic_cast<WaveshaperEffect*>(dist.get()) != nullptr);

    auto reverb = createDspEffect(DspModuleType::DIFFUSED_REVERB);
    REQUIRE(dynamic_cast<DiffusedReverbEffect*>(reverb.get()) != nullptr);

    auto plate = createDspEffect(DspModuleType::PLATE_REVERB);
    REQUIRE(dynamic_cast<PlateReverbEffect*>(plate.get()) != nullptr);

    auto trem = createDspEffect(DspModuleType::TREMOLO);
    REQUIRE(dynamic_cast<TremoloEffect*>(trem.get()) != nullptr);

    auto flang = createDspEffect(DspModuleType::FLANGER);
    REQUIRE(dynamic_cast<FlangerEffect*>(flang.get()) != nullptr);

    auto pitch = createDspEffect(DspModuleType::PITCH_SHIFTER);
    REQUIRE(dynamic_cast<PitchShifterEffect*>(pitch.get()) != nullptr);

    auto retime = createDspEffect(DspModuleType::RETIME);
    REQUIRE(dynamic_cast<ReTimeEffect*>(retime.get()) != nullptr);
}

TEST_CASE("DspEffectFactory returns nullptr for BYPASS / unknown ids", "[factory]")
{
    REQUIRE(createDspEffect(DspModuleType::BYPASS) == nullptr);
    REQUIRE(createDspEffect(static_cast<DspModuleType>(255)) == nullptr);
}
