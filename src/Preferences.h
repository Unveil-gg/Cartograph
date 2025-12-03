#pragma once

#include "UI/Modals.h"  // For ProjectSortOrder enum
#include <string>

namespace Cartograph {

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
};

} // namespace Cartograph

