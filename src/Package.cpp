#include "Package.h"
#include "Model.h"
#include "IOJson.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>

// minizip-ng compatibility layer provides zip.h and unzip.h
#include "mz.h"
#include "mz_zip.h"
#include "mz_compat.h"

using json = nlohmann::json;

namespace Cartograph {

std::string Package::CreateManifest(const Model& model) {
    json j;
    j["kind"] = "unveil-cartograph";
    j["version"] = 1;
    j["title"] = model.meta.title;
    j["author"] = model.meta.author;
    return j.dump(2);
}

bool Package::Save(const Model& model, const std::string& path) {
    // Create ZIP file
    zipFile zf = zipOpen(path.c_str(), APPEND_STATUS_CREATE);
    if (!zf) {
        return false;
    }
    
    bool success = true;
    
    // Add manifest.json
    std::string manifest = CreateManifest(model);
    zip_fileinfo fi = {};
    if (zipOpenNewFileInZip(zf, "manifest.json", &fi, 
                            nullptr, 0, nullptr, 0, nullptr,
                            Z_DEFLATED, Z_DEFAULT_COMPRESSION) == ZIP_OK) {
        zipWriteInFileInZip(zf, manifest.data(), manifest.size());
        zipCloseFileInZip(zf);
    } else {
        success = false;
    }
    
    // Add project.json
    std::string projectJson = IOJson::SaveToString(model);
    if (zipOpenNewFileInZip(zf, "project.json", &fi,
                            nullptr, 0, nullptr, 0, nullptr,
                            Z_DEFLATED, Z_DEFAULT_COMPRESSION) == ZIP_OK) {
        zipWriteInFileInZip(zf, projectJson.data(), projectJson.size());
        zipCloseFileInZip(zf);
    } else {
        success = false;
    }
    
    // TODO: Add thumbnail, custom icons, themes
    
    zipClose(zf, nullptr);
    return success;
}

bool Package::Load(const std::string& path, Model& outModel) {
    // Open ZIP file
    unzFile uf = unzOpen(path.c_str());
    if (!uf) {
        return false;
    }
    
    bool foundProject = false;
    
    // Iterate through files
    if (unzGoToFirstFile(uf) == UNZ_OK) {
        do {
            char filename[256];
            unz_file_info fileInfo;
            if (unzGetCurrentFileInfo(uf, &fileInfo, filename, sizeof(filename),
                                     nullptr, 0, nullptr, 0) != UNZ_OK) {
                continue;
            }
            
            // Look for project.json
            if (std::string(filename) == "project.json") {
                if (unzOpenCurrentFile(uf) == UNZ_OK) {
                    std::vector<char> buffer(fileInfo.uncompressed_size);
                    int read = unzReadCurrentFile(uf, buffer.data(), buffer.size());
                    unzCloseCurrentFile(uf);
                    
                    if (read > 0) {
                        std::string json(buffer.data(), read);
                        if (IOJson::LoadFromString(json, outModel)) {
                            foundProject = true;
                        }
                    }
                }
            }
            
        } while (unzGoToNextFile(uf) == UNZ_OK);
    }
    
    unzClose(uf);
    return foundProject;
}

} // namespace Cartograph

