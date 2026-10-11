#include "Canvas.h"
#include "render/Renderer.h"
#include "render/GlRenderer.h"
#include "Icons.h"
#include <imgui.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

namespace Cartograph {


void Canvas::RenderRoomOverlays(IRenderer& renderer, const Model& model,
                               const std::string& hoveredRoomId,
                               const std::string& selectedRoomId,
                               const std::string& selectedRegionGroupId) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    
    // Check if any selection is active (for greyscale effect)
    bool hasRoomSelection = !selectedRoomId.empty();
    bool hasRegionSelection = !selectedRegionGroupId.empty();
    bool hasAnySelection = hasRoomSelection || hasRegionSelection;
    
    // Greyscale overlay color for non-selected rooms
    Color greyscaleOverlay(0.4f, 0.4f, 0.4f, 0.45f);
    
    // Draw overlays for each room
    for (const auto& room : model.rooms) {
        // Get all cells assigned to this room
        std::vector<std::pair<int, int>> roomCells;
        for (const auto& assignment : model.cellRoomAssignments) {
            if (assignment.second == room.id) {
                roomCells.push_back(assignment.first);
            }
        }
        
        if (roomCells.empty()) continue;
        
        // Check room highlight state
        bool isRoomSelected = (hasRoomSelection && room.id == selectedRoomId);
        bool isInSelectedRegion = (hasRegionSelection && 
            room.parentRegionGroupId == selectedRegionGroupId);
        bool isHovered = (!hoveredRoomId.empty() && room.id == hoveredRoomId);
        
        // Determine if this room should be highlighted or greyed out
        bool shouldHighlight = isRoomSelected || isInSelectedRegion || isHovered;
        bool shouldGreyscale = hasAnySelection && !shouldHighlight;
        
        // Room overlay color: selected strongest, region medium, default subtle
        Color overlayColor = room.color;
        if (isRoomSelected) {
            overlayColor.a = 0.6f;  // Strongest highlight for room selection
        } else if (isInSelectedRegion) {
            overlayColor.a = 0.45f;  // Medium highlight for region selection
        } else if (isHovered) {
            overlayColor.a = 0.4f;
        } else {
            overlayColor.a = 0.15f;
        }
        
        // Room outline color (opaque, but dimmed if greyscaled)
        Color outlineColor = room.color;
        outlineColor.a = 1.0f;
        if (shouldGreyscale) {
            // Desaturate outline for greyscaled rooms
            float grey = (outlineColor.r + outlineColor.g + outlineColor.b) / 3.0f;
            outlineColor.r = grey * 0.7f;
            outlineColor.g = grey * 0.7f;
            outlineColor.b = grey * 0.7f;
            outlineColor.a = 0.6f;
        }
        
        // Draw filled overlay for each cell
        for (const auto& cell : roomCells) {
            int tx = cell.first;
            int ty = cell.second;
            
            float wx, wy;
            TileToWorld(tx, ty, tileWidth, tileHeight, &wx, &wy);
            
            float sx, sy;
            WorldToScreen(wx, wy, &sx, &sy);
            float sw = tileWidth * zoom;
            float sh = tileHeight * zoom;
            
            // Draw room color overlay
            renderer.DrawRect(sx, sy, sw, sh, overlayColor);
            
            // Apply greyscale overlay on top for non-selected rooms
            if (shouldGreyscale) {
                renderer.DrawRect(sx, sy, sw, sh, greyscaleOverlay);
            }
        }
        
        // Draw outline for perimeter of room
        // For each cell, check which sides are on the perimeter
        for (const auto& cell : roomCells) {
            int cx = cell.first;
            int cy = cell.second;
            
            float wx, wy;
            TileToWorld(cx, cy, tileWidth, tileHeight, &wx, &wy);
            
            float sx, sy;
            WorldToScreen(wx, wy, &sx, &sy);
            float sw = tileWidth * zoom;
            float sh = tileHeight * zoom;
            
            // Check each of 4 sides
            int dx[] = {0, 0, 1, -1};
            int dy[] = {1, -1, 0, 0};
            
            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                
                // Check if adjacent cell is in the same room
                bool adjacentInRoom = false;
                for (const auto& other : roomCells) {
                    if (other.first == nx && other.second == ny) {
                        adjacentInRoom = true;
                        break;
                    }
                }
                
                // If adjacent cell is not in room, draw outline on this side
                if (!adjacentInRoom) {
                    float x1, y1, x2, y2;
                    if (i == 0) { // South
                        x1 = sx; y1 = sy + sh;
                        x2 = sx + sw; y2 = sy + sh;
                    } else if (i == 1) { // North
                        x1 = sx; y1 = sy;
                        x2 = sx + sw; y2 = sy;
                    } else if (i == 2) { // East
                        x1 = sx + sw; y1 = sy;
                        x2 = sx + sw; y2 = sy + sh;
                    } else { // West
                        x1 = sx; y1 = sy;
                        x2 = sx; y2 = sy + sh;
                    }
                    
                    // Adjust outline thickness based on selection state
                    float thickness = 2.0f * std::max(1.0f, zoom);
                    if (isRoomSelected || isInSelectedRegion) {
                        thickness *= 1.5f;  // Thicker outline for selected
                    }
                    
                    dl->AddLine(
                        ImVec2(x1, y1), 
                        ImVec2(x2, y2), 
                        outlineColor.ToU32(), 
                        thickness
                    );
                }
            }
        }
    }
}


void Canvas::CaptureThumbnail(IRenderer& renderer, const Model& model,
                              int viewportX, int viewportY,
                              int viewportW, int viewportH) {
    // Guard: viewport must be valid
    if (viewportW <= 0 || viewportH <= 0) {
        hasCachedThumbnail = false;
        return;
    }
    
    // Thumbnail dimensions (16:9 aspect ratio)
    const int thumbWidth = 384;
    const int thumbHeight = 216;
    
    // Cast to GlRenderer for framebuffer operations
    GlRenderer* glRenderer = dynamic_cast<GlRenderer*>(&renderer);
    if (!glRenderer) {
        hasCachedThumbnail = false;
        return;
    }
    
    // Get actual framebuffer size (handles HiDPI/Retina displays)
    int fbWidth, fbHeight;
    glRenderer->GetDrawableSize(&fbWidth, &fbHeight);
    
    // Get window size for calculating scale factor
    int winWidth, winHeight;
    glRenderer->GetWindowSize(&winWidth, &winHeight);
    
    // Calculate HiDPI scale factor
    float scaleX = static_cast<float>(fbWidth) / winWidth;
    float scaleY = static_cast<float>(fbHeight) / winHeight;
    
    // Convert viewport coordinates to framebuffer pixels (accounting for HiDPI)
    int fbViewportX = static_cast<int>(viewportX * scaleX);
    int fbViewportY = static_cast<int>(viewportY * scaleY);
    int fbViewportW = static_cast<int>(viewportW * scaleX);
    int fbViewportH = static_cast<int>(viewportH * scaleY);
    
    // Convert Y from ImGui (top-left origin) to OpenGL (bottom-left origin)
    int glY = fbHeight - fbViewportY - fbViewportH;
    
    // Read pixels from framebuffer
    std::vector<uint8_t> capturedPixels(fbViewportW * fbViewportH * 4);
    glRenderer->ReadPixels(fbViewportX, glY, fbViewportW, fbViewportH,
                          capturedPixels.data());
    
    // Flip vertically (OpenGL reads bottom-up, we need top-down)
    std::vector<uint8_t> flipped(fbViewportW * fbViewportH * 4);
    for (int y = 0; y < fbViewportH; ++y) {
        memcpy(
            flipped.data() + y * fbViewportW * 4,
            capturedPixels.data() + (fbViewportH - 1 - y) * fbViewportW * 4,
            fbViewportW * 4
        );
    }
    
    // Resize to thumbnail dimensions using bilinear-ish sampling
    cachedThumbnail.resize(thumbWidth * thumbHeight * 4);
    
    float srcScaleX = static_cast<float>(fbViewportW) / thumbWidth;
    float srcScaleY = static_cast<float>(fbViewportH) / thumbHeight;
    
    for (int y = 0; y < thumbHeight; ++y) {
        for (int x = 0; x < thumbWidth; ++x) {
            int srcX = std::min(static_cast<int>(x * srcScaleX), fbViewportW - 1);
            int srcY = std::min(static_cast<int>(y * srcScaleY), fbViewportH - 1);
            
            int srcIdx = (srcY * fbViewportW + srcX) * 4;
            int dstIdx = (y * thumbWidth + x) * 4;
            
            cachedThumbnail[dstIdx + 0] = flipped[srcIdx + 0];
            cachedThumbnail[dstIdx + 1] = flipped[srcIdx + 1];
            cachedThumbnail[dstIdx + 2] = flipped[srcIdx + 2];
            cachedThumbnail[dstIdx + 3] = flipped[srcIdx + 3];
        }
    }
    
    cachedThumbnailWidth = thumbWidth;
    cachedThumbnailHeight = thumbHeight;
    hasCachedThumbnail = true;
}


} // namespace Cartograph

