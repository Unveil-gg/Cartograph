#pragma once

#include <cstdint>

namespace Cartograph {

// Coalescing thresholds for command merging
static const uint64_t COALESCE_TIME_MS = 150;
static const float COALESCE_DIST_SQ = 16.0f;  // 4 tiles squared
static const uint64_t PROPERTY_COALESCE_TIME_MS = 300;  // Longer for typing

} // namespace Cartograph

