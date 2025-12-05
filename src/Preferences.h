#pragma once

#include "UI/Modals.h"  // For ProjectSortOrder enum
#include <string>
#include <vector>

namespace Cartograph {

// Default theme name constant
static const std::string kDefaultThemeName = "Dark";

/**
 * Entry in the recently opened projects list.
 * Stored in recent_projects.json.
 */
struct RecentProjectEntry {
    std::string path;        // Full path to .cart file or .cartproj folder
    std::string type;        // "cart" or "folder"
    std::string lastOpened;  // ISO 8601 timestamp
};

/**
 * Global application preferences (persisted to user data directory).
 * Handles loading/saving of user preferences like sort order.
 */
class Preferences {
public:
    /**
     * Load preferences from disk.
     * Called once at app startup.
     */
    static void Load();
    
    /**
     * Save preferences to disk.
     * Called when preferences change.
     */
    static void Save();
    
    // Preference values
    static ProjectSortOrder projectBrowserSortOrder;
    static std::string themeName;  // Active theme name
    static float uiScale;          // UI scale factor
};

/**
 * Recently opened projects list (persisted separately).
 * Tracks projects opened from any location.
 */
class RecentProjects {
public:
    // Maximum number of recent projects to track
    static constexpr size_t MAX_RECENT = 20;
    
    /**
     * Load recent projects from disk.
     * Called once at app startup.
     */
    static void Load();
    
    /**
     * Save recent projects to disk.
     * Called when list changes.
     */
    static void Save();
    
    /**
     * Add or update a project in the recent list.
     * Moves existing entries to top, prunes old entries.
     * @param path Full path to project file or folder
     */
    static void Add(const std::string& path);
    
    /**
     * Remove a project from the recent list.
     * @param path Path to remove
     */
    static void Remove(const std::string& path);
    
    /**
     * Clear all recent projects.
     */
    static void Clear();
    
    /**
     * Get list of recent projects (filtered for existence).
     * @return Vector of entries where files still exist
     */
    static std::vector<RecentProjectEntry> GetValidEntries();
    
private:
    static std::vector<RecentProjectEntry> s_entries;
    static bool s_loaded;
};

} // namespace Cartograph

