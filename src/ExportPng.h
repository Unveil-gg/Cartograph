#pragma once

#include <string>
#include <vector>

namespace Cartograph {

class Model;
class IRenderer;
class Canvas;
class IconManager;

/**
 * Export options for PNG export.
 */
struct ExportOptions {
    // Size mode
    enum class SizeMode {
        Scale,           // Use scale multiplier
        CustomDimensions // Explicit width/height
    } sizeMode = SizeMode::Scale;
    
    int scale = 2;              // Scale multiplier (1×, 2×, 3×, 4×)
    int customWidth = 1920;     // Custom dimensions (used if sizeMode = Custom)
    int customHeight = 1080;
    
    // Safety limit
    static constexpr int MAX_DIMENSION = 16384;
    
    // User-configurable padding (pixels at 1× scale)
    int padding = 16;
    
    // Transparency and background
    bool transparency = true;   // Transparent background
    float bgColorR = 1.0f;      // Background color if not transparent
    float bgColorG = 1.0f;
    float bgColorB = 1.0f;
    
    // Layer visibility
    bool layerGrid = true;
    bool layerRoomOutlines = true;
    bool layerRoomLabels = true;
    bool layerTiles = true;
    bool layerDoors = true;
    bool layerMarkers = true;
};

/**
 * PNG export functions.
 */
class ExportPng {
public:
    /**
     * Export model to PNG file.
     * @param model Model to export
     * @param canvas Canvas for rendering
     * @param renderer Renderer
     * @param icons Icon manager (for rendering markers)
     * @param path Output path
     * @param options Export options
     * @return true on success
     */
    static bool Export(
        const Model& model,
        Canvas& canvas,
        IRenderer& renderer,
        IconManager* icons,
        const std::string& path,
        const ExportOptions& options
    );
    
    /**
     * Calculate the output dimensions for export.
     * @param model Model
     * @param options Export options
     * @param outWidth Output width
     * @param outHeight Output height
     */
    static void CalculateDimensions(
        const Model& model,
        const ExportOptions& options,
        int* outWidth,
        int* outHeight
    );
};

} // namespace Cartograph

