#include "ProjectFolder.h"
#include "Model.h"
#include "Icons.h"
#include "IOJson.h"
#include "platform/Fs.h"
#include <filesystem>
#include <nlohmann/json.hpp>

// stb_image_write for PNG encoding
#define STB_IMAGE_WRITE_IMPLEMENTATION_PROJECT_FOLDER
#include <stb/stb_image_write.h>

namespace fs = std::filesystem;

namespace Cartograph {

// Length of ".cartproj" extension
static constexpr size_t CARTPROJ_EXT_LEN = 9;

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
    if (!fs::exists(projectPath) || !fs::is_regular_file(projectPath)) {
        return false;
    }
    
    // Validate JSON content has Cartograph-specific fields
    auto content = Platform::ReadTextFile(projectPath);
    if (!content) {
        return false;
    }
    
    try {
        auto j = nlohmann::json::parse(*content);
        
        // Must have version == 1 (Cartograph format)
        if (!j.contains("version") || j["version"] != 1) {
            return false;
        }
        
        // Must have grid object (Cartograph-specific)
        if (!j.contains("grid") || !j["grid"].is_object()) {
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool ProjectFolder::HasCartprojExtension(const std::string& path) {
    if (path.size() < CARTPROJ_EXT_LEN) {
        return false;
    }
    std::string ext = path.substr(path.size() - CARTPROJ_EXT_LEN);
    // Case-insensitive comparison
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == CARTPROJ_EXTENSION;
}

std::string ProjectFolder::EnsureCartprojExtension(const std::string& path) {
    // Strip trailing slashes first
    std::string cleanPath = path;
    while (!cleanPath.empty() && 
           (cleanPath.back() == '/' || cleanPath.back() == '\\')) {
        cleanPath.pop_back();
    }
    
    if (HasCartprojExtension(cleanPath)) {
        return cleanPath;
    }
    return cleanPath + CARTPROJ_EXTENSION;
}

std::string ProjectFolder::SanitizeProjectName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    
    bool lastWasDash = false;
    
    for (char c : name) {
        // Convert whitespace to dash
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            // Collapse multiple whitespace/dashes to single dash
            if (!lastWasDash && !result.empty()) {
                result += '-';
                lastWasDash = true;
            }
        }
        // Replace invalid filesystem characters with dash
        else if (c == '/' || c == '\\' || c == ':' || c == '*' || 
                 c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            if (!lastWasDash && !result.empty()) {
                result += '-';
                lastWasDash = true;
            }
        }
        // Keep valid characters
        else {
            result += c;
            lastWasDash = (c == '-');  // Track existing dashes
        }
    }
    
    // Trim trailing dashes
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    
    // Trim leading dashes
    size_t start = result.find_first_not_of('-');
    if (start == std::string::npos) {
        return "";  // All dashes or empty
    }
    
    return result.substr(start);
}

std::string ProjectFolder::GetFolderNameFromPath(const std::string& path) {
    // Normalize path to remove trailing slashes, then extract filename
    fs::path p = fs::path(path).lexically_normal();
    std::string name = p.filename().string();
    
    // If still empty (e.g., root path), try parent
    if (name.empty() || name == "." || name == "..") {
        name = p.parent_path().filename().string();
    }
    
    // Strip .cartproj extension if present
    if (HasCartprojExtension(name)) {
        name = name.substr(0, name.size() - CARTPROJ_EXT_LEN);
    }
    
    return name;
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

