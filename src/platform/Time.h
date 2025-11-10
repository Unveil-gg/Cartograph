#pragma once

#include <cstdint>

namespace Platform {

/**
 * Get current time in seconds since application start.
 * @return Time in seconds with high precision
 */
double GetTime();

/**
 * Get current timestamp in milliseconds since epoch.
 * @return Milliseconds since epoch
 */
uint64_t GetTimestampMs();

} // namespace Platform

