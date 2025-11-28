#pragma once

#include <string>

namespace Platform {

/**
 * Get the user data directory for persistent storage.
 * macOS: ~/Library/Application Support/Unveil Cartograph/
 * Windows: %APPDATA%\Unveil\Cartograph\
 * @return Path to user data directory
 */
std::string GetUserDataDir();

/**
 * Get the autosave directory path.
 * @return Path to autosave directory
 */
std::string GetAutosaveDir();

/**
 * Get the default projects directory (where new projects are created).
 * macOS: ~/Documents/Cartograph/
 * Windows: %USERPROFILE%\Documents\Cartograph\
 * Linux: ~/Documents/Cartograph/
 * @return Path to default projects directory
 */
std::string GetDefaultProjectsDir();

/**
 * Get the application assets directory.
 * @return Path to assets directory
 */
std::string GetAssetsDir();

/**
 * Ensure a directory exists, creating it if necessary.
 * @param path Directory path
 * @return true if directory exists or was created successfully
 */
bool EnsureDirectoryExists(const std::string& path);

} // namespace Platform

