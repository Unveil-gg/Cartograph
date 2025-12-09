#pragma once

#include <string>
#include <vector>

namespace Cartograph {

class Model;
class IconManager;

/// Extension for project folders (includes leading dot).
static constexpr const char* CARTPROJ_EXTENSION = ".cartproj";

/**
 * Project folder handler for .cartproj format.
 * A .cartproj folder contains:
 * - project.json (model data with relative icon paths)
 * - icons/ (custom icon PNG files)
 * - preview.png (optional thumbnail)
 * 
 * This format is git-friendly and allows direct icon editing.
 * macOS treats .cartproj as a package bundle (selectable as single item).
 */
class ProjectFolder {
public:
    /**
     * Save model to a project folder.
     * @param model Model to save
     * @param folderPath Output folder path
     * @param icons Optional icon manager for custom icons
     * @param thumbnailPixels Optional thumbnail RGBA8 data
     * @param thumbnailWidth Thumbnail width (required if pixels provided)
     * @param thumbnailHeight Thumbnail height (required if pixels provided)
     * @return true on success
     */
    static bool Save(
        const Model& model,
        const std::string& folderPath,
        IconManager* icons = nullptr,
        const uint8_t* thumbnailPixels = nullptr,
        int thumbnailWidth = 0,
        int thumbnailHeight = 0
    );
    
    /**
     * Load model from a project folder.
     * @param folderPath Input folder path
     * @param outModel Output model
     * @param icons Optional icon manager to load custom icons into
     * @return true on success
     */
    static bool Load(
        const std::string& folderPath,
        Model& outModel,
        IconManager* icons = nullptr
    );
    
    /**
     * Check if a path is a valid project folder.
     * Accepts both paths with and without .cartproj extension.
     * Validates that project.json exists and contains Cartograph data.
     * @param path Path to check
     * @return true if it's a valid project folder
     */
    static bool IsProjectFolder(const std::string& path);
    
    /**
     * Check if path has .cartproj extension.
     * @param path Path to check
     * @return true if path ends with .cartproj
     */
    static bool HasCartprojExtension(const std::string& path);
    
    /**
     * Ensure path has .cartproj extension.
     * @param path Path to modify
     * @return Path with .cartproj extension appended if not present
     */
    static std::string EnsureCartprojExtension(const std::string& path);
    
    /**
     * Sanitize a project name for use as a folder name.
     * Replaces invalid filesystem characters with underscores.
     * @param name Raw project name
     * @return Sanitized name safe for filesystem use
     */
    static std::string SanitizeProjectName(const std::string& name);
    
    /**
     * Extract folder name from a path, handling trailing slashes.
     * Strips .cartproj extension from the returned name.
     * @param path Folder path (may have trailing slash or .cartproj)
     * @return Folder name without extension, or empty if extraction fails
     */
    static std::string GetFolderNameFromPath(const std::string& path);
    
private:
    /**
     * Get list of custom icon files to save.
     * @param icons Icon manager
     * @return Vector of icon name and file path pairs
     */
    static std::vector<std::pair<std::string, std::string>>
        GetIconList(IconManager* icons);
};

} // namespace Cartograph

