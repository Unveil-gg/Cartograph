#include "ExportPng.h"
#include "Model.h"
#include <algorithm>
#include <cstring>

// stb_image_write for PNG export
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

namespace Cartograph {

// Helper class for direct pixel manipulation
class PixelBuffer {
public:
    PixelBuffer(int width, int height) 
        : m_width(width), m_height(height) {
        m_pixels.resize(width * height * 4, 0);
    }
    
    // Set pixel with bounds checking
    void SetPixel(int x, int y, const Color& color) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
        
        int idx = (y * m_width + x) * 4;
        m_pixels[idx + 0] = static_cast<uint8_t>(color.r * 255);
        m_pixels[idx + 1] = static_cast<uint8_t>(color.g * 255);
        m_pixels[idx + 2] = static_cast<uint8_t>(color.b * 255);
        m_pixels[idx + 3] = static_cast<uint8_t>(color.a * 255);
    }
    
    // Fill rectangle
    void FillRect(int x, int y, int w, int h, const Color& color) {
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) {
                SetPixel(px, py, color);
            }
        }
    }
    
    // Draw horizontal line with thickness
    void DrawHLine(int x1, int x2, int y, const Color& color, int thickness = 1) {
        int halfThick = thickness / 2;
        for (int t = -halfThick; t <= halfThick; ++t) {
            int py = y + t;
            if (py < 0 || py >= m_height) continue;
            
            int startX = std::max(0, std::min(x1, x2));
            int endX = std::min(m_width - 1, std::max(x1, x2));
            for (int x = startX; x <= endX; ++x) {
                SetPixel(x, py, color);
            }
        }
    }
    
    // Draw vertical line with thickness
    void DrawVLine(int x, int y1, int y2, const Color& color, int thickness = 1) {
        int halfThick = thickness / 2;
        for (int t = -halfThick; t <= halfThick; ++t) {
            int px = x + t;
            if (px < 0 || px >= m_width) continue;
            
            int startY = std::max(0, std::min(y1, y2));
            int endY = std::min(m_height - 1, std::max(y1, y2));
            for (int y = startY; y <= endY; ++y) {
                SetPixel(px, y, color);
            }
        }
    }
    
    const uint8_t* Data() const { return m_pixels.data(); }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    
private:
    std::vector<uint8_t> m_pixels;
    int m_width, m_height;
};

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
    
    // Get content bounds
    ContentBounds bounds = model.CalculateContentBounds();
    
    // Calculate effective scale
    int contentWidthTiles = bounds.maxX - bounds.minX + 1;
    int contentHeightTiles = bounds.maxY - bounds.minY + 1;
    int contentWidthPx = contentWidthTiles * model.grid.tileWidth + 
                        options.padding * 2;
    float scale = (float)width / contentWidthPx;
    
    // Create pixel buffer
    PixelBuffer buffer(width, height);
    
    // Clear background
    if (!options.transparency) {
        Color bgColor(options.bgColorR, options.bgColorG, options.bgColorB, 1.0f);
        buffer.FillRect(0, 0, width, height, bgColor);
    }
    
    // Helper to convert tile coordinates to pixel coordinates
    auto tileToPixel = [&](int tileX, int tileY, int* px, int* py) {
        *px = static_cast<int>((tileX - bounds.minX) * model.grid.tileWidth * scale + options.padding * scale);
        *py = static_cast<int>((tileY - bounds.minY) * model.grid.tileHeight * scale + options.padding * scale);
    };
    
    // Render tiles
    if (options.layerTiles) {
        for (const auto& row : model.tiles) {
            for (const auto& run : row.runs) {
                if (run.tileId == 0) continue;  // Skip empty
                
                // Find tile color in palette
                Color tileColor(0.5f, 0.5f, 0.5f, 1.0f);
                for (const auto& tile : model.palette) {
                    if (tile.id == run.tileId) {
                        tileColor = tile.color;
                        break;
                    }
                }
                
                // Draw each tile in the run
                for (int i = 0; i < run.count; ++i) {
                    int tx = run.startX + i;
                    int ty = row.y;
                    
                    int px, py;
                    tileToPixel(tx, ty, &px, &py);
                    
                    int tileW = static_cast<int>(model.grid.tileWidth * scale);
                    int tileH = static_cast<int>(model.grid.tileHeight * scale);
                    
                    buffer.FillRect(px, py, tileW, tileH, tileColor);
                }
            }
        }
    }
    
    // Render edges (walls and doors)
    if (options.layerDoors) {
        // Calculate line thickness based on scale (2px base * scale)
        int thickness = std::max(1, static_cast<int>(2 * scale));
        
        for (const auto& [edgeId, state] : model.edges) {
            if (state == EdgeState::None) continue;
            
            Color lineColor = (state == EdgeState::Wall) ? 
                             model.theme.wallColor : model.theme.doorColor;
            
            // Calculate edge line endpoints in pixel coordinates
            int px1, py1, px2, py2;
            
            // Determine if edge is horizontal or vertical
            bool isVertical = (edgeId.x1 != edgeId.x2);
            
            if (isVertical) {
                // Vertical edge
                int x = std::max(edgeId.x1, edgeId.x2);
                int y = std::min(edgeId.y1, edgeId.y2);
                
                tileToPixel(x, y, &px1, &py1);
                tileToPixel(x, y + 1, &px2, &py2);
                
                buffer.DrawVLine(px1, py1, py2, lineColor, thickness);
            } else {
                // Horizontal edge
                int x = std::min(edgeId.x1, edgeId.x2);
                int y = std::max(edgeId.y1, edgeId.y2);
                
                tileToPixel(x, y, &px1, &py1);
                tileToPixel(x + 1, y, &px2, &py2);
                
                buffer.DrawHLine(px1, px2, py1, lineColor, thickness);
            }
        }
    }
    
    // Render grid
    if (options.layerGrid) {
        Color gridColor(0.2f, 0.2f, 0.2f, 0.5f);
        
        // Vertical lines
        for (int tx = bounds.minX; tx <= bounds.maxX + 1; ++tx) {
            int px, py;
            tileToPixel(tx, bounds.minY, &px, &py);
            int py2 = static_cast<int>((bounds.maxY - bounds.minY + 1) * model.grid.tileHeight * scale + options.padding * scale);
            buffer.DrawVLine(px, py, py2, gridColor);
        }
        
        // Horizontal lines
        for (int ty = bounds.minY; ty <= bounds.maxY + 1; ++ty) {
            int px, py;
            tileToPixel(bounds.minX, ty, &px, &py);
            int px2 = static_cast<int>((bounds.maxX - bounds.minX + 1) * model.grid.tileWidth * scale + options.padding * scale);
            buffer.DrawHLine(px, px2, py, gridColor);
        }
    }
    
    // TODO: Render markers (if needed)
    // For now, markers are skipped as they require icon rendering
    
    // Write PNG
    int result = stbi_write_png(
        path.c_str(),
        buffer.Width(), buffer.Height(),
        4,  // RGBA
        buffer.Data(),
        buffer.Width() * 4
    );
    
    return result != 0;
}

} // namespace Cartograph
