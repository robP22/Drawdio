#pragma once

#include <cstddef>

constexpr int GridSize = 256;
constexpr int TotalCells = GridSize * GridSize;
constexpr int PedalSlotCount = 6;
constexpr int KnobsPerPedal = 4;
constexpr int TotalKnobs = PedalSlotCount * KnobsPerPedal;

constexpr size_t MaxReverbDelayMs = 5000;
constexpr double MaxGranularDurationSec = 10.0;
constexpr size_t MaxSimpleDelaySec = 2;
