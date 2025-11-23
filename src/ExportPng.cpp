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
    if (options.cropMode == ExportOptions::CropMode::FullGrid) {
        *outWidth = model.grid.cols * model.grid.tileWidth * 
                    options.scale;
        *outHeight = model.grid.rows * model.grid.tileHeight * 
                     options.scale;
        return;
    }
    
    // Calculate bounding box of all rooms
    if (model.rooms.empty()) {
        *outWidth = 512;
        *outHeight = 512;
        return;
    }
    
    // Use grid bounds for export
    // TODO: Calculate actual bounds from painted tiles or regions
    int minX = 0;
    int minY = 0;
    int maxX = model.grid.cols;
    int maxY = model.grid.rows;
    
    // Add padding (use average of width and height)
    int avgTileSize = (model.grid.tileWidth + model.grid.tileHeight) / 2;
    int paddingTiles = options.padding / avgTileSize;
    minX -= paddingTiles;
    minY -= paddingTiles;
    maxX += paddingTiles;
    maxY += paddingTiles;
    
    *outWidth = (maxX - minX) * model.grid.tileWidth * options.scale;
    *outHeight = (maxY - minY) * model.grid.tileHeight * options.scale;
}

bool ExportPng::Export(
    const Model& model,
    Canvas& canvas,
    IRenderer& renderer,
    const std::string& path,
    const ExportOptions& options
) {
    // Calculate dimensions
    int width, height;
    CalculateDimensions(model, options, &width, &height);
    
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
    exportCanvas.zoom = options.scale;
    exportCanvas.offsetX = 0;
    exportCanvas.offsetY = 0;
    exportCanvas.showGrid = options.layerGrid;
    
    // Render
    // TODO: Apply layer visibility from options
    exportCanvas.Render(renderer, model, 0, 0, width, height);
    
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

