#pragma once

#include <cstddef>
#include <array>

constexpr int GridSize = 512;
constexpr int TotalCells = GridSize * GridSize;
constexpr int DirtyRowWordCount = GridSize / 64;
using DirtyRowMask = std::array<uint64_t, DirtyRowWordCount>;
static_assert(GridSize % 64 == 0 && GridSize > 1, "GridSize must be divisible by 64");
static_assert(TotalCells <= 262144, "TotalCells exceeds 512x512 limit");
constexpr int PedalSlotCount = 6;
constexpr int KnobsPerPedal = 4;
constexpr int TotalKnobs = PedalSlotCount * KnobsPerPedal;
constexpr float PedalGainMinDb = -32.0f;
constexpr float PedalGainMaxDb = 6.0f;
constexpr float PedalGainMin = 0.0251189f;
constexpr float PedalGainMax = 1.9952623f;

constexpr size_t MaxReverbDelayMs = 5000;
constexpr double MaxGranularDurationSec = 10.0;
constexpr size_t MaxSimpleDelaySec = 2;

constexpr int EnvelopeSliceCount = 128;
