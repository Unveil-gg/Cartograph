#include "Thumbnail.h"
#include "Model.h"
#include "Canvas.h"
#include "render/Renderer.h"
#include "render/GlRenderer.h"
#include "Icons.h"
#include <algorithm>
#include <climits>

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
    
    // Create offscreen framebuffer (RAII-managed)
    auto fbo = glRenderer->CreateFramebuffer(WIDTH, HEIGHT);
    if (!fbo) {
        return false;
    }
    
    // Set render target (pass raw pointer, ownership stays with unique_ptr)
    glRenderer->SetRenderTarget(fbo.get());
    
    // Clear with editor background color (dark gray, not transparent)
    glRenderer->Clear(Color(0.1f, 0.1f, 0.12f, 1.0f));
    
    // Calculate bounding box of actual drawn content (painted tiles only)
    // Use INT_MAX/INT_MIN to properly handle negative coordinates
    int minX = INT_MAX;
    int minY = INT_MAX;
    int maxX = INT_MIN;
    int maxY = INT_MIN;
    bool hasContent = false;
    
    // Check painted tiles - this is what actually matters for visual preview
    for (const auto& row : model.tiles) {
        if (!row.runs.empty()) {
            for (const auto& run : row.runs) {
                if (run.tileId != 0) {  // Non-empty tile
                    minX = std::min(minX, run.startX);
                    maxX = std::max(maxX, run.startX + run.count);
                    minY = std::min(minY, row.y);
                    maxY = std::max(maxY, row.y + 1);
                    hasContent = true;
                }
            }
        }
    }
    
    // If nothing drawn, show center portion of grid (empty project view)
    if (!hasContent) {
        int centerX = model.grid.cols / 2;
        int centerY = model.grid.rows / 2;
        int viewSize = 20;  // Show 20x20 cell area
        minX = centerX - viewSize / 2;
        maxX = centerX + viewSize / 2;
        minY = centerY - viewSize / 2;
        maxY = centerY + viewSize / 2;
    }
    
    // Add padding around content (no clamping to support negative coords)
    int contentWidth = maxX - minX;
    int contentHeight = maxY - minY;
    int padding = (contentWidth < 10 || contentHeight < 10) ? 4 : 2;
    
    minX -= padding;
    minY -= padding;
    maxX += padding;
    maxY += padding;
    
    // Calculate dimensions based on content bounds (in world pixels)
    float contentWidthPx = (maxX - minX) * model.grid.tileWidth;
    float contentHeightPx = (maxY - minY) * model.grid.tileHeight;
    
    // Calculate zoom to fit content (with slight padding factor)
    float zoomX = WIDTH / (contentWidthPx * 1.1f);
    float zoomY = HEIGHT / (contentHeightPx * 1.1f);
    float fitZoom = std::min(zoomX, zoomY);
    
    // Clamp zoom to reasonable range
    fitZoom = std::clamp(fitZoom, 0.5f, 4.0f);
    
    // Calculate center of content area (in world pixels)
    float contentCenterX = (minX + maxX) * 0.5f * model.grid.tileWidth;
    float contentCenterY = (minY + maxY) * 0.5f * model.grid.tileHeight;
    
    // Create a canvas configured for thumbnail rendering
    Canvas thumbCanvas = canvas;
    thumbCanvas.zoom = fitZoom;
    thumbCanvas.offsetX = contentCenterX - (WIDTH * 0.5f / fitZoom);
    thumbCanvas.offsetY = contentCenterY - (HEIGHT * 0.5f / fitZoom);
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
    
    // Cleanup - FBO automatically destroyed by unique_ptr
    glRenderer->SetRenderTarget(nullptr);
    
    return true;
}

} // namespace Cartograph

