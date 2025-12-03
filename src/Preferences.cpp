#include "Preferences.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace Cartograph {

// Initialize static members with defaults
ProjectSortOrder Preferences::projectBrowserSortOrder = ProjectSortOrder::MostRecent;

void Preferences::Load() {
    std::string path = Platform::GetUserDataDir() + "preferences.json";
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return;  // No preferences file yet - use defaults
    }
    
    try {
        nlohmann::json j = nlohmann::json::parse(file);
        
        if (j.contains("projectBrowserSortOrder")) {
            std::string val = j["projectBrowserSortOrder"].get<std::string>();
            if (val == "OldestFirst") {
                projectBrowserSortOrder = ProjectSortOrder::OldestFirst;
            } else if (val == "AtoZ") {
                projectBrowserSortOrder = ProjectSortOrder::AtoZ;
            } else if (val == "ZtoA") {
                projectBrowserSortOrder = ProjectSortOrder::ZtoA;
            } else {
                projectBrowserSortOrder = ProjectSortOrder::MostRecent;
            }
        }
    } catch (...) {
        // Parse error - use defaults
    }
}

void Preferences::Save() {
    nlohmann::json j;
    
    // Convert sort order to string
    const char* sortStr = "MostRecent";
    switch (projectBrowserSortOrder) {
        case ProjectSortOrder::OldestFirst:
            sortStr = "OldestFirst";
            break;
        case ProjectSortOrder::AtoZ:
            sortStr = "AtoZ";
            break;
        case ProjectSortOrder::ZtoA:
            sortStr = "ZtoA";
            break;
        default:
            break;
    }
    j["projectBrowserSortOrder"] = sortStr;
    
    // Ensure directory exists
    std::string dir = Platform::GetUserDataDir();
    Platform::EnsureDirectoryExists(dir);
    
    // Write to file
    std::string path = dir + "preferences.json";
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
    }
}

} // namespace Cartograph

