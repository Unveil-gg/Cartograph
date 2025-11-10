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
        *outWidth = model.grid.cols * model.grid.tileSize * options.scale;
        *outHeight = model.grid.rows * model.grid.tileSize * options.scale;
        return;
    }
    
    // Calculate bounding box of all rooms
    if (model.rooms.empty()) {
        *outWidth = 512;
        *outHeight = 512;
        return;
    }
    
    int minX = model.rooms[0].rect.x;
    int minY = model.rooms[0].rect.y;
    int maxX = model.rooms[0].rect.x + model.rooms[0].rect.w;
    int maxY = model.rooms[0].rect.y + model.rooms[0].rect.h;
    
    for (const auto& room : model.rooms) {
        minX = std::min(minX, room.rect.x);
        minY = std::min(minY, room.rect.y);
        maxX = std::max(maxX, room.rect.x + room.rect.w);
        maxY = std::max(maxY, room.rect.y + room.rect.h);
    }
    
    // Add padding
    int paddingTiles = options.padding / model.grid.tileSize;
    minX -= paddingTiles;
    minY -= paddingTiles;
    maxX += paddingTiles;
    maxY += paddingTiles;
    
    *outWidth = (maxX - minX) * model.grid.tileSize * options.scale;
    *outHeight = (maxY - minY) * model.grid.tileSize * options.scale;
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

