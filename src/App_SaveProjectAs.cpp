#include "App.h"
#include "render/GlRenderer.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include "platform/Time.h"
#include "IOJson.h"
#include "Package.h"
#include "ProjectFolder.h"
#include "ExportPng.h"
#include "Thumbnail.h"
#include "Preferences.h"
#include <algorithm>
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include <filesystem>

// SDL resource deleters implementation (outside namespace)


namespace Cartograph {


void App::SaveProjectAs(const std::string& path) {
    bool success = false;
    
    // Use cached thumbnail from last render (simpler and more reliable)
    std::vector<uint8_t> thumbnailPixels;
    int thumbWidth = 0, thumbHeight = 0;
    
    if (m_canvas.hasCachedThumbnail) {
        thumbnailPixels = m_canvas.cachedThumbnail;
        thumbWidth = m_canvas.cachedThumbnailWidth;
        thumbHeight = m_canvas.cachedThumbnailHeight;
    }
    
    // Detect save format and save accordingly
    if (Package::HasCartExtension(path)) {
        // Save as .cart package (ZIP with embedded icons)
        success = Package::Save(m_model, path, &m_icons,
                               thumbnailPixels.empty() ? nullptr : 
                                   thumbnailPixels.data(),
                               thumbWidth, thumbHeight);
    } else {
        // Save as project folder (git-friendly format)
        success = ProjectFolder::Save(m_model, path, &m_icons,
                                     thumbnailPixels.empty() ? nullptr : 
                                         thumbnailPixels.data(),
                                     thumbWidth, thumbHeight);
    }
    
    if (success) {
        m_currentFilePath = path;
        m_model.ClearDirty();
        CleanupAutosave();  // Remove autosave after successful manual save
        UpdateWindowTitle();
        m_ui.m_welcomeScreen.AddRecentProject(path);
        
        // Extract project name from path for console message
        std::string projectName = std::filesystem::path(path)
            .filename().string();
        if (projectName.empty()) {
            projectName = std::filesystem::path(path)
                .parent_path().filename().string();
        }
        m_ui.ShowToast("Saved: " + projectName, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to save: " + path, Toast::Type::Error);
    }
}


void App::SaveProjectFolder(const std::string& folderPath) {
    // Use cached thumbnail from last render (simpler and more reliable)
    std::vector<uint8_t> thumbnailPixels;
    int thumbWidth = 0, thumbHeight = 0;
    
    if (m_canvas.hasCachedThumbnail) {
        thumbnailPixels = m_canvas.cachedThumbnail;
        thumbWidth = m_canvas.cachedThumbnailWidth;
        thumbHeight = m_canvas.cachedThumbnailHeight;
    }
    
    bool success = ProjectFolder::Save(m_model, folderPath, &m_icons,
                                      thumbnailPixels.empty() ? nullptr : 
                                          thumbnailPixels.data(),
                                      thumbWidth, thumbHeight);
    
    if (success) {
        m_currentFilePath = folderPath;
        m_model.ClearDirty();
        CleanupAutosave();
        UpdateWindowTitle();
        m_ui.m_welcomeScreen.AddRecentProject(folderPath);
        
        // Extract project name from path
        std::string projectName = std::filesystem::path(folderPath)
            .filename().string();
        if (projectName.empty()) {
            projectName = std::filesystem::path(folderPath)
                .parent_path().filename().string();
        }
        m_ui.ShowToast("Saved: " + projectName, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to save project folder: " + folderPath, 
                      Toast::Type::Error);
    }
}


bool App::RenameProjectFolder(const std::string& newTitle) {
    namespace fs = std::filesystem;
    
    // Must have a current file path (project folder)
    if (m_currentFilePath.empty()) {
        m_ui.ShowToast("No project folder to rename", Toast::Type::Error);
        return false;
    }
    
    // Only works for project folders, not .cart files
    if (Package::HasCartExtension(m_currentFilePath)) {
        m_ui.ShowToast("Cannot rename .cart files this way", 
                      Toast::Type::Error);
        return false;
    }
    
    // Sanitize the new title for filesystem use
    std::string sanitizedName = ProjectFolder::SanitizeProjectName(newTitle);
    
    // Preserve .cartproj extension if current path has it
    bool hasCartprojExt = ProjectFolder::HasCartprojExtension(m_currentFilePath);
    
    if (sanitizedName.empty()) {
        m_ui.ShowToast("Project name cannot be empty", Toast::Type::Error);
        return false;
    }
    
    // Explicitly strip trailing slashes from path string
    // (lexically_normal() doesn't reliably remove them on all platforms)
    std::string pathStr = m_currentFilePath;
    while (!pathStr.empty() && (pathStr.back() == '/' || pathStr.back() == '\\')) {
        pathStr.pop_back();
    }
    
    if (pathStr.empty()) {
        m_ui.ShowToast("Invalid project path", Toast::Type::Error);
        return false;
    }
    
    // Now construct paths - parent_path() will work correctly
    fs::path currentPath(pathStr);
    fs::path parentDir = currentPath.parent_path();
    
    // Preserve .cartproj extension
    std::string newFolderName = sanitizedName;
    if (hasCartprojExt) {
        newFolderName += CARTPROJ_EXTENSION;
    }
    fs::path newPath = parentDir / newFolderName;
    
    // Get strings for comparison
    std::string currentNorm = currentPath.string();
    std::string newNorm = newPath.string();
    
    // Check if the name is actually different
    // Use case-insensitive comparison on macOS/Windows (case-preserving filesystems)
#if defined(__APPLE__) || defined(_WIN32)
    std::string currentLower = currentNorm;
    std::string newLower = newNorm;
    std::transform(currentLower.begin(), currentLower.end(), 
                   currentLower.begin(), ::tolower);
    std::transform(newLower.begin(), newLower.end(), 
                   newLower.begin(), ::tolower);
    if (currentLower == newLower) {
        // Same path (case-insensitive) - nothing to do
        return true;
    }
#else
    if (currentNorm == newNorm) {
        // Same path - nothing to do
        return true;
    }
#endif
    
    // Check if target folder already exists (and is different from current)
    if (fs::exists(newPath)) {
        // On case-insensitive filesystems, check if it's actually the same folder
        std::error_code ec;
        if (fs::equivalent(currentPath, newPath, ec)) {
            // Same folder, just different case - proceed with rename for case change
        } else {
            m_ui.ShowToast("A folder named \"" + sanitizedName + 
                          "\" already exists", Toast::Type::Error);
            return false;
        }
    }
    
    // Store old path for recent projects update
    std::string oldPath = m_currentFilePath;
    
    // Perform the rename
    std::error_code ec;
    fs::rename(currentPath, newPath, ec);
    
    if (ec) {
        m_ui.ShowToast("Failed to rename folder: " + ec.message(), 
                      Toast::Type::Error);
        return false;
    }
    
    // Update current file path (ensure trailing slash for folders)
    std::string newPathStr = newPath.string();
    if (newPathStr.back() != '/' && newPathStr.back() != '\\') {
#ifdef _WIN32
        newPathStr += '\\';
#else
        newPathStr += '/';
#endif
    }
    m_currentFilePath = newPathStr;
    
    // Update recent projects list
    RecentProjects::Remove(oldPath);
    RecentProjects::Add(m_currentFilePath);
    
    // Update window title
    UpdateWindowTitle();
    
    m_ui.ShowToast("Project renamed to \"" + sanitizedName + "\"", 
                  Toast::Type::Success);
    
    return true;
}


void App::ExportPackage(const std::string& cartPath) {
    // Use cached thumbnail from last render (simpler and more reliable)
    std::vector<uint8_t> thumbnailPixels;
    int thumbWidth = 0, thumbHeight = 0;
    
    if (m_canvas.hasCachedThumbnail) {
        thumbnailPixels = m_canvas.cachedThumbnail;
        thumbWidth = m_canvas.cachedThumbnailWidth;
        thumbHeight = m_canvas.cachedThumbnailHeight;
    }
    
    bool success = Package::Save(m_model, cartPath, &m_icons,
                                thumbnailPixels.empty() ? nullptr : 
                                    thumbnailPixels.data(),
                                thumbWidth, thumbHeight);
    
    if (success) {
        // Extract filename from path
        std::string filename = std::filesystem::path(cartPath)
            .filename().string();
        m_ui.ShowToast("Exported: " + filename, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to export package: " + cartPath, 
                      Toast::Type::Error);
    }
}


void App::ExportPng(const std::string& path) {
    bool success = Cartograph::ExportPng::Export(
        m_model, 
        m_canvas, 
        *m_renderer, 
        &m_icons,
        path, 
        m_ui.m_modals.exportOptions
    );
    
    if (success) {
        // Extract filename from path
        std::string filename = std::filesystem::path(path).filename().string();
        m_ui.ShowToast("Exported: " + filename, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to export: " + path, Toast::Type::Error);
    }
}


void App::CheckAutosaveRecovery() {
    std::string autosaveDir = Platform::GetAutosaveDir();
    std::string autosavePath = autosaveDir + "autosave.json";
    std::string metadataPath = autosaveDir + "metadata.json";
    
    // Check if autosave file exists
    auto autosaveContent = Platform::ReadFile(autosavePath);
    if (!autosaveContent) {
        return;  // No autosave to recover
    }
    
    // Check metadata for clean shutdown flag
    auto metadataContent = Platform::ReadFile(metadataPath);
    bool wasCleanShutdown = true;  // Default to clean if no metadata
    
    if (metadataContent) {
        try {
            nlohmann::json meta = nlohmann::json::parse(*metadataContent);
            wasCleanShutdown = meta.value("cleanShutdown", true);
        } catch (...) {
            // Metadata parse error, assume unclean
            wasCleanShutdown = false;
        }
    }
    
    // If shutdown wasn't clean, mark for recovery
    if (!wasCleanShutdown) {
        m_hasAutosaveRecovery = true;
    }
}


void App::SaveAutosaveMetadata() {
    std::string autosaveDir = Platform::GetAutosaveDir();
    Platform::EnsureDirectoryExists(autosaveDir);
    
    std::string metadataPath = autosaveDir + "metadata.json";
    
    nlohmann::json meta;
    meta["projectPath"] = m_currentFilePath;
    meta["timestamp"] = static_cast<int64_t>(Platform::GetTime());
    meta["cleanShutdown"] = !m_model.dirty;  // Clean if no unsaved changes
    
    std::string content = meta.dump(2);
    Platform::WriteTextFile(metadataPath, content);
}


void App::CleanupAutosave() {
    std::string autosaveDir = Platform::GetAutosaveDir();
    std::string autosavePath = autosaveDir + "autosave.json";
    std::string metadataPath = autosaveDir + "metadata.json";
    
    // Delete autosave files
    // Note: Platform::DeleteFile doesn't exist yet, so we'll use filesystem
    try {
        std::filesystem::remove(autosavePath);
        std::filesystem::remove(metadataPath);
    } catch (...) {
        // Ignore errors - file might not exist
    }
}


void App::UpdateWindowTitle() {
    if (!m_window) return;
    
    std::string title;
    
    // Get project name: prefer meta.title, fall back to filename, then "Untitled"
    std::string projectName;
    if (!m_model.meta.title.empty()) {
        projectName = m_model.meta.title;
    } else if (!m_currentFilePath.empty()) {
        // Extract filename from path as fallback
        size_t lastSlash = m_currentFilePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            projectName = m_currentFilePath.substr(lastSlash + 1);
        } else {
            projectName = m_currentFilePath;
        }
    } else {
        projectName = "Untitled";
    }
    
    // Add modified indicator using platform convention
    if (m_model.dirty) {
#ifdef __APPLE__
        // macOS uses bullet before project name
        title = "• " + projectName + " - Cartograph";
#else
        // Windows/Linux use asterisk after project name
        title = "*" + projectName + " - Cartograph";
#endif
    } else {
        title = projectName + " - Cartograph";
    }
    
    SDL_SetWindowTitle(m_window.get(), title.c_str());
}


} // namespace Cartograph
