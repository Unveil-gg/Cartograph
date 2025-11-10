#include "Time.h"
#include <SDL3/SDL.h>
#include <chrono>

namespace Platform {

double GetTime() {
    return SDL_GetTicks() / 1000.0;
}

uint64_t GetTimestampMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

} // namespace Platform

