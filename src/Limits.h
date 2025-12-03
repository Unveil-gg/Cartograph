#pragma once

#include <cstddef>

namespace Cartograph {

/**
 * Security limits for file loading and input validation.
 * These constants prevent malicious files from causing memory exhaustion
 * or other denial-of-service conditions.
 */
namespace Limits {

// Maximum JSON file size before parsing (50 MB)
constexpr size_t MAX_PROJECT_JSON_SIZE = 50 * 1024 * 1024;

// Grid dimension limits (prevents integer overflow in pixel calculations)
constexpr int MAX_GRID_DIMENSION = 10000;
constexpr int MIN_GRID_DIMENSION = 1;
constexpr int MAX_TILE_SIZE = 256;
constexpr int MIN_TILE_SIZE = 1;

// Collection size limits from JSON (prevents OOM from malicious files)
constexpr size_t MAX_TILE_ROWS = 1000000;      // 1M tile rows
constexpr size_t MAX_MARKERS = 100000;         // 100K markers
constexpr size_t MAX_ROOMS = 10000;            // 10K rooms
constexpr size_t MAX_REGION_GROUPS = 10000;    // 10K region groups
constexpr size_t MAX_EDGES = 1000000;          // 1M edges
constexpr size_t MAX_DOORS = 100000;           // 100K doors
constexpr size_t MAX_PALETTE_ENTRIES = 10000;  // 10K palette colors
constexpr size_t MAX_CELL_ASSIGNMENTS = 10000000;  // 10M cell assignments

}  // namespace Limits
}  // namespace Cartograph

