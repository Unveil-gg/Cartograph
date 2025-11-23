#include "Canvas.h"
#include "render/Renderer.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Cartograph {

Canvas::Canvas()
    : offsetX(0.0f), offsetY(0.0f), zoom(1.0f), showGrid(true),
      m_vpX(0), m_vpY(0), m_vpW(0), m_vpH(0)
{
}

void Canvas::Update(Model& model, float deltaTime) {
    // Input handling will be managed by UI layer
    // This is just a placeholder for any canvas-specific updates
}

void Canvas::Render(
    IRenderer& renderer,
    const Model& model,
    int viewportX, int viewportY,
    int viewportW, int viewportH,
    const EdgeId* hoveredEdge
) {
    m_vpX = viewportX;
    m_vpY = viewportY;
    m_vpW = viewportW;
    m_vpH = viewportH;
    
    // Use ImGui's clip rect to prevent drawing outside canvas bounds
    // This works with GetForegroundDrawList()
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->PushClipRect(
        ImVec2(static_cast<float>(viewportX), 
               static_cast<float>(viewportY)),
        ImVec2(static_cast<float>(viewportX + viewportW), 
               static_cast<float>(viewportY + viewportH)),
        true  // Intersect with current clip rect
    );
    
    // Render in order: grid, rooms, tiles, edges, doors, markers
    if (showGrid) {
        RenderGrid(renderer, model.grid);
    }
    RenderRooms(renderer, model);
    RenderTiles(renderer, model);
    RenderEdges(renderer, model, hoveredEdge);
    RenderDoors(renderer, model);
    RenderMarkers(renderer, model);
    
    // Pop clip rect
    dl->PopClipRect();
}

void Canvas::ScreenToWorld(float sx, float sy, float* wx, float* wy) const {
    *wx = (sx - m_vpX) / zoom + offsetX;
    *wy = (sy - m_vpY) / zoom + offsetY;
}

void Canvas::WorldToScreen(float wx, float wy, float* sx, float* sy) const {
    *sx = (wx - offsetX) * zoom + m_vpX;
    *sy = (wy - offsetY) * zoom + m_vpY;
}

void Canvas::ScreenToTile(
    float sx, float sy, 
    int tileWidth, int tileHeight, 
    int* tx, int* ty
) const {
    float wx, wy;
    ScreenToWorld(sx, sy, &wx, &wy);
    *tx = static_cast<int>(std::floor(wx / tileWidth));
    *ty = static_cast<int>(std::floor(wy / tileHeight));
}

void Canvas::TileToWorld(
    int tx, int ty, 
    int tileWidth, int tileHeight, 
    float* wx, float* wy
) const {
    *wx = tx * tileWidth;
    *wy = ty * tileHeight;
}

void Canvas::SetZoom(float newZoom) {
    zoom = std::clamp(newZoom, 0.1f, 10.0f);
}

void Canvas::Pan(float dx, float dy) {
    offsetX += dx / zoom;
    offsetY += dy / zoom;
}

void Canvas::FocusOnTile(int tx, int ty, int tileWidth, int tileHeight) {
    offsetX = tx * tileWidth - m_vpW / (2.0f * zoom);
    offsetY = ty * tileHeight - m_vpH / (2.0f * zoom);
}

bool Canvas::IsVisible(
    const Rect& rect, 
    int tileWidth, int tileHeight
) const {
    // Convert tile rect to world space
    float wx1 = rect.x * tileWidth;
    float wy1 = rect.y * tileHeight;
    float wx2 = (rect.x + rect.w) * tileWidth;
    float wy2 = (rect.y + rect.h) * tileHeight;
    
    // Convert to screen space
    float sx1, sy1, sx2, sy2;
    WorldToScreen(wx1, wy1, &sx1, &sy1);
    WorldToScreen(wx2, wy2, &sx2, &sy2);
    
    // Check overlap with viewport
    return !(sx2 < m_vpX || sx1 > m_vpX + m_vpW ||
             sy2 < m_vpY || sy1 > m_vpY + m_vpH);
}

void Canvas::RenderGrid(IRenderer& renderer, const GridConfig& grid) {
    const int tileWidth = grid.tileWidth;
    const int tileHeight = grid.tileHeight;
    const Color gridColor(0.2f, 0.2f, 0.2f, 0.5f);
    
    // Calculate visible tile range
    int minTx, minTy, maxTx, maxTy;
    ScreenToTile(m_vpX, m_vpY, tileWidth, tileHeight, &minTx, &minTy);
    ScreenToTile(m_vpX + m_vpW, m_vpY + m_vpH, tileWidth, tileHeight, 
                 &maxTx, &maxTy);
    
    // Clamp to grid bounds
    minTx = std::max(0, minTx - 1);
    minTy = std::max(0, minTy - 1);
    maxTx = std::min(grid.cols, maxTx + 1);
    maxTy = std::min(grid.rows, maxTy + 1);
    
    // Draw vertical lines
    for (int tx = minTx; tx <= maxTx; ++tx) {
        float wx1, wy1, wx2, wy2;
        TileToWorld(tx, minTy, tileWidth, tileHeight, &wx1, &wy1);
        TileToWorld(tx, maxTy, tileWidth, tileHeight, &wx2, &wy2);
        
        float sx1, sy1, sx2, sy2;
        WorldToScreen(wx1, wy1, &sx1, &sy1);
        WorldToScreen(wx2, wy2, &sx2, &sy2);
        
        renderer.DrawLine(sx1, sy1, sx2, sy2, gridColor, 1.0f);
    }
    
    // Draw horizontal lines
    for (int ty = minTy; ty <= maxTy; ++ty) {
        float wx1, wy1, wx2, wy2;
        TileToWorld(minTx, ty, tileWidth, tileHeight, &wx1, &wy1);
        TileToWorld(maxTx, ty, tileWidth, tileHeight, &wx2, &wy2);
        
        float sx1, sy1, sx2, sy2;
        WorldToScreen(wx1, wy1, &sx1, &sy1);
        WorldToScreen(wx2, wy2, &sx2, &sy2);
        
        renderer.DrawLine(sx1, sy1, sx2, sy2, gridColor, 1.0f);
    }
}

void Canvas::RenderRooms(IRenderer& renderer, const Model& model) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    
    // Render named rooms (those promoted from regions)
    for (const auto& room : model.rooms) {
        // Find the region this room references
        const Region* region = nullptr;
        for (const auto& r : model.inferredRegions) {
            if (r.id == room.regionId) {
                region = &r;
                break;
            }
        }
        
        if (!region) continue;  // Room references invalid region
        
        const Rect& bbox = region->boundingBox;
        if (!IsVisible(bbox, tileWidth, tileHeight)) {
            continue;
        }
        
        // Convert bounding box to world space
        float wx = bbox.x * tileWidth;
        float wy = bbox.y * tileHeight;
        float ww = bbox.w * tileWidth;
        float wh = bbox.h * tileHeight;
        
        // Convert to screen space
        float sx, sy;
        WorldToScreen(wx, wy, &sx, &sy);
        float sw = ww * zoom;
        float sh = wh * zoom;
        
        // Draw fill with room color (semi-transparent)
        Color fillColor = room.color;
        fillColor.a *= 0.3f;
        renderer.DrawRect(sx, sy, sw, sh, fillColor);
        
        // Draw outline
        renderer.DrawRectOutline(sx, sy, sw, sh, room.color, 2.0f);
        
        // Draw room name (if zoom is high enough)
        if (zoom > 0.5f) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            ImVec2 textPos(sx + 4, sy + 4);
            dl->AddText(textPos, IM_COL32(255, 255, 255, 255), 
                room.name.c_str());
        }
    }
}

void Canvas::RenderTiles(IRenderer& renderer, const Model& model) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    
    // Render all tiles (no room requirement - uses absolute coordinates)
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
            // Note: run.startX and row.y are absolute grid coordinates
            for (int i = 0; i < run.count; ++i) {
                // Use absolute tile coordinates
                int tx = run.startX + i;
                int ty = row.y;
                
                // Convert tile coordinates to world pixel coordinates
                float wx, wy;
                TileToWorld(tx, ty, tileWidth, tileHeight, &wx, &wy);
                
                // Convert world to screen coordinates
                float sx, sy;
                WorldToScreen(wx, wy, &sx, &sy);
                float sw = tileWidth * zoom;
                float sh = tileHeight * zoom;
                
                renderer.DrawRect(sx, sy, sw, sh, tileColor);
            }
        }
    }
}

void Canvas::RenderEdges(
    IRenderer& renderer,
    const Model& model,
    const EdgeId* hoveredEdge
) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    const Color wallColor = model.theme.wallColor;
    const Color doorColor = model.theme.doorColor;
    const Color hoverColor = model.theme.edgeHoverColor;
    
    // Render all edges in the model
    for (const auto& [edgeId, state] : model.edges) {
        if (state == EdgeState::None) continue;
        
        // Calculate edge line endpoints in world coordinates
        float wx1 = edgeId.x1 * tileWidth;
        float wy1 = edgeId.y1 * tileHeight;
        float wx2 = edgeId.x2 * tileWidth;
        float wy2 = edgeId.y2 * tileHeight;
        
        // Determine which edge this is (horizontal or vertical)
        bool isVertical = (edgeId.x1 != edgeId.x2);
        
        if (isVertical) {
            // Vertical edge - draw between x1 and x2
            wx1 = std::max(edgeId.x1, edgeId.x2) * tileWidth;
            wx2 = wx1;
            wy1 = std::min(edgeId.y1, edgeId.y2) * tileHeight;
            wy2 = wy1 + tileHeight;
        } else {
            // Horizontal edge - draw between y1 and y2
            wy1 = std::max(edgeId.y1, edgeId.y2) * tileHeight;
            wy2 = wy1;
            wx1 = std::min(edgeId.x1, edgeId.x2) * tileWidth;
            wx2 = wx1 + tileWidth;
        }
        
        // Convert to screen coordinates
        float sx1, sy1, sx2, sy2;
        WorldToScreen(wx1, wy1, &sx1, &sy1);
        WorldToScreen(wx2, wy2, &sx2, &sy2);
        
        // Determine color and style
        Color lineColor = (state == EdgeState::Wall) ? wallColor : doorColor;
        float thickness = 2.0f * zoom;
        
        // Check if this edge is hovered
        bool isHovered = false;
        if (hoveredEdge) {
            isHovered = (edgeId == *hoveredEdge);
        }
        
        if (isHovered) {
            // Draw hover highlight (thicker, semi-transparent)
            renderer.DrawLine(sx1, sy1, sx2, sy2, hoverColor, 
                            thickness * 2.0f);
        }
        
        // Draw the edge
        if (state == EdgeState::Wall) {
            // Solid line for walls
            renderer.DrawLine(sx1, sy1, sx2, sy2, lineColor, thickness);
        } else {
            // Dashed line for doors
            float dx = sx2 - sx1;
            float dy = sy2 - sy1;
            float len = std::sqrt(dx * dx + dy * dy);
            float dashLen = 8.0f;
            float gapLen = 4.0f;
            float totalLen = dashLen + gapLen;
            int numDashes = static_cast<int>(len / totalLen);
            
            for (int i = 0; i <= numDashes; ++i) {
                float t1 = (i * totalLen) / len;
                float t2 = std::min(1.0f, (i * totalLen + dashLen) / len);
                
                float dx1 = sx1 + dx * t1;
                float dy1 = sy1 + dy * t1;
                float dx2 = sx1 + dx * t2;
                float dy2 = sy1 + dy * t2;
                
                renderer.DrawLine(dx1, dy1, dx2, dy2, lineColor, thickness);
            }
        }
    }
}

void Canvas::RenderDoors(IRenderer& renderer, const Model& model) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    const Color doorColor(1.0f, 0.8f, 0.2f, 1.0f);
    
    // Legacy door rendering (for compatibility)
    for (const auto& door : model.doors) {
        // Draw both endpoints
        for (int ep = 0; ep < 2; ++ep) {
            const auto& endpoint = (ep == 0) ? door.a : door.b;
            
            float wx, wy;
            TileToWorld(endpoint.x, endpoint.y, tileWidth, tileHeight, 
                        &wx, &wy);
            
            float sx, sy;
            WorldToScreen(wx, wy, &sx, &sy);
            // Use average of width and height for door size
            float avgSize = (tileWidth + tileHeight) / 2.0f;
            float size = avgSize * zoom * 0.5f;
            
            // Draw as a small square
            renderer.DrawRect(
                sx - size/2, sy - size/2, 
                size, size, 
                doorColor
            );
        }
    }
}

void Canvas::RenderMarkers(IRenderer& renderer, const Model& model) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    
    for (const auto& marker : model.markers) {
        float wx, wy;
        TileToWorld(marker.x, marker.y, tileWidth, tileHeight, &wx, &wy);
        
        float sx, sy;
        WorldToScreen(wx, wy, &sx, &sy);
        // Use average of width and height for marker size
        float avgSize = (tileWidth + tileHeight) / 2.0f;
        float size = avgSize * zoom * 0.7f;
        
        // Draw as a colored circle (approximated with rect for now)
        renderer.DrawRect(
            sx - size/2, sy - size/2, 
            size, size, 
            marker.color
        );
        
        // Draw label if zoomed in
        if (zoom > 0.7f && !marker.label.empty()) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            ImVec2 textPos(sx + size/2 + 2, sy);
            dl->AddText(textPos, marker.color.ToU32(), 
                        marker.label.c_str());
        }
    }
}

} // namespace Cartograph

