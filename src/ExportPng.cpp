#include "ExportPng.h"
#include "Model.h"
#include "Canvas.h"
#include "render/Renderer.h"
#include "render/GlRenderer.h"
#include <algorithm>

// stb_image_write for PNG export
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

namespace Cartograph {

void ExportPng::CalculateDimensions(
    const Model& model,
    const ExportOptions& options,
    int* outWidth,
    int* outHeight
) {
    // Get actual content bounds
    ContentBounds bounds = model.CalculateContentBounds();
    
    // Handle empty project
    if (bounds.isEmpty) {
        *outWidth = 0;
        *outHeight = 0;
        return;
    }
    
    // Calculate content dimensions in tiles
    int contentWidthTiles = bounds.maxX - bounds.minX + 1;
    int contentHeightTiles = bounds.maxY - bounds.minY + 1;
    
    // Convert to pixels at 1× scale
    int contentWidthPx = contentWidthTiles * model.grid.tileWidth;
    int contentHeightPx = contentHeightTiles * model.grid.tileHeight;
    
    // Add padding (in pixels at 1× scale)
    contentWidthPx += options.padding * 2;
    contentHeightPx += options.padding * 2;
    
    // Apply sizing mode
    if (options.sizeMode == ExportOptions::SizeMode::Scale) {
        // Simple scale multiplier
        *outWidth = contentWidthPx * options.scale;
        *outHeight = contentHeightPx * options.scale;
    } else {
        // Custom dimensions - scale to fit, maintain aspect ratio
        float contentAspect = (float)contentWidthPx / contentHeightPx;
        float targetAspect = (float)options.customWidth / 
                            options.customHeight;
        
        if (contentAspect > targetAspect) {
            // Content wider - fit to width
            *outWidth = options.customWidth;
            *outHeight = (int)(options.customWidth / contentAspect);
        } else {
            // Content taller - fit to height
            *outHeight = options.customHeight;
            *outWidth = (int)(options.customHeight * contentAspect);
        }
    }
    
    // Clamp to safety limits
    *outWidth = std::min(*outWidth, ExportOptions::MAX_DIMENSION);
    *outHeight = std::min(*outHeight, ExportOptions::MAX_DIMENSION);
}

bool ExportPng::Export(
    const Model& model,
    Canvas& canvas,
    IRenderer& renderer,
    IconManager* icons,
    const std::string& path,
    const ExportOptions& options
) {
    // Calculate dimensions
    int width, height;
    CalculateDimensions(model, options, &width, &height);
    
    // Check for empty project
    if (width == 0 || height == 0) {
        return false;
    }
    
    // Get content bounds for centering
    ContentBounds bounds = model.CalculateContentBounds();
    
    // Create offscreen framebuffer
    GlRenderer* glRenderer = dynamic_cast<GlRenderer*>(&renderer);
    if (!glRenderer) {
        return false;
    }
    
    void* fbo = glRenderer->CreateFramebuffer(width, height);
    if (!fbo) {
        return false;
    }
    
    // Set render target
    glRenderer->SetRenderTarget(fbo);
    
    // Clear
    if (options.transparency) {
        glRenderer->Clear(Color(0, 0, 0, 0));
    } else {
        glRenderer->Clear(Color(
            options.bgColorR, 
            options.bgColorG, 
            options.bgColorB, 
            1.0f
        ));
    }
    
    // Temporarily adjust canvas for export
    Canvas exportCanvas = canvas;
    
    // Calculate effective scale based on actual output dimensions
    int contentWidthTiles = bounds.maxX - bounds.minX + 1;
    int contentHeightTiles = bounds.maxY - bounds.minY + 1;
    int contentWidthPx = contentWidthTiles * model.grid.tileWidth + 
                        options.padding * 2;
    int contentHeightPx = contentHeightTiles * model.grid.tileHeight + 
                         options.padding * 2;
    
    float effectiveScale = (float)width / contentWidthPx;
    
    exportCanvas.zoom = effectiveScale;
    exportCanvas.offsetX = bounds.minX * model.grid.tileWidth - 
                          options.padding;
    exportCanvas.offsetY = bounds.minY * model.grid.tileHeight - 
                          options.padding;
    exportCanvas.showGrid = options.layerGrid;
    
    // Create render context for export
    RenderContext exportContext;
    exportContext.skipImGui = true;
    exportContext.showGrid = options.layerGrid;
    exportContext.showTiles = options.layerTiles;
    exportContext.showEdges = options.layerDoors;
    exportContext.showMarkers = options.layerMarkers;
    exportContext.showRooms = false;  // Skip room metadata overlays
    
    // Render with export context
    exportCanvas.Render(
        renderer, model, icons, 
        0, 0, width, height,
        nullptr,  // No hovered edge in export
        false,    // No room overlays
        nullptr,  // No selected marker in export
        nullptr,  // No hovered marker in export
        &exportContext
    );
    
    // Read pixels
    std::vector<uint8_t> pixels(width * height * 4);
    glRenderer->ReadPixels(0, 0, width, height, pixels.data());
    
    // Flip vertically (OpenGL bottom-up -> stb top-down)
    std::vector<uint8_t> flipped(width * height * 4);
    for (int y = 0; y < height; ++y) {
        memcpy(
            flipped.data() + y * width * 4,
            pixels.data() + (height - 1 - y) * width * 4,
            width * 4
        );
    }
    
    // Write PNG
    int result = stbi_write_png(
        path.c_str(),
        width, height,
        4,  // RGBA
        flipped.data(),
        width * 4
    );
    
    // Cleanup
    glRenderer->DestroyFramebuffer(fbo);
    glRenderer->SetRenderTarget(nullptr);
    
    return result != 0;
}

} // namespace Cartograph

