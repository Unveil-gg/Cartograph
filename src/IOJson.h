#pragma  once

#include <string>
#include <optional>

namespace Cartograph {

class Model;

/**
 * JSON serialization for project data.
 * Handles reading/writing project.json with stable ordering.
 */
class IOJson {
public:
    /**
     * Save model to JSON string.
     * @param model Model to save
     * @return JSON string
     */
    static std::string SaveToString(const Model& model);
    
    /**
     * Load model from JSON string.
     * @param json JSON string
     * @param outModel Output model
     * @return true on success, false on error
     */
    static bool LoadFromString(const std::string& json, Model& outModel);
    
    /**
     * Save model to JSON file.
     * @param model Model to save
     * @param path File path
     * @return true on success
     */
    static bool SaveToFile(const Model& model, const std::string& path);
    
    /**
     * Load model from JSON file.
     * @param path File path
     * @param outModel Output model
     * @return true on success
     */
    static bool LoadFromFile(const std::string& path, Model& outModel);
};

} // namespace Cartograph

