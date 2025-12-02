#include "System.h"
#include <SDL3/SDL.h>

namespace Platform {

std::string GetModifierKeyName() {
#ifdef __APPLE__
    return "Cmd";
#else
    return "Ctrl";
#endif
}

std::string FormatShortcut(const std::string& keys) {
    std::string modifier = GetModifierKeyName();
    return modifier + "+" + keys;
}

bool OpenURL(const std::string& url) {
    return SDL_OpenURL(url.c_str());
}

std::string GetPlatformName() {
#ifdef __APPLE__
    return "macOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

} // namespace Platform

