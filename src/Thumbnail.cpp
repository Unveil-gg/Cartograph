#include "Thumbnail.h"
#include "Model.h"
#include "Canvas.h"
#include "render/Renderer.h"
#include "render/GlRenderer.h"
#include "Icons.h"
#include <algorithm>

// stb_image_write for PNG encoding
#define STB_IMAGE_WRITE_IMPLEMENTATION_THUMBNAIL
#include <stb/stb_image_write.h>

namespace Cartograph {

bool Thumbnail::Generate(
    const Model& model,
    Canvas& canvas,
    IRenderer& renderer,
    IconManager* icons,
    const std::string& path
) {
    std::vector<uint8_t> pixels;
    int width, height;
    
    if (!GenerateToMemory(model, canvas, renderer, icons, 
                          pixels, width, height)) {
        return false;
    }
    
    // Write PNG to file
    int result = stbi_write_png(
        path.c_str(),
        width, height,
        4,  // RGBA
        pixels.data(),
        width * 4
    );
    
    return result != 0;
}

bool Thumbnail::GenerateToMemory(
    const Model& model,
    Canvas& canvas,
    IRenderer& renderer,
    IconManager* icons,
    std::vector<uint8_t>& outPixels,
    int& outWidth,
    int& outHeight
) {
    outWidth = WIDTH;
    outHeight = HEIGHT;
    
    // Cast to GlRenderer for framebuffer operations
    GlRenderer* glRenderer = dynamic_cast<GlRenderer*>(&renderer);
    if (!glRenderer) {
        return false;
    }
    
    // Create offscreen framebuffer
    void* fbo = glRenderer->CreateFramebuffer(WIDTH, HEIGHT);
    if (!fbo) {
        return false;
    }
    
    // Set render target
    glRenderer->SetRenderTarget(fbo);
    
    // Clear with transparent background
    glRenderer->Clear(Color(0, 0, 0, 0));
    
    // Calculate zoom to fit entire map
    // Get map bounds in world coordinates
    float mapWidth = model.grid.cols * model.grid.tileWidth;
    float mapHeight = model.grid.rows * model.grid.tileHeight;
    
    // Calculate zoom to fit both dimensions with some padding
    float zoomX = WIDTH / (mapWidth * 1.1f);
    float zoomY = HEIGHT / (mapHeight * 1.1f);
    float fitZoom = std::min(zoomX, zoomY);
    
    // Clamp zoom to reasonable range (don't zoom in too much on tiny maps)
    fitZoom = std::min(fitZoom, 2.0f);
    
    // Create a canvas configured for thumbnail rendering
    Canvas thumbCanvas = canvas;
    thumbCanvas.zoom = fitZoom;
    thumbCanvas.offsetX = -(mapWidth * fitZoom - WIDTH) * 0.5f / fitZoom;
    thumbCanvas.offsetY = -(mapHeight * fitZoom - HEIGHT) * 0.5f / fitZoom;
    thumbCanvas.showGrid = false;  // No grid in thumbnails
    
    // Render the map
    thumbCanvas.Render(
        renderer, model, icons,
        0, 0, WIDTH, HEIGHT,
        nullptr,  // No hovered edge
        true,     // Show room overlays
        nullptr,  // No selected marker
        nullptr   // No hovered marker
    );
    
    // Read pixels from framebuffer
    std::vector<uint8_t> rawPixels(WIDTH * HEIGHT * 4);
    glRenderer->ReadPixels(0, 0, WIDTH, HEIGHT, rawPixels.data());
    
    // Flip vertically (OpenGL is bottom-up, stb expects top-down)
    outPixels.resize(WIDTH * HEIGHT * 4);
    for (int y = 0; y < HEIGHT; ++y) {
        memcpy(
            outPixels.data() + y * WIDTH * 4,
            rawPixels.data() + (HEIGHT - 1 - y) * WIDTH * 4,
            WIDTH * 4
        );
    }
    
    // Cleanup
    glRenderer->DestroyFramebuffer(fbo);
    glRenderer->SetRenderTarget(nullptr);
    
    return true;
}

} // namespace Cartograph

