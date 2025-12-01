#pragma once

#include <string>
#include <vector>
#include <optional>

namespace Platform {

/**
 * Show native open file dialog.
 * @param title Dialog title
 * @param filters File type filters (e.g., {"Cart Files", "*.cart"})
 * @return Selected file path, or nullopt if cancelled
 */
std::optional<std::string> ShowOpenFileDialog(
    const std::string& title,
    const std::vector<std::pair<std::string, std::string>>& filters = {}
);

/**
 * Show native open dialog for files or folders.
 * Used for importing projects which can be either .cart files or folders.
 * @param title Dialog title
 * @param allowFiles Allow file selection
 * @param allowFolders Allow folder selection
 * @param fileExtensions File extensions to filter (e.g., {"cart"})
 * @param defaultPath Default directory to open in
 * @return Selected file or folder path, or nullopt if cancelled
 */
std::optional<std::string> ShowOpenDialogForImport(
    const std::string& title,
    bool allowFiles,
    bool allowFolders,
    const std::vector<std::string>& fileExtensions = {},
    const std::string& defaultPath = ""
);

/**
 * Show native save file dialog.
 * @param title Dialog title
 * @param defaultName Default filename
 * @param filters File type filters
 * @return Selected file path, or nullopt if cancelled
 */
std::optional<std::string> ShowSaveFileDialog(
    const std::string& title,
    const std::string& defaultName = "",
    const std::vector<std::pair<std::string, std::string>>& filters = {}
);

/**
 * Read entire file into memory.
 * @param path File path
 * @return File contents, or nullopt on error
 */
std::optional<std::vector<uint8_t>> ReadFile(const std::string& path);

/**
 * Write data to file.
 * @param path File path
 * @param data Data to write
 * @return true on success
 */
bool WriteFile(const std::string& path, const std::vector<uint8_t>& data);

/**
 * Read text file.
 * @param path File path
 * @return File contents as string, or nullopt on error
 */
std::optional<std::string> ReadTextFile(const std::string& path);

/**
 * Write text file.
 * @param path File path
 * @param text Text to write
 * @return true on success
 */
bool WriteTextFile(const std::string& path, const std::string& text);

/**
 * Check if file exists.
 * @param path File path
 * @return true if file exists
 */
bool FileExists(const std::string& path);

} // namespace Platform

