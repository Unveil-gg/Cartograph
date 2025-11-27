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

// stb_image_resize2 for icon resizing
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>

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

int IconManager::LoadFromDirectory(const std::string& dir, 
                                   const std::string& category,
                                   bool recursive) {
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
            if (LoadIcon(entry.path().string(), name, category)) {
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

bool IconManager::LoadIcon(const std::string& path, 
                           const std::string& name,
                           const std::string& category) {
    std::string iconName = name;
    if (iconName.empty()) {
        iconName = fs::path(path).stem().string();
    }
    
    IconData data;
    data.name = iconName;
    data.category = category;
    
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    bool success = false;
    // stb_image supports: PNG, JPEG, BMP, GIF, TGA, PSD, HDR, PIC, PNM
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || 
        ext == ".bmp" || ext == ".gif" || ext == ".tga" ||
        ext == ".webp") {
        // All raster formats use stb_image loader
        success = LoadPng(path, data);
    } else if (ext == ".svg") {
#ifdef CARTOGRAPH_USE_SVG
        success = LoadSvg(path, data);
#endif
    }
    
    if (success) {
        // Store pixel data permanently for rebuilds
        m_iconData[iconName] = std::move(data);
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
    if (!m_atlasDirty || m_iconData.empty()) {
        return;
    }
    
    // Delete old texture if it exists
    if (m_atlasTexture != 0) {
        GLuint texId = (GLuint)(intptr_t)m_atlasTexture;
        glDeleteTextures(1, &texId);
        m_atlasTexture = 0;
    }
    
    // Clear old icon entries (we'll rebuild them)
    m_icons.clear();
    
    // Simple atlas packing: arrange icons in a grid
    // TODO: For a production app, use a proper bin-packing algorithm
    
    const int maxIconSize = 64;  // Max icon size
    const int iconsPerRow = 16;
    
    int numIcons = m_iconData.size();
    int numRows = (numIcons + iconsPerRow - 1) / iconsPerRow;
    
    m_atlasWidth = iconsPerRow * maxIconSize;
    m_atlasHeight = numRows * maxIconSize;
    
    // Allocate atlas buffer
    std::vector<uint8_t> atlasPixels(m_atlasWidth * m_atlasHeight * 4, 0);
    
    // Copy all icons into atlas (rebuild entire atlas)
    size_t i = 0;
    for (const auto& pair : m_iconData) {
        const IconData& iconData = pair.second;
        
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
        
        // Create icon entry with updated UVs
        Icon icon;
        icon.name = iconData.name;
        icon.category = iconData.category;
        icon.atlasX = atlasX;
        icon.atlasY = atlasY;
        icon.width = iconData.width;
        icon.height = iconData.height;
        icon.u0 = (float)atlasX / m_atlasWidth;
        icon.v0 = (float)atlasY / m_atlasHeight;
        icon.u1 = (float)(atlasX + iconData.width) / m_atlasWidth;
        icon.v1 = (float)(atlasY + iconData.height) / m_atlasHeight;
        
        m_icons[icon.name] = icon;
        ++i;
    }
    
    // Upload new atlas texture to GPU
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
    m_atlasDirty = false;
}

const Icon* IconManager::GetIcon(const std::string& name) const {
    auto it = m_icons.find(name);
    return (it != m_icons.end()) ? &it->second : nullptr;
}

std::vector<std::string> IconManager::GetAllIconNames() const {
    std::vector<std::string> names;
    names.reserve(m_icons.size());
    for (const auto& pair : m_icons) {
        names.push_back(pair.first);
    }
    // Sort alphabetically for consistent ordering
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> IconManager::GetIconNamesByCategory(
    const std::string& category
) const {
    std::vector<std::string> names;
    for (const auto& pair : m_icons) {
        if (pair.second.category == category) {
            names.push_back(pair.first);
        }
    }
    // Sort alphabetically for consistent ordering
    std::sort(names.begin(), names.end());
    return names;
}

void IconManager::Clear() {
    m_icons.clear();
    m_iconData.clear();
    
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
    int height,
    const std::string& category
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
    data.category = category;
    data.width = width;
    data.height = height;
    data.pixels.assign(pixels, pixels + width * height * 4);
    
    // Store pixel data permanently for rebuilds
    m_iconData[name] = std::move(data);
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
    
    // Size limits: max 2048×2048 input (will be resized to 64×64)
    const int MAX_INPUT_SIZE = 2048;
    if (width > MAX_INPUT_SIZE || height > MAX_INPUT_SIZE) {
        errorMsg = "Icon too large (max " + std::to_string(MAX_INPUT_SIZE) + 
                   "×" + std::to_string(MAX_INPUT_SIZE) + ", got " +
                   std::to_string(width) + "×" + std::to_string(height) + ")";
        return false;
    }
    
    // Minimum size check (avoid tiny images that won't upscale well)
    const int MIN_INPUT_SIZE = 8;
    if (width < MIN_INPUT_SIZE || height < MIN_INPUT_SIZE) {
        errorMsg = "Icon too small (min " + std::to_string(MIN_INPUT_SIZE) + 
                   "×" + std::to_string(MIN_INPUT_SIZE) + ", got " +
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

} // namespace Cartograph

