#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// Forward declare ImTextureID - actual definition in imgui.h
// ImGui defines it as ImU64 (unsigned long long)
using ImTextureID = unsigned long long;

namespace Cartograph {

/**
 * Icon entry in the atlas.
 */
struct Icon {
    std::string name;
    int atlasX, atlasY;     // Position in atlas
    int width, height;      // Icon dimensions
    float u0, v0, u1, v1;   // UV coordinates
};

/**
 * Icon manager.
 * Loads PNG (and optionally SVG) icons from disk, builds a texture atlas,
 * and provides lookup by name.
 */
class IconManager {
public:
    IconManager();
    ~IconManager();
    
    /**
     * Load icons from a directory.
     * @param dir Directory path
     * @param recursive Search subdirectories
     * @return Number of icons loaded
     */
    int LoadFromDirectory(const std::string& dir, bool recursive = true);
    
    /**
     * Load a single icon file.
     * @param path File path (PNG or SVG)
     * @param name Icon name (defaults to filename without extension)
     * @return true on success
     */
    bool LoadIcon(const std::string& path, const std::string& name = "");
    
    /**
     * Build the texture atlas from loaded icons.
     * Must be called after loading icons and before rendering.
     */
    void BuildAtlas();
    
    /**
     * Get an icon by name.
     * @param name Icon name
     * @return Pointer to icon, or nullptr if not found
     */
    const Icon* GetIcon(const std::string& name) const;
    
    /**
     * Get the atlas texture ID (for ImGui rendering).
     */
    ImTextureID GetAtlasTexture() const { return m_atlasTexture; }
    
    /**
     * Get the number of icons loaded.
     */
    size_t GetIconCount() const { return m_icons.size(); }
    
    /**
     * Clear all loaded icons.
     */
    void Clear();
    
    /**
     * Add an icon from memory (for dynamic imports).
     * @param name Icon name
     * @param pixels RGBA8 pixel data
     * @param width Width in pixels
     * @param height Height in pixels
     * @return true on success
     */
    bool AddIconFromMemory(
        const std::string& name,
        const uint8_t* pixels,
        int width,
        int height
    );
    
    /**
     * Validate icon data.
     * @param width Icon width
     * @param height Icon height
     * @param fileSize File size in bytes (0 to skip)
     * @param errorMsg Output error message
     * @return true if valid
     */
    static bool ValidateIcon(
        int width,
        int height,
        size_t fileSize,
        std::string& errorMsg
    );
    
    /**
     * Generate a unique icon name from a base name.
     * @param baseName Base name (e.g., "chest")
     * @return Unique name (e.g., "chest_1" if "chest" exists)
     */
    std::string GenerateUniqueName(const std::string& baseName) const;
    
    /**
     * Process icon from file (validation + load).
     * This is designed to be called from a background thread.
     * @param path File path
     * @param outPixels Output pixel data (RGBA8)
     * @param outWidth Output width
     * @param outHeight Output height
     * @param errorMsg Output error message
     * @return true on success
     */
    static bool ProcessIconFromFile(
        const std::string& path,
        std::vector<uint8_t>& outPixels,
        int& outWidth,
        int& outHeight,
        std::string& errorMsg
    );
    
private:
    struct IconData {
        std::string name;
        int width, height;
        std::vector<uint8_t> pixels;  // RGBA8
    };
    
    bool LoadPng(const std::string& path, IconData& out);
    bool LoadSvg(const std::string& path, IconData& out);
    
    std::unordered_map<std::string, Icon> m_icons;
    std::vector<IconData> m_pendingIcons;  // Not yet in atlas
    ImTextureID m_atlasTexture;
    int m_atlasWidth, m_atlasHeight;
    bool m_atlasDirty;
};

} // namespace Cartograph

