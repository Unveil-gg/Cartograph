#include "CanvasPanel.h"
#include "Modals.h"
#include "../App.h"
#include "../platform/Paths.h"
#include "../platform/System.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>
#include <stb/stb_image.h>
#include <filesystem>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <set>
#include <cstring>

namespace Cartograph {

// ============================================================================
// Helper functions
// ============================================================================

/**
 * Get all tiles along a line from (x0, y0) to (x1, y1).
 * Uses Bresenham's line algorithm to ensure continuous painting.
 */
std::vector<std::pair<int, int>> GetTilesAlongLine(
    int x0, int y0, 
    int x1, int y1
) {
    std::vector<std::pair<int, int>> tiles;
    
    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0;
    int y = y0;
    
    while (true) {
        tiles.push_back({x, y});
        
        if (x == x1 && y == y1) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return tiles;
}

CanvasPanel::CanvasPanel() {
}

CanvasPanel::~CanvasPanel() {
    // Clean up SDL cursors
    if (m_eyedropperCursor) {
        SDL_DestroyCursor(m_eyedropperCursor);
        m_eyedropperCursor = nullptr;
    }
    if (m_zoomCursor) {
        SDL_DestroyCursor(m_zoomCursor);
        m_zoomCursor = nullptr;
    }
    // Don't destroy default cursor - it's owned by SDL
}

void CanvasPanel::InitCursors() {
    if (m_cursorsInitialized) return;
    
    // Get assets directory - try multiple paths for dev vs installed
    std::string assetsDir = Platform::GetAssetsDir();
    std::string toolsDir = assetsDir + "tools/";
    
    // Check if tools directory exists, fallback to source assets for dev
    if (!std::filesystem::exists(toolsDir)) {
        // Try source assets directory (for development)
        const char* basePath = SDL_GetBasePath();
        if (basePath) {
            std::string devPath = std::string(basePath) + 
                "../../../assets/tools/";
            if (std::filesystem::exists(devPath)) {
                toolsDir = devPath;
            }
        }
    }
    
    // Store default cursor for restoration
    m_defaultCursor = SDL_GetDefaultCursor();
    
    // Cursor fill modes
    enum class FillMode {
        None,              // Keep original colors
        WhiteInteriorFill  // Fill interior (enclosed) areas with white
    };
    
    // Helper lambda to load cursor from PNG with fill options
    auto loadCursor = [](const std::string& path, FillMode fillMode, 
                        int hotX, int hotY) -> SDL_Cursor* {
        int width, height, channels;
        unsigned char* pixels = stbi_load(
            path.c_str(), &width, &height, &channels, 4  // Force RGBA
        );
        
        if (!pixels) {
            return nullptr;
        }
        
        // Apply fill mode
        if (fillMode == FillMode::WhiteInteriorFill) {
            // Fill only INTERIOR transparent pixels with white
            // Exterior (connected to edges) stays transparent
            
            // Create visited mask for flood-fill from edges
            std::vector<bool> exterior(width * height, false);
            std::vector<std::pair<int, int>> queue;
            
            // Helper to check if pixel is transparent
            auto isTransparent = [&](int x, int y) {
                if (x < 0 || x >= width || y < 0 || y >= height) 
                    return false;
                return pixels[(y * width + x) * 4 + 3] < 128;
            };
            
            // Seed flood-fill from all transparent edge pixels
            for (int x = 0; x < width; ++x) {
                if (isTransparent(x, 0)) {
                    exterior[x] = true;
                    queue.push_back({x, 0});
                }
                if (isTransparent(x, height - 1)) {
                    exterior[(height - 1) * width + x] = true;
                    queue.push_back({x, height - 1});
                }
            }
            for (int y = 0; y < height; ++y) {
                if (isTransparent(0, y)) {
                    exterior[y * width] = true;
                    queue.push_back({0, y});
                }
                if (isTransparent(width - 1, y)) {
                    exterior[y * width + width - 1] = true;
                    queue.push_back({width - 1, y});
                }
            }
            
            // Flood-fill to mark all exterior transparent pixels
            while (!queue.empty()) {
                auto [cx, cy] = queue.back();
                queue.pop_back();
                
                // Check 4-connected neighbors
                const int dx[] = {-1, 1, 0, 0};
                const int dy[] = {0, 0, -1, 1};
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int idx = ny * width + nx;
                        if (!exterior[idx] && isTransparent(nx, ny)) {
                            exterior[idx] = true;
                            queue.push_back({nx, ny});
                        }
                    }
                }
            }
            
            // Fill interior transparent pixels with white
            for (int i = 0; i < width * height; ++i) {
                unsigned char a = pixels[i * 4 + 3];
                if (a < 128 && !exterior[i]) {
                    // Interior transparent pixel - fill with white
                    pixels[i * 4 + 0] = 255;  // R
                    pixels[i * 4 + 1] = 255;  // G
                    pixels[i * 4 + 2] = 255;  // B
                    pixels[i * 4 + 3] = 255;  // A (fully opaque)
                }
                // Keep strokes and exterior transparent pixels as-is
            }
        }
        // FillMode::None - keep original colors
        
        // Create SDL surface from pixel data
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            width, height,
            SDL_PIXELFORMAT_RGBA32,
            pixels,
            width * 4  // pitch = width * 4 bytes per pixel
        );
        
        if (!surface) {
            stbi_image_free(pixels);
            return nullptr;
        }
        
        // Create cursor from surface
        SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX, hotY);
        
        // Clean up surface (cursor has its own copy)
        SDL_DestroySurface(surface);
        stbi_image_free(pixels);
        
        return cursor;
    };
    
    // Load eyedropper cursor (white interior fill, hotspot at tip)
    // Icons are 32x32, pipette tip is at bottom-left (~x=4, y=28)
    m_eyedropperCursor = loadCursor(
        toolsDir + "pipette.png", 
        FillMode::WhiteInteriorFill,  // White fill inside outlines only
        4, 28   // Hotspot at pipette tip
    );
    
    // Load zoom cursor (keep original black, hotspot at center)
    // Icons are 32x32, center of magnifying glass (~x=12, y=12)
    m_zoomCursor = loadCursor(
        toolsDir + "zoom-in.png",
        FillMode::None,  // Keep original black color
        12, 12  // Hotspot at center of lens
    );
    
    m_cursorsInitialized = true;
}

void CanvasPanel::UpdateCursor() {
    // Only update cursor if we have initialized them
    if (!m_cursorsInitialized) {
        InitCursors();
    }
    
    // Determine which cursor to use
    SDL_Cursor* desiredCursor = m_defaultCursor;
    
    if (isHoveringCanvas) {
        if (currentTool == Tool::Eyedropper && m_eyedropperCursor) {
            desiredCursor = m_eyedropperCursor;
        } else if (currentTool == Tool::Zoom && m_zoomCursor) {
            desiredCursor = m_zoomCursor;
        }
    }
    
    // Only call SDL_SetCursor if cursor needs to change
    // (SDL_GetCursor comparison to avoid unnecessary calls)
    if (SDL_GetCursor() != desiredCursor) {
        SDL_SetCursor(desiredCursor);
    }
}

bool CanvasPanel::DetectEdgeHover(
    float mouseX, float mouseY,
    const Canvas& canvas,
    const GridConfig& grid,
    EdgeId* outEdgeId,
    EdgeSide* outEdgeSide
) {
    // Convert mouse to world coordinates
    float worldX, worldY;
    canvas.ScreenToWorld(mouseX, mouseY, &worldX, &worldY);
    
    // Calculate which tile we're in (Y-up coordinate system)
    int tx = static_cast<int>(std::floor(worldX / grid.tileWidth));
    int ty = -static_cast<int>(std::floor(worldY / grid.tileHeight));
    
    // Calculate position within the tile (0.0 to 1.0)
    // For Y-up: use tile's world Y position (which is -ty * tileHeight)
    float tileWorldX = tx * grid.tileWidth;
    float tileWorldY = -ty * grid.tileHeight;
    float relX = (worldX - tileWorldX) / grid.tileWidth;
    float relY = (worldY - tileWorldY) / grid.tileHeight;
    
    // Clamp to [0, 1] (should already be, but just in case)
    relX = std::max(0.0f, std::min(1.0f, relX));
    relY = std::max(0.0f, std::min(1.0f, relY));
    
    // Threshold for edge detection (configurable)
    float threshold = grid.edgeHoverThreshold;
    
    // Find the closest edge
    float distToNorth = relY;
    float distToSouth = 1.0f - relY;
    float distToWest = relX;
    float distToEast = 1.0f - relX;
    
    float minDist = std::min({distToNorth, distToSouth, distToWest, distToEast});
    
    // Only trigger if within threshold
    if (minDist > threshold) {
        return false;
    }
    
    // Return the closest edge
    if (minDist == distToNorth) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::North);
        *outEdgeSide = EdgeSide::North;
        return true;
    } else if (minDist == distToSouth) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::South);
        *outEdgeSide = EdgeSide::South;
        return true;
    } else if (minDist == distToWest) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::West);
        *outEdgeSide = EdgeSide::West;
        return true;
    } else {  // distToEast
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::East);
        *outEdgeSide = EdgeSide::East;
        return true;
    }
}


} // namespace Cartograph
