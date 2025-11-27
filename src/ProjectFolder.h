#pragma once

#include <string>
#include <vector>

namespace Cartograph {

class Model;
class IconManager;

/**
 * Project folder handler for .cartproj format.
 * A .cartproj folder contains:
 * - project.json (model data with relative icon paths)
 * - icons/ (custom icon PNG files)
 * 
 * This format is git-friendly and allows direct icon editing.
 */
class ProjectFolder {
public:
    /**
     * Save model to a project folder.
     * @param model Model to save
     * @param folderPath Output folder path
     * @param icons Optional icon manager for custom icons
     * @return true on success
     */
    static bool Save(
        const Model& model,
        const std::string& folderPath,
        IconManager* icons = nullptr
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
     * @param path Path to check
     * @return true if it's a valid project folder
     */
    static bool IsProjectFolder(const std::string& path);
    
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

