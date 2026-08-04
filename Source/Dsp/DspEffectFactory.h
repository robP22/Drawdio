#pragma once
#include <memory>
#include "Core/DspModuleType.h"
#include "Effects/DspEffect.h"

std::unique_ptr<DspEffect> createDspEffect(DspModuleType type);
