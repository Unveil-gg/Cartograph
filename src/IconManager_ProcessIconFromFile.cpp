#include "Icons.h"
#include "platform/Fs.h"
#include <filesystem>
#include <algorithm>

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// stb_image for PNG loading
#include <stb/stb_image.h>

// stb_image_resize2 for icon resizing
#include <stb/stb_image_resize2.h>

namespace fs = std::filesystem;

namespace Cartograph {


bool IconManager::ProcessIconFromFile(
    const std::string& path,
    std::vector<uint8_t>& outPixels,
    int& outWidth,
    int& outHeight,
    std::string& errorMsg
) {
    // Load image using stb_image
    int width, height, channels;
    unsigned char* data = stbi_load(
        path.c_str(),
        &width, &height, &channels,
        4  // Force RGBA
    );
    
    if (!data) {
        errorMsg = "Failed to load image";
        return false;
    }
    
    // Validate dimensions (accepts non-square, will resize to square)
    if (!ValidateIcon(width, height, 0, errorMsg)) {
        stbi_image_free(data);
        return false;
    }
    
    // Resize to standard 64×64 (non-square images will be scaled to fit)
    const int targetSize = 64;
    
    if (width != targetSize || height != targetSize) {
        // Allocate buffer for resized image
        std::vector<uint8_t> resizedData(targetSize * targetSize * 4);
        
        // Resize using stb_image_resize2
        // Use SRGB for proper color space handling
        unsigned char* result = stbir_resize_uint8_srgb(
            data, width, height, 0,           // Source
            resizedData.data(), 
            targetSize, targetSize, 0,        // Destination
            STBIR_RGBA                        // 4 channels
        );
        
        stbi_image_free(data);
        
        if (result == nullptr) {
            errorMsg = "Failed to resize icon";
            return false;
        }
        
        // Use resized data
        outPixels = std::move(resizedData);
        outWidth = targetSize;
        outHeight = targetSize;
    } else {
        // Already correct size, copy directly
        size_t dataSize = width * height * 4;
        outPixels.assign(data, data + dataSize);
        outWidth = width;
        outHeight = height;
        
        stbi_image_free(data);
    }
    
    return true;
}


bool IconManager::RenameIcon(
    const std::string& oldName,
    const std::string& newName,
    std::string& errorMsg
) {
    // Validate old name exists
    if (m_icons.find(oldName) == m_icons.end()) {
        errorMsg = "Icon '" + oldName + "' not found";
        return false;
    }
    
    // Validate new name doesn't exist
    if (m_icons.find(newName) != m_icons.end()) {
        errorMsg = "Icon '" + newName + "' already exists";
        return false;
    }
    
    // Validate new name is not empty
    if (newName.empty()) {
        errorMsg = "Icon name cannot be empty";
        return false;
    }
    
    // Update m_icons map
    auto iconNode = m_icons.extract(oldName);
    if (!iconNode.empty()) {
        iconNode.key() = newName;
        iconNode.mapped().name = newName;
        m_icons.insert(std::move(iconNode));
    }
    
    // Update m_iconData map
    auto dataNode = m_iconData.extract(oldName);
    if (!dataNode.empty()) {
        dataNode.key() = newName;
        dataNode.mapped().name = newName;
        m_iconData.insert(std::move(dataNode));
    }
    
    return true;
}


std::vector<std::pair<std::string, const std::vector<uint8_t>*>>
IconManager::GetCustomIconData() const {
    std::vector<std::pair<std::string, const std::vector<uint8_t>*>> result;
    
    for (const auto& pair : m_iconData) {
        // Only include custom marker icons (not built-in tool icons)
        if (pair.second.category == "marker") {
            result.push_back({pair.first, &pair.second.pixels});
        }
    }
    
    return result;
}


bool IconManager::GetIconDimensions(
    const std::string& name,
    int& outWidth,
    int& outHeight
) const {
    auto it = m_iconData.find(name);
    if (it != m_iconData.end()) {
        outWidth = it->second.width;
        outHeight = it->second.height;
        return true;
    }
    return false;
}


bool IconManager::DeleteIcon(
    const std::string& name,
    std::string& errorMsg
) {
    // Check if icon exists
    auto iconIt = m_icons.find(name);
    if (iconIt == m_icons.end()) {
        errorMsg = "Icon '" + name + "' not found";
        return false;
    }
    
    // Only allow deletion of custom marker icons (not built-in tool icons)
    if (iconIt->second.category != "marker") {
        errorMsg = "Cannot delete built-in icon '" + name + "'";
        return false;
    }
    
    // Remove from both maps
    m_icons.erase(name);
    m_iconData.erase(name);
    
    // Mark atlas for rebuild
    m_atlasDirty = true;
    
    return true;
}


bool IconManager::GetIconData(
    const std::string& name,
    std::vector<uint8_t>& outPixels,
    int& outWidth,
    int& outHeight,
    std::string& outCategory
) const {
    auto it = m_iconData.find(name);
    if (it == m_iconData.end()) {
        return false;
    }
    
    const IconData& data = it->second;
    outPixels = data.pixels;
    outWidth = data.width;
    outHeight = data.height;
    outCategory = data.category;
    
    return true;
}


} // namespace Cartograph

