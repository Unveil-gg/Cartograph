#include "ProjectLoad.h"

#include "IOJson.h"
#include "Model.h"
#include "Package.h"
#include "ProjectFolder.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace Cartograph {

bool LoadProjectAny(
    const std::string& path,
    Model& outModel,
    std::string* error
) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };

    std::error_code ec;
    if (!fs::exists(path, ec)) return fail("not found: " + path);

    if (fs::is_directory(path, ec)) {
        if (!ProjectFolder::IsProjectFolder(path)) {
            return fail("not a Cartograph project folder: " + path);
        }
        if (!ProjectFolder::Load(path, outModel, nullptr)) {
            return fail("could not load project folder: " + path);
        }
        return true;
    }

    if (Package::HasCartExtension(path)) {
        if (!Package::Load(path, outModel, nullptr)) {
            return fail("could not read .cart package: " + path);
        }
        return true;
    }

    if (!IOJson::LoadFromFile(path, outModel)) {
        return fail("could not read project JSON: " + path);
    }
    return true;
}

}  // namespace Cartograph
