#include "Icons.h"
#include "platform/Fs.h"
#include <filesystem>
#include <algorithm>

// OpenGL for texture cleanup
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

// stb_image for PNG loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace fs = std::filesystem;

namespace Cartograph {

IconManager::IconManager()
    : m_atlasTexture(0)
    , m_atlasWidth(0)
    , m_atlasHeight(0)
    , m_atlasDirty(false)
{
}

IconManager::~IconManager() {
    // Only clear if not already cleared during explicit shutdown
    if (!m_icons.empty() || m_atlasTexture != 0) {
        Clear();
    }
}

int IconManager::LoadFromDirectory(const std::string& dir, bool recursive) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return 0;
    }
    
    int count = 0;
    
    // Helper lambda to process entries
    auto processEntry = [&](const fs::directory_entry& entry) {
        if (!entry.is_regular_file()) return;
        
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".png" || ext == ".svg") {
            std::string name = entry.path().stem().string();
            if (LoadIcon(entry.path().string(), name)) {
                count++;
            }
        }
    };
    
    // Use different iterator types based on recursive flag
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            processEntry(entry);
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dir)) {
            processEntry(entry);
        }
    }
    
    return count;
}

bool IconManager::LoadIcon(const std::string& path, const std::string& name) {
    std::string iconName = name;
    if (iconName.empty()) {
        iconName = fs::path(path).stem().string();
    }
    
    IconData data;
    data.name = iconName;
    
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    bool success = false;
    if (ext == ".png") {
        success = LoadPng(path, data);
    } else if (ext == ".svg") {
#ifdef CARTOGRAPH_USE_SVG
        success = LoadSvg(path, data);
#endif
    }
    
    if (success) {
        m_pendingIcons.push_back(std::move(data));
        m_atlasDirty = true;
        return true;
    }
    
    return false;
}

bool IconManager::LoadPng(const std::string& path, IconData& out) {
    int width, height, channels;
    unsigned char* data = stbi_load(
        path.c_str(), 
        &width, &height, &channels, 
        4  // Force RGBA
    );
    
    if (!data) {
        return false;
    }
    
    out.width = width;
    out.height = height;
    out.pixels.assign(data, data + width * height * 4);
    
    stbi_image_free(data);
    return true;
}

bool IconManager::LoadSvg(const std::string& path, IconData& out) {
    // TODO: Implement SVG loading with NanoSVG
    // For now, return false
    return false;
}

void IconManager::BuildAtlas() {
    if (!m_atlasDirty || m_pendingIcons.empty()) {
        return;
    }
    
    // Simple atlas packing: arrange icons in a grid
    // For a production app, use a proper bin-packing algorithm
    
    const int maxIconSize = 64;  // Max icon size
    const int iconsPerRow = 16;
    
    int numIcons = m_pendingIcons.size();
    int numRows = (numIcons + iconsPerRow - 1) / iconsPerRow;
    
    m_atlasWidth = iconsPerRow * maxIconSize;
    m_atlasHeight = numRows * maxIconSize;
    
    // Allocate atlas buffer
    std::vector<uint8_t> atlasPixels(m_atlasWidth * m_atlasHeight * 4, 0);
    
    // Copy icons into atlas
    for (size_t i = 0; i < m_pendingIcons.size(); ++i) {
        const auto& iconData = m_pendingIcons[i];
        
        int row = i / iconsPerRow;
        int col = i % iconsPerRow;
        int atlasX = col * maxIconSize;
        int atlasY = row * maxIconSize;
        
        // Copy pixels
        for (int y = 0; y < iconData.height; ++y) {
            for (int x = 0; x < iconData.width; ++x) {
                int srcIdx = (y * iconData.width + x) * 4;
                int dstIdx = ((atlasY + y) * m_atlasWidth + (atlasX + x)) * 4;
                
                if (dstIdx + 3 < atlasPixels.size()) {
                    atlasPixels[dstIdx + 0] = iconData.pixels[srcIdx + 0];
                    atlasPixels[dstIdx + 1] = iconData.pixels[srcIdx + 1];
                    atlasPixels[dstIdx + 2] = iconData.pixels[srcIdx + 2];
                    atlasPixels[dstIdx + 3] = iconData.pixels[srcIdx + 3];
                }
            }
        }
        
        // Create icon entry
        Icon icon;
        icon.name = iconData.name;
        icon.atlasX = atlasX;
        icon.atlasY = atlasY;
        icon.width = iconData.width;
        icon.height = iconData.height;
        icon.u0 = (float)atlasX / m_atlasWidth;
        icon.v0 = (float)atlasY / m_atlasHeight;
        icon.u1 = (float)(atlasX + iconData.width) / m_atlasWidth;
        icon.v1 = (float)(atlasY + iconData.height) / m_atlasHeight;
        
        m_icons[icon.name] = icon;
    }
    
    // Upload atlas texture to GPU
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        m_atlasWidth,
        m_atlasHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        atlasPixels.data()
    );
    
    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    
    m_atlasTexture = (ImTextureID)(intptr_t)texId;
    
    m_pendingIcons.clear();
    m_atlasDirty = false;
}

const Icon* IconManager::GetIcon(const std::string& name) const {
    auto it = m_icons.find(name);
    return (it != m_icons.end()) ? &it->second : nullptr;
}

void IconManager::Clear() {
    m_icons.clear();
    m_pendingIcons.clear();
    
    // Properly destroy GL texture before clearing texture ID
    if (m_atlasTexture != 0) {
        GLuint texId = (GLuint)(intptr_t)m_atlasTexture;
        glDeleteTextures(1, &texId);
        m_atlasTexture = 0;
    }
    
    m_atlasDirty = false;
}

bool IconManager::AddIconFromMemory(
    const std::string& name,
    const uint8_t* pixels,
    int width,
    int height
) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }
    
    // Validate icon
    std::string errorMsg;
    if (!ValidateIcon(width, height, 0, errorMsg)) {
        return false;
    }
    
    // Create icon data
    IconData data;
    data.name = name;
    data.width = width;
    data.height = height;
    data.pixels.assign(pixels, pixels + width * height * 4);
    
    // Add to pending icons
    m_pendingIcons.push_back(std::move(data));
    m_atlasDirty = true;
    
    return true;
}

bool IconManager::ValidateIcon(
    int width,
    int height,
    size_t fileSize,
    std::string& errorMsg
) {
    // Check dimensions
    if (width <= 0 || height <= 0) {
        errorMsg = "Invalid dimensions";
        return false;
    }
    
    // Must be square
    if (width != height) {
        errorMsg = "Icon must be square (got " + std::to_string(width) + 
                   "×" + std::to_string(height) + ")";
        return false;
    }
    
    // Size limits (8x8 to 512x512)
    if (width < 8 || width > 512) {
        errorMsg = "Icon size must be between 8×8 and 512×512 (got " + 
                   std::to_string(width) + "×" + std::to_string(height) + ")";
        return false;
    }
    
    // File size check (if provided)
    if (fileSize > 0) {
        const size_t MAX_FILE_SIZE = 1024 * 1024;  // 1MB
        if (fileSize > MAX_FILE_SIZE) {
            errorMsg = "File too large (max 1MB)";
            return false;
        }
    }
    
    return true;
}

std::string IconManager::GenerateUniqueName(
    const std::string& baseName
) const {
    // If base name doesn't exist, use it
    if (m_icons.find(baseName) == m_icons.end()) {
        return baseName;
    }
    
    // Try appending _1, _2, etc.
    for (int i = 1; i < 1000; ++i) {
        std::string candidate = baseName + "_" + std::to_string(i);
        if (m_icons.find(candidate) == m_icons.end()) {
            return candidate;
        }
    }
    
    // Fallback: use timestamp
    return baseName + "_" + std::to_string(time(nullptr));
}

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
    
    // Validate dimensions
    if (!ValidateIcon(width, height, 0, errorMsg)) {
        stbi_image_free(data);
        return false;
    }
    
    // TODO: Resize to 64×64 if stb_image_resize2 is available
    // For now, accept icons at their native size
    
    // Copy pixel data
    size_t dataSize = width * height * 4;
    outPixels.assign(data, data + dataSize);
    outWidth = width;
    outHeight = height;
    
    stbi_image_free(data);
    return true;
}

} // namespace Cartograph

