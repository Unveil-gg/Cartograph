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

std::string GetAssetsDir() {
#ifdef __APPLE__
    // When running as .app bundle, assets are in Resources/
    char* base = SDL_GetBasePath();
    if (base) {
        std::string path = std::string(base) + "../Resources/assets/";
        SDL_free(base);
        if (fs::exists(path)) {
            return path;
        }
    }
#endif
    // Fallback: relative to executable
    char* base = SDL_GetBasePath();
    if (base) {
        std::string path = std::string(base) + "assets/";
        SDL_free(base);
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

