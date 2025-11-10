#pragma once

#include <string>
#include <optional>

namespace Cartograph {

class Model;

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
     * @return true on success
     */
    static bool Save(const Model& model, const std::string& path);
    
    /**
     * Load model from a .cart package file.
     * @param path Input path (.cart)
     * @param outModel Output model
     * @return true on success
     */
    static bool Load(const std::string& path, Model& outModel);
    
    /**
     * Create a manifest.json string.
     * @param model Model to create manifest for
     * @return JSON string
     */
    static std::string CreateManifest(const Model& model);
};

} // namespace Cartograph

