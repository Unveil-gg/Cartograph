#pragma once

#include <string>
#include <vector>

namespace Cartograph {

class Model;
class IRenderer;
class Canvas;

/**
 * Export options for PNG export.
 */
struct ExportOptions {
    int scale = 1;              // 1×, 2×, 3×, etc.
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
    bool layerLegend = false;
    
    // Cropping
    enum class CropMode {
        UsedArea,   // Crop to rooms + padding
        FullGrid    // Export entire grid
    } cropMode = CropMode::UsedArea;
    
    int padding = 16;  // Padding in pixels @ 1× scale
    int dpi = 96;      // DPI metadata
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
     * @param path Output path
     * @param options Export options
     * @return true on success
     */
    static bool Export(
        const Model& model,
        Canvas& canvas,
        IRenderer& renderer,
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

