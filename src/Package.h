#pragma once

#include <string>
#include <optional>

namespace Cartograph {

class Model;
class IconManager;

/**
 * .cart package file handler.
 * .cart is a ZIP container with:
 * - /manifest.json (metadata)
 * - /project.json (model data)
 * - /thumb.png (optional preview)
 * - /icons/ (optional custom icons)
 * - /themes/ (optional custom themes)
 */
class Package {
public:
    /**
     * Save model to a .cart package file.
     * @param model Model to save
     * @param path Output path (.cart)
     * @param icons Optional icon manager for custom icons
     * @param thumbnailPixels Optional thumbnail RGBA8 data
     * @param thumbnailWidth Thumbnail width (required if pixels provided)
     * @param thumbnailHeight Thumbnail height (required if pixels provided)
     * @return true on success
     */
    static bool Save(
        const Model& model, 
        const std::string& path,
        IconManager* icons = nullptr,
        const uint8_t* thumbnailPixels = nullptr,
        int thumbnailWidth = 0,
        int thumbnailHeight = 0
    );
    
    /**
     * Load model from a .cart package file.
     * @param path Input path (.cart)
     * @param outModel Output model
     * @param icons Optional icon manager to load custom icons into
     * @return true on success
     */
    static bool Load(
        const std::string& path, 
        Model& outModel,
        IconManager* icons = nullptr
    );
    
    /**
     * Create a manifest.json string.
     * @param model Model to create manifest for
     * @return JSON string
     */
    static std::string CreateManifest(const Model& model);
};

} // namespace Cartograph

