#include "Fs.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace Platform {

std::optional<std::string> ShowOpenFileDialog(
    const std::string& title,
    const std::vector<std::pair<std::string, std::string>>& filters
) {
    // TODO: Implement native file dialogs using SDL3 or platform APIs
    // For now, return nullopt (will need platform-specific implementation)
    return std::nullopt;
}

std::optional<std::string> ShowSaveFileDialog(
    const std::string& title,
    const std::string& defaultName,
    const std::vector<std::pair<std::string, std::string>>& filters
) {
    // TODO: Implement native file dialogs
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
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
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
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

