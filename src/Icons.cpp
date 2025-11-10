#include "Icons.h"
#include "platform/Fs.h"
#include <filesystem>
#include <algorithm>

// stb_image for PNG loading
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace fs = std::filesystem;

namespace Cartograph {

IconManager::IconManager()
    : m_atlasTexture(nullptr)
    , m_atlasWidth(0)
    , m_atlasHeight(0)
    , m_atlasDirty(false)
{
}

IconManager::~IconManager() {
    Clear();
}

int IconManager::LoadFromDirectory(const std::string& dir, bool recursive) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return 0;
    }
    
    int count = 0;
    
    auto iterator = recursive 
        ? fs::recursive_directory_iterator(dir)
        : fs::directory_iterator(dir);
    
    for (const auto& entry : iterator) {
        if (!entry.is_regular_file()) continue;
        
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".png" || ext == ".svg") {
            std::string name = entry.path().stem().string();
            if (LoadIcon(entry.path().string(), name)) {
                count++;
            }
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
    
    // TODO: Upload atlas texture to GPU
    // For now, just clear pending icons
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
    
    // TODO: Destroy GL texture
    m_atlasTexture = nullptr;
    m_atlasDirty = false;
}

} // namespace Cartograph

