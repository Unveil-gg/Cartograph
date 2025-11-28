#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Cartograph {

class Model;
class IRenderer;
class Canvas;
class IconManager;

/**
 * Thumbnail generation for project previews.
 * Generates small preview images for welcome screen display.
 */
class Thumbnail {
public:
    /**
     * Generate thumbnail as PNG file.
     * Creates a 384x216 (16:9) preview of the entire map.
     * @param model Model to render
     * @param canvas Canvas for view state
     * @param renderer Renderer
     * @param icons Icon manager for markers
     * @param path Output PNG path
     * @return true on success
     */
    static bool Generate(
        const Model& model,
        Canvas& canvas,
        IRenderer& renderer,
        IconManager* icons,
        const std::string& path
    );
    
    /**
     * Generate thumbnail as pixel data in memory.
     * @param model Model to render
     * @param canvas Canvas for view state
     * @param renderer Renderer
     * @param icons Icon manager for markers
     * @param outPixels Output RGBA8 pixel data
     * @param outWidth Output width (384)
     * @param outHeight Output height (216)
     * @return true on success
     */
    static bool GenerateToMemory(
        const Model& model,
        Canvas& canvas,
        IRenderer& renderer,
        IconManager* icons,
        std::vector<uint8_t>& outPixels,
        int& outWidth,
        int& outHeight
    );
    
    // Standard thumbnail dimensions (16:9 aspect ratio)
    static constexpr int WIDTH = 384;
    static constexpr int HEIGHT = 216;
};

} // namespace Cartograph

