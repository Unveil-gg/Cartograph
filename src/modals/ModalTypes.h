#pragma once

#include "../Model.h"
#include <string>

namespace Cartograph {

/**
 * Sort order options for project browser modal.
 */
enum class ProjectSortOrder {
    MostRecent,    // Newest first (default)
    OldestFirst,   // Oldest first
    AtoZ,          // Alphabetical A-Z
    ZtoA           // Alphabetical Z-A
};

/**
 * Project template presets (for new project modal).
 */
enum class ProjectTemplate {
    Custom,      // User-defined settings
    Small,       // 128x128, 16px cells
    Medium,      // 256x256, 16px cells
    Large,       // 512x512, 16px cells
    Metroidvania // 256x256, 8px cells (detailed)
};

/**
 * New project configuration (for new project modal).
 */
struct NewProjectConfig {
    char projectName[256] = "New Map";
    GridPreset gridPreset = GridPreset::Square;
    int mapWidth = 256;
    int mapHeight = 256;
    std::string saveDirectory = "";
    std::string fullSavePath = "";
};

} // namespace Cartograph
