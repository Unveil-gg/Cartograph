#include "Paths.h"
#include <SDL3/SDL.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace Platform {

std::string GetUserDataDir() {
#ifdef __APPLE__
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + 
            "/Library/Application Support/Unveil Cartograph/";
    }
#elif defined(_WIN32)
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\Unveil\\Cartograph\\";
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.local/share/unveil-cartograph/";
    }
#endif
    return "./userdata/";
}

std::string GetAutosaveDir() {
    return GetUserDataDir() + "Autosave/";
}

std::string GetDefaultProjectsDir() {
#ifdef __APPLE__
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/Documents/Cartograph/";
    }
#elif defined(_WIN32)
    const char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        return std::string(userprofile) + "\\Documents\\Cartograph\\";
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/Documents/Cartograph/";
    }
#endif
    return "./projects/";
}

std::string GetAssetsDir() {
#ifdef __APPLE__
    // When running as .app bundle, assets are in Resources/
    const char* base = SDL_GetBasePath();
    if (base) {
        std::string path = std::string(base) + "../Resources/assets/";
        if (fs::exists(path)) {
            return path;
        }
    }
#endif
    // Fallback: relative to executable
    const char* basePath = SDL_GetBasePath();
    if (basePath) {
        std::string path = std::string(basePath) + "assets/";
        return path;
    }
    return "./assets/";
}

bool EnsureDirectoryExists(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return fs::create_directories(path);
        }
        return fs::is_directory(path);
    } catch (...) {
        return false;
    }
}

} // namespace Platform

