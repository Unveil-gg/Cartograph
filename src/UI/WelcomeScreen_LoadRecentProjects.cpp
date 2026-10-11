#include "WelcomeScreen.h"
#include "../UI.h"
#include "../App.h"
#include "../Model.h"
#include "../Canvas.h"
#include "../History.h"
#include "../Jobs.h"
#include "../Icons.h"
#include "../Keymap.h"
#include "../Package.h"
#include "../ProjectFolder.h"
#include "../Preferences.h"
#include "../platform/Paths.h"
#include "../platform/Fs.h"
#include "../platform/Time.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

// minizip-ng for reading .cart thumbnails
#include "unzip.h"

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// STB image for loading thumbnails (implementation in Icons.cpp)
#include "../../third_party/stb/stb_image.h"

namespace Cartograph {



/**
 * Extract thumbnail from a .cart file to cache directory.
 * @param cartPath Path to .cart file
 * @return Path to extracted thumbnail, or empty if extraction failed
 */
static std::string ExtractCartThumbnail(const std::string& cartPath) {
    namespace fs = std::filesystem;
    
    // Create cache directory for thumbnails
    std::string cacheDir = Platform::GetUserDataDir() + "thumbnail_cache/";
    Platform::EnsureDirectoryExists(cacheDir);
    
    // Generate cache filename based on cart file path hash
    // (Simple approach: use filename + modification time)
    fs::path cartFsPath(cartPath);
    std::string baseName = cartFsPath.stem().string();
    
    // Get file modification time for cache invalidation
    std::error_code ec;
    auto modTime = fs::last_write_time(cartPath, ec);
    if (ec) return "";
    
    auto modTimeVal = static_cast<long long>(
        modTime.time_since_epoch().count());
    std::string cacheFileName = baseName + "_" + 
        std::to_string(modTimeVal) + ".png";
    std::string cachePath = cacheDir + cacheFileName;
    
    // Check if cached thumbnail already exists and is valid
    if (fs::exists(cachePath)) {
        return cachePath;
    }
    
    // Extract thumb.png from .cart ZIP
    unzFile uf = unzOpen(cartPath.c_str());
    if (!uf) return "";
    
    // Try to locate thumb.png
    if (unzLocateFile(uf, "thumb.png", 0) != UNZ_OK) {
        unzClose(uf);
        return "";
    }
    
    // Get file info
    unz_file_info fileInfo;
    if (unzGetCurrentFileInfo(uf, &fileInfo, nullptr, 0, 
                              nullptr, 0, nullptr, 0) != UNZ_OK) {
        unzClose(uf);
        return "";
    }
    
    // Open and read the file
    if (unzOpenCurrentFile(uf) != UNZ_OK) {
        unzClose(uf);
        return "";
    }
    
    std::vector<uint8_t> buffer(fileInfo.uncompressed_size);
    int bytesRead = unzReadCurrentFile(uf, buffer.data(), buffer.size());
    unzCloseCurrentFile(uf);
    unzClose(uf);
    
    if (bytesRead <= 0) return "";
    
    // Write to cache file
    std::ofstream outFile(cachePath, std::ios::binary);
    if (!outFile.is_open()) return "";
    
    outFile.write(reinterpret_cast<const char*>(buffer.data()), bytesRead);
    outFile.close();
    
    return cachePath;
}


/**
 * Create RecentProject entry from a project folder path.
 * @param folderPath Path to project folder
 * @param lastModified Optional last modified string (empty = use file time)
 * @return Populated RecentProject entry
 */
static RecentProject CreateProjectEntryFromFolder(
    const std::string& folderPath,
    const std::string& lastModified = ""
) {
    namespace fs = std::filesystem;
            
    RecentProject project;
    project.path = folderPath;
    
    // Get folder name using helper (handles trailing slashes)
    std::string folderName = ProjectFolder::GetFolderNameFromPath(folderPath);
    project.name = folderName;  // Default to folder name
            
    // Set thumbnail path if preview.png exists
    fs::path p(folderPath);
    fs::path previewPath = p / "preview.png";
    if (fs::exists(previewPath)) {
        project.thumbnailPath = previewPath.string();
    }
            
    // Read title and description from project.json
    fs::path projectJsonPath = p / "project.json";
    if (fs::exists(projectJsonPath)) {
        try {
            std::ifstream file(projectJsonPath);
            if (file.is_open()) {
                nlohmann::json j = nlohmann::json::parse(file);
                if (j.contains("meta")) {
                    // Use meta.title as display name if available
                    if (j["meta"].contains("title")) {
                        std::string title = 
                            j["meta"]["title"].get<std::string>();
                        if (!title.empty()) {
                            project.name = title;
                        }
                    }
                    // Also read description
                    if (j["meta"].contains("description")) {
                        project.description = 
                            j["meta"]["description"].get<std::string>();
                    }
                }
            }
        } catch (...) {
            // Silently ignore parse errors - folder name fallback used
        }
    }
    
    // Set last modified time
    if (!lastModified.empty()) {
        project.lastModified = lastModified;
    } else {
        // Get from filesystem
        std::error_code ec;
        auto modTime = fs::last_write_time(folderPath, ec);
        if (!ec) {
            auto sctp = std::chrono::time_point_cast<
                std::chrono::system_clock::duration>(
                modTime - fs::file_time_type::clock::now() + 
                std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm = *std::localtime(&time_t);
            char timeStr[64];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tm);
            project.lastModified = timeStr;
        }
    }
    
    return project;
}


void WelcomeScreen::LoadRecentProjects() {
    recentProjects.clear();
    
    namespace fs = std::filesystem;
    
    // Track normalized paths we've already added (for deduplication)
    std::unordered_set<std::string> addedPaths;
    
    // 1. Load from persistent recent projects list
    auto entries = RecentProjects::GetValidEntries();
    
    for (const auto& entry : entries) {
        // Normalize path for deduplication
        std::string normalizedPath = Platform::NormalizePath(entry.path);
        
        // Skip if already added (handles duplicates from storage)
        if (addedPaths.count(normalizedPath) > 0) continue;
        
        RecentProject project;
        project.path = entry.path;
        
        bool isCartFile = (entry.type == "cart");
        
        if (isCartFile) {
            // .cart file - extract name from filename
            fs::path p(entry.path);
            project.name = p.stem().string();
            
            // Extract thumbnail from .cart to cache
            project.thumbnailPath = ExtractCartThumbnail(entry.path);
            
            project.description = "";
            
        } else {
            // Project folder
            project = CreateProjectEntryFromFolder(entry.path, entry.lastOpened);
        }
        
        project.lastModified = entry.lastOpened;
        recentProjects.push_back(project);
        addedPaths.insert(normalizedPath);
    }
    
    // 2. Scan default projects directory for additional projects
    // Looks for both .cartproj folders and legacy plain folders
    std::string projectsDir = Platform::GetDefaultProjectsDir();
    
    if (fs::exists(projectsDir) && fs::is_directory(projectsDir)) {
        try {
            for (const auto& dirEntry : fs::directory_iterator(projectsDir)) {
                if (!dirEntry.is_directory()) continue;
                
                std::string folderPath = dirEntry.path().string();
                std::string normalizedPath = Platform::NormalizePath(folderPath);
                
                // Skip if already in recent list (using normalized path)
                if (addedPaths.count(normalizedPath) > 0) continue;
                
                // Use proper validation (checks project.json + Cartograph format)
                if (!ProjectFolder::IsProjectFolder(folderPath)) continue;
                
                // Add to list
                RecentProject project = CreateProjectEntryFromFolder(folderPath);
                recentProjects.push_back(project);
                addedPaths.insert(normalizedPath);
            }
        } catch (...) {
            // Silently ignore scan errors
        }
    }
    
    // 3. Sort by last modified (most recent first)
    std::sort(recentProjects.begin(), recentProjects.end(),
        [](const RecentProject& a, const RecentProject& b) {
            return a.lastModified > b.lastModified;
        });
}



void WelcomeScreen::AddRecentProject(const std::string& path) {
    // Add to persistent recent projects list
    RecentProjects::Add(path);
}



void WelcomeScreen::LoadThumbnailTexture(RecentProject& project) {
    // Skip if already loaded
    if (project.thumbnailLoaded) {
        return;
    }
    
    namespace fs = std::filesystem;
    
    // Check if thumbnail path exists
    if (project.thumbnailPath.empty() || 
        !fs::exists(project.thumbnailPath)) {
        // Use placeholder
        if (placeholderTexture == 0) {
            placeholderTexture = GeneratePlaceholderTexture();
        }
        project.thumbnailTextureId = placeholderTexture;
        project.thumbnailLoaded = true;
        return;
    }
    
    // Load image from file
    int width, height, channels;
    unsigned char* data = stbi_load(
        project.thumbnailPath.c_str(),
        &width, &height, &channels, 4  // Force RGBA
    );
    
    if (!data) {
        // Failed to load - use placeholder
        if (placeholderTexture == 0) {
            placeholderTexture = GeneratePlaceholderTexture();
        }
        project.thumbnailTextureId = placeholderTexture;
        project.thumbnailLoaded = true;
        return;
    }
    
    // Create OpenGL texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );
    
    // Unbind and cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    
    project.thumbnailTextureId = texId;
    project.thumbnailLoaded = true;
}



void WelcomeScreen::UnloadThumbnailTextures() {
    // Unload all thumbnail textures
    for (auto& project : recentProjects) {
        if (project.thumbnailLoaded && 
            project.thumbnailTextureId != 0 &&
            project.thumbnailTextureId != placeholderTexture) {
            GLuint texId = project.thumbnailTextureId;
            glDeleteTextures(1, &texId);
            project.thumbnailTextureId = 0;
            project.thumbnailLoaded = false;
        }
    }
    
    // Delete placeholder texture
    if (placeholderTexture != 0) {
        GLuint texId = placeholderTexture;
        glDeleteTextures(1, &texId);
        placeholderTexture = 0;
    }
}



} // namespace Cartograph
