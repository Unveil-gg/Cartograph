#include "ExportPng.h"
#include "ExportPngBuffer.h"
#include "Model.h"
#include "Icons.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// stb_image_write for PNG export. Silence warnings in the vendored header.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace Cartograph {

// Helper class for direct pixel manipulation

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
    Canvas& /*canvas*/,
    IRenderer& /*renderer*/,
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
    // Y-up coordinate system: higher tile Y renders at lower pixel Y (top)
    auto tileToPixel = [&](int tileX, int tileY, int* px, int* py) {
        *px = static_cast<int>(
            (tileX - bounds.minX) * model.grid.tileWidth * scale + 
            options.padding * scale);
        *py = static_cast<int>(
            (bounds.maxY - tileY) * model.grid.tileHeight * scale + 
            options.padding * scale);
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
                // Horizontal edge (Y-up: boundary at minY, not maxY)
                int x = std::min(edgeId.x1, edgeId.x2);
                int y = std::min(edgeId.y1, edgeId.y2);
                
                tileToPixel(x, y, &px1, &py1);
                tileToPixel(x + 1, y, &px2, &py2);
                
                buffer.DrawHLine(px1, px2, py1, lineColor, thickness);
            }
        }
    }
    
    // Render grid (extends into padding for visual continuity)
    if (options.layerGrid) {
        Color gridColor(0.2f, 0.2f, 0.2f, 0.5f);
        
        // Calculate how many extra tiles fit in the padding area
        int paddingTilesX = static_cast<int>(std::ceil(
            static_cast<float>(options.padding) / model.grid.tileWidth)) + 1;
        int paddingTilesY = static_cast<int>(std::ceil(
            static_cast<float>(options.padding) / model.grid.tileHeight)) + 1;
        
        // Extended grid bounds (content + padding on both sides)
        int gridMinX = bounds.minX - paddingTilesX;
        int gridMaxX = bounds.maxX + 1 + paddingTilesX;
        int gridMinY = bounds.minY - paddingTilesY;
        int gridMaxY = bounds.maxY + 1 + paddingTilesY;
        
        // Vertical lines (span full image height)
        for (int tx = gridMinX; tx <= gridMaxX; ++tx) {
            int px, py;
            tileToPixel(tx, bounds.minY, &px, &py);
            // Lines span from top to bottom of image
            buffer.BlendVLine(px, 0, height, gridColor);
        }
        
        // Horizontal lines (span full image width)
        for (int ty = gridMinY; ty <= gridMaxY; ++ty) {
            int px, py;
            tileToPixel(bounds.minX, ty, &px, &py);
            // Lines span from left to right of image
            buffer.BlendHLine(0, width, py, gridColor);
        }
    }
    
    // Render markers
    if (options.layerMarkers) {
        for (const auto& marker : model.markers) {
            // Calculate marker position in pixels
            // Y-up coordinate system: higher tile Y renders at lower pixel Y
            int px = static_cast<int>(
                (marker.x - bounds.minX) * model.grid.tileWidth * scale + 
                options.padding * scale
            );
            int py = static_cast<int>(
                (bounds.maxY - marker.y) * model.grid.tileHeight * scale + 
                options.padding * scale
            );
            
            // Calculate marker size
            int markerSize = static_cast<int>(
                std::min(model.grid.tileWidth, model.grid.tileHeight) * 
                marker.size * scale
            );
            
            // Try to render icon if not using simple markers
            bool iconRendered = false;
            if (!options.useSimpleMarkers && icons && !marker.icon.empty()) {
                std::vector<uint8_t> iconPixels;
                int iconW, iconH;
                std::string iconCategory;
                
                if (icons->GetIconData(marker.icon, iconPixels, 
                                        iconW, iconH, iconCategory)) {
                    buffer.BlitIconScaled(
                        px, py, markerSize,
                        iconPixels.data(), iconW, iconH,
                        marker.color
                    );
                    iconRendered = true;
                }
            }
            
            // Fallback to simple circle if icon not rendered
            if (!iconRendered) {
                int radius = markerSize / 2;
                
                // Draw filled circle for marker
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (dx * dx + dy * dy <= radius * radius) {
                            buffer.BlendPixel(px + dx, py + dy, marker.color);
                        }
                    }
                }
                
                // Draw border (slightly darker)
                Color borderColor(
                    marker.color.r * 0.7f,
                    marker.color.g * 0.7f,
                    marker.color.b * 0.7f,
                    1.0f
                );
                for (int angle = 0; angle < 360; ++angle) {
                    float rad = angle * 3.14159f / 180.0f;
                    int bx = px + static_cast<int>(radius * std::cos(rad));
                    int by = py + static_cast<int>(radius * std::sin(rad));
                    buffer.SetPixel(bx, by, borderColor);
                }
            }
        }
    }
    
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
