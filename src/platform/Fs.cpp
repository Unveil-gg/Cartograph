#include "Fs.h"
#include "../Config.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace Platform {

std::optional<std::string> ShowOpenFileDialog(
    const std::string& /*title*/,
    const std::vector<std::pair<std::string, std::string>>& /*filters*/
) {
    // Fallback stub - platform-specific implementations in:
    // - platform/macos/Fs.mm (macOS)
    // - platform/windows/Fs.cpp (Windows)
    return std::nullopt;
}

std::optional<std::string> ShowSaveFileDialog(
    const std::string& /*title*/,
    const std::string& /*defaultName*/,
    const std::vector<std::pair<std::string, std::string>>& /*filters*/
) {
    // Fallback stub - see platform-specific implementations
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> ReadFile(const std::string& path) {
    std::error_code ec;
    const auto fileSize = fs::file_size(path, ec);
    if (ec || fileSize > Cartograph::Limits::MAX_PROJECT_JSON_SIZE) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (fileSize > 0 &&
        !file.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(fileSize))) {
        return std::nullopt;
    }

    return buffer;
}

bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

std::optional<std::string> ReadTextFile(const std::string& path) {
    std::error_code ec;
    const auto fileSize = fs::file_size(path, ec);
    if (ec || fileSize > Cartograph::Limits::MAX_PROJECT_JSON_SIZE) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string content(static_cast<size_t>(fileSize), '\0');
    if (fileSize > 0 &&
        !file.read(content.data(),
                   static_cast<std::streamsize>(fileSize))) {
        return std::nullopt;
    }
    return content;
}

bool WriteTextFile(const std::string& path, const std::string& text) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << text;
    return file.good();
}

bool FileExists(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

} // namespace Platform

