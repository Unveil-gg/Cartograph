#include "Preferences.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace Cartograph {

// Initialize static members with defaults
ProjectSortOrder Preferences::projectBrowserSortOrder = ProjectSortOrder::MostRecent;
std::string Preferences::themeName = kDefaultThemeName;
float Preferences::uiScale = 1.0f;

// RecentProjects static members
std::vector<RecentProjectEntry> RecentProjects::s_entries;
bool RecentProjects::s_loaded = false;

// ============================================================================
// Preferences implementation
// ============================================================================

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
        
        // Theme preferences
        if (j.contains("themeName")) {
            themeName = j["themeName"].get<std::string>();
        }
        if (j.contains("uiScale")) {
            uiScale = j["uiScale"].get<float>();
            // Clamp to valid range
            if (uiScale < 0.5f) uiScale = 0.5f;
            if (uiScale > 2.0f) uiScale = 2.0f;
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
    
    // Theme preferences
    j["themeName"] = themeName;
    j["uiScale"] = uiScale;
    
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

// ============================================================================
// RecentProjects implementation
// ============================================================================

/**
 * Get current timestamp in ISO 8601 format.
 */
static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);
    
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

/**
 * Determine project type from path.
 * @return "cart" for .cart files, "folder" for directories/.cartproj
 */
static std::string GetProjectType(const std::string& path) {
    if (path.size() >= 5 && 
        path.substr(path.size() - 5) == ".cart") {
        return "cart";
    }
    // Both .cartproj and plain folders are "folder" type
    return "folder";
}

void RecentProjects::Load() {
    s_entries.clear();
    s_loaded = true;
    
    std::string path = Platform::GetUserDataDir() + "recent_projects.json";
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return;  // No recent projects file yet
    }
    
    try {
        nlohmann::json j = nlohmann::json::parse(file);
        
        if (j.contains("recent") && j["recent"].is_array()) {
            for (const auto& entry : j["recent"]) {
                RecentProjectEntry rpe;
                
                if (entry.contains("path")) {
                    rpe.path = entry["path"].get<std::string>();
                }
                if (entry.contains("type")) {
                    rpe.type = entry["type"].get<std::string>();
                }
                if (entry.contains("lastOpened")) {
                    rpe.lastOpened = entry["lastOpened"].get<std::string>();
                }
                
                if (!rpe.path.empty()) {
                    s_entries.push_back(rpe);
                }
            }
        }
    } catch (...) {
        // Parse error - start fresh
        s_entries.clear();
    }
}

void RecentProjects::Save() {
    nlohmann::json j;
    j["version"] = 1;
    
    nlohmann::json recentArray = nlohmann::json::array();
    for (const auto& entry : s_entries) {
        nlohmann::json e;
        e["path"] = entry.path;
        e["type"] = entry.type;
        e["lastOpened"] = entry.lastOpened;
        recentArray.push_back(e);
    }
    j["recent"] = recentArray;
    
    // Ensure directory exists
    std::string dir = Platform::GetUserDataDir();
    Platform::EnsureDirectoryExists(dir);
    
    // Write to file
    std::string path = dir + "recent_projects.json";
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
    }
}

void RecentProjects::Add(const std::string& path) {
    if (!s_loaded) {
        Load();
    }
    
    // Normalize path for consistent comparison and storage
    std::string normalizedPath = Platform::NormalizePath(path);
    
    // Remove existing entry with same normalized path (deduplication)
    s_entries.erase(
        std::remove_if(s_entries.begin(), s_entries.end(),
            [&normalizedPath](const RecentProjectEntry& e) {
                return Platform::NormalizePath(e.path) == normalizedPath;
            }),
        s_entries.end()
    );
    
    // Create new entry with normalized path
    RecentProjectEntry entry;
    entry.path = normalizedPath;
    entry.type = GetProjectType(normalizedPath);
    entry.lastOpened = GetCurrentTimestamp();
    
    // Insert at front (most recent)
    s_entries.insert(s_entries.begin(), entry);
    
    // Prune to max size
    if (s_entries.size() > MAX_RECENT) {
        s_entries.resize(MAX_RECENT);
    }
    
    // Save immediately
    Save();
}

void RecentProjects::Remove(const std::string& path) {
    if (!s_loaded) {
        Load();
    }
    
    // Normalize path for consistent comparison
    std::string normalizedPath = Platform::NormalizePath(path);
    
    s_entries.erase(
        std::remove_if(s_entries.begin(), s_entries.end(),
            [&normalizedPath](const RecentProjectEntry& e) {
                return Platform::NormalizePath(e.path) == normalizedPath;
            }),
        s_entries.end()
    );
    
    Save();
}

void RecentProjects::Clear() {
    s_entries.clear();
    s_loaded = true;
    Save();
}

std::vector<RecentProjectEntry> RecentProjects::GetValidEntries() {
    if (!s_loaded) {
        Load();
    }
    
    namespace fs = std::filesystem;
    std::vector<RecentProjectEntry> valid;
    
    for (const auto& entry : s_entries) {
        // Check if file/folder still exists
        if (fs::exists(entry.path)) {
            valid.push_back(entry);
        }
    }
    
    return valid;
}

} // namespace Cartograph

