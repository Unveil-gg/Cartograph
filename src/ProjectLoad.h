#pragma once

#include <string>

namespace Cartograph {

class Model;

/**
 * Load a project from a .cart package, a .cartproj folder, or a bare
 * project.json file. No icons or window are involved, so this works
 * headless. Old versions are migrated by IOJson.
 * @param path Input path
 * @param outModel Output model
 * @param error Optional: receives a message on failure
 * @return true on success
 */
bool LoadProjectAny(
    const std::string& path,
    Model& outModel,
    std::string* error = nullptr
);

}  // namespace Cartograph
