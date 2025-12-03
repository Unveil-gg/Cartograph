#include "ProjectFolder.h"
#include "Model.h"
#include "Icons.h"
#include "IOJson.h"
#include "platform/Fs.h"
#include <filesystem>

// stb_image_write for PNG encoding
#define STB_IMAGE_WRITE_IMPLEMENTATION_PROJECT_FOLDER
#include <stb/stb_image_write.h>

namespace fs = std::filesystem;

namespace Cartograph {

bool ProjectFolder::Save(
    const Model& model,
    const std::string& folderPath,
    IconManager* icons,
    const uint8_t* thumbnailPixels,
    int thumbnailWidth,
    int thumbnailHeight
) {
    // Create folder if it doesn't exist
    if (!fs::exists(folderPath)) {
        if (!fs::create_directories(folderPath)) {
            return false;
        }
    }
    
    // Save project.json
    std::string projectPath = folderPath + "/project.json";
    if (!IOJson::SaveToFile(model, projectPath)) {
        return false;
    }
    
    // Save custom icons
    if (icons) {
        std::string iconsPath = folderPath + "/icons";
        
        // Get custom icon data
        auto customIcons = icons->GetCustomIconData();
        
        if (!customIcons.empty()) {
            // Create icons directory
            if (!fs::exists(iconsPath)) {
                if (!fs::create_directories(iconsPath)) {
                    return false;
                }
            }
            
            // Save each icon as PNG
            for (const auto& iconPair : customIcons) {
                const std::string& iconName = iconPair.first;
                const std::vector<uint8_t>* pixels = iconPair.second;
                
                if (!pixels || pixels->empty()) continue;
                
                // Get icon dimensions
                int width, height;
                if (!icons->GetIconDimensions(iconName, width, height)) {
                    continue;
                }
                
                // Write PNG file
                std::string iconFilePath = iconsPath + "/" + 
                                          iconName + ".png";
                if (!stbi_write_png(iconFilePath.c_str(), width, height, 4,
                                   pixels->data(), width * 4)) {
                    // Failed to write icon, but continue with others
                    continue;
                }
            }
        }
    }
    
    // Save thumbnail
    if (thumbnailPixels && thumbnailWidth > 0 && thumbnailHeight > 0) {
        std::string thumbPath = folderPath + "/preview.png";
        if (!stbi_write_png(thumbPath.c_str(), thumbnailWidth, 
                           thumbnailHeight, 4, thumbnailPixels, 
                           thumbnailWidth * 4)) {
            // Failed to write thumbnail, but don't fail entire save
        }
    }
    
    return true;
}

bool ProjectFolder::Load(
    const std::string& folderPath,
    Model& outModel,
    IconManager* icons
) {
    // Check if folder exists
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        return false;
    }
    
    // Load project.json
    std::string projectPath = folderPath + "/project.json";
    if (!IOJson::LoadFromFile(projectPath, outModel)) {
        return false;
    }
    
    // Load custom icons from icons/ folder
    if (icons) {
        std::string iconsPath = folderPath + "/icons";
        
        if (fs::exists(iconsPath) && fs::is_directory(iconsPath)) {
            for (const auto& entry : fs::directory_iterator(iconsPath)) {
                if (!entry.is_regular_file()) continue;
                
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), 
                              ext.begin(), ::tolower);
                
                if (ext == ".png") {
                    std::string iconName = entry.path().stem().string();
                    std::string iconPath = entry.path().string();
                    
                    // Process and load icon
                    std::vector<uint8_t> pixels;
                    int width, height;
                    std::string errorMsg;
                    
                    if (IconManager::ProcessIconFromFile(
                            iconPath, pixels, width, height, errorMsg)) {
                        icons->AddIconFromMemory(
                            iconName, pixels.data(), width, height, "marker"
                        );
                    }
                }
            }
        }
    }
    
    return true;
}

bool ProjectFolder::IsProjectFolder(const std::string& path) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return false;
    }
    
    // Check for project.json
    std::string projectPath = path + "/project.json";
    return fs::exists(projectPath) && fs::is_regular_file(projectPath);
}

std::vector<std::pair<std::string, std::string>>
ProjectFolder::GetIconList(IconManager* icons) {
    std::vector<std::pair<std::string, std::string>> result;
    
    if (!icons) return result;
    
    auto customIcons = icons->GetCustomIconData();
    for (const auto& iconPair : customIcons) {
        const std::string& iconName = iconPair.first;
        std::string relativePath = "icons/" + iconName + ".png";
        result.push_back({iconName, relativePath});
    }
    
    return result;
}

} // namespace Cartograph

