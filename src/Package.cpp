#include "Package.h"
#include "Model.h"
#include "IOJson.h"
#include "Icons.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>

// minizip-ng compatibility layer provides zip.h and unzip.h
#include "zip.h"
#include "unzip.h"

// stb_image_write for PNG encoding (implementation in ExportPng.cpp)
#include <stb/stb_image_write.h>
// stb_image for PNG decoding (implementation in Icons.cpp)
#include <stb/stb_image.h>

using json = nlohmann::json;

namespace Cartograph {

std::string Package::CreateManifest(const Model& model) {
    json j;
    j["kind"] = "unveil-cartograph";
    j["version"] = 1;
    j["title"] = model.meta.title;
    j["author"] = model.meta.author;
    j["description"] = model.meta.description;
    return j.dump(2);
}

bool Package::Save(
    const Model& model, 
    const std::string& path,
    IconManager* icons,
    const uint8_t* thumbnailPixels,
    int thumbnailWidth,
    int thumbnailHeight
) {
    // Create ZIP file
    zipFile zf = zipOpen(path.c_str(), APPEND_STATUS_CREATE);
    if (!zf) {
        return false;
    }
    
    bool success = true;
    zip_fileinfo fi = {};
    
    // Add manifest.json
    std::string manifest = CreateManifest(model);
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
    
    // Add custom icons
    if (icons) {
        // Memory writer callback for stb_image_write
        struct MemoryWriter {
            std::vector<uint8_t> data;
            
            static void callback(void* context, void* data, int size) {
                MemoryWriter* writer = static_cast<MemoryWriter*>(context);
                uint8_t* bytes = static_cast<uint8_t*>(data);
                writer->data.insert(writer->data.end(), bytes, bytes + size);
            }
        };
        
        auto customIcons = icons->GetCustomIconData();
        for (const auto& iconPair : customIcons) {
            const std::string& iconName = iconPair.first;
            const std::vector<uint8_t>* pixels = iconPair.second;
            
            if (!pixels || pixels->empty()) continue;
            
            // Get icon dimensions
            int width, height;
            if (!icons->GetIconDimensions(iconName, width, height)) {
                continue;
            }
            
            // Encode to PNG using callback-based API
            MemoryWriter writer;
            int result = stbi_write_png_to_func(
                MemoryWriter::callback,
                &writer,
                width, height, 4,
                pixels->data(),
                0  // stride (0 = tightly packed)
            );
            
            if (result && !writer.data.empty()) {
                // Add to ZIP as icons/{name}.png
                std::string zipPath = "icons/" + iconName + ".png";
                if (zipOpenNewFileInZip(zf, zipPath.c_str(), &fi,
                                       nullptr, 0, nullptr, 0, nullptr,
                                       Z_DEFLATED, 
                                       Z_DEFAULT_COMPRESSION) == ZIP_OK) {
                    zipWriteInFileInZip(zf, writer.data.data(), 
                                       writer.data.size());
                    zipCloseFileInZip(zf);
                }
            }
        }
    }
    
    // Add thumbnail
    if (thumbnailPixels && thumbnailWidth > 0 && thumbnailHeight > 0) {
        // Memory writer for PNG encoding
        struct MemoryWriter {
            std::vector<uint8_t> data;
            
            static void callback(void* context, void* data, int size) {
                MemoryWriter* writer = static_cast<MemoryWriter*>(context);
                uint8_t* bytes = static_cast<uint8_t*>(data);
                writer->data.insert(writer->data.end(), bytes, bytes + size);
            }
        };
        
        MemoryWriter writer;
        int result = stbi_write_png_to_func(
            MemoryWriter::callback,
            &writer,
            thumbnailWidth, thumbnailHeight, 4,
            thumbnailPixels,
            thumbnailWidth * 4
        );
        
        if (result && !writer.data.empty()) {
            if (zipOpenNewFileInZip(zf, "thumb.png", &fi,
                                   nullptr, 0, nullptr, 0, nullptr,
                                   Z_DEFLATED, 
                                   Z_DEFAULT_COMPRESSION) == ZIP_OK) {
                zipWriteInFileInZip(zf, writer.data.data(), 
                                   writer.data.size());
                zipCloseFileInZip(zf);
            }
        }
    }
    
    // TODO: Add themes
    
    zipClose(zf, nullptr);
    return success;
}

bool Package::Load(
    const std::string& path, 
    Model& outModel,
    IconManager* icons
) {
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
            if (unzGetCurrentFileInfo(uf, &fileInfo, filename, 
                                     sizeof(filename),
                                     nullptr, 0, nullptr, 0) != UNZ_OK) {
                continue;
            }
            
            std::string fname(filename);
            
            // Look for project.json
            if (fname == "project.json") {
                if (unzOpenCurrentFile(uf) == UNZ_OK) {
                    std::vector<char> buffer(fileInfo.uncompressed_size);
                    int read = unzReadCurrentFile(uf, buffer.data(), 
                                                 buffer.size());
                    unzCloseCurrentFile(uf);
                    
                    if (read > 0) {
                        std::string json(buffer.data(), read);
                        if (IOJson::LoadFromString(json, outModel)) {
                            foundProject = true;
                        }
                    }
                }
            }
            // Look for custom icons (icons/*.png)
            else if (icons && fname.substr(0, 6) == "icons/" && 
                     fname.size() > 10 && 
                     fname.substr(fname.size() - 4) == ".png") {
                if (unzOpenCurrentFile(uf) == UNZ_OK) {
                    std::vector<uint8_t> buffer(fileInfo.uncompressed_size);
                    int read = unzReadCurrentFile(uf, buffer.data(), 
                                                 buffer.size());
                    unzCloseCurrentFile(uf);
                    
                    if (read > 0) {
                        // Extract icon name from path (icons/name.png -> name)
                        std::string iconName = fname.substr(6, 
                                                           fname.size() - 10);
                        
                        // Process the PNG data
                        std::vector<uint8_t> pixels;
                        int width, height;
                        std::string errorMsg;
                        
                        // Use stb_image to decode PNG from memory
                        int channels;
                        unsigned char* data = stbi_load_from_memory(
                            buffer.data(), buffer.size(),
                            &width, &height, &channels, 4
                        );
                        
                        if (data) {
                            // Add to icon manager
                            icons->AddIconFromMemory(
                                iconName, data, width, height, "marker"
                            );
                            stbi_image_free(data);
                        }
                    }
                }
            }
            
        } while (unzGoToNextFile(uf) == UNZ_OK);
    }
    
    unzClose(uf);
    
    // Rebuild atlas if icons were loaded
    if (icons && foundProject) {
        icons->BuildAtlas();
    }
    
    return foundProject;
}

} // namespace Cartograph

