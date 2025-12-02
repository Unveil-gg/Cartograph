#pragma once

#include <string>

/**
 * Platform-specific system utilities.
 */
namespace Platform {

/**
 * Get the name of the primary modifier key for the current platform.
 * @return "Cmd" on macOS, "Ctrl" on Windows/Linux
 */
std::string GetModifierKeyName();

/**
 * Format a keyboard shortcut string with platform-appropriate modifier.
 * @param keys Shortcut keys (e.g., "N", "Shift+S")
 * @return Formatted shortcut (e.g., "Cmd+N" on macOS, "Ctrl+N" on Windows)
 */
std::string FormatShortcut(const std::string& keys);

/**
 * Open a URL in the system's default browser.
 * @param url URL to open
 * @return true on success
 */
bool OpenURL(const std::string& url);

/**
 * Get the platform name for display purposes.
 * @return "macOS", "Windows", or "Linux"
 */
std::string GetPlatformName();

} // namespace Platform

