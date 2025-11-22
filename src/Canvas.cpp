#include "Canvas.h"
#include "render/Renderer.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Cartograph {

Canvas::Canvas()
    : m_vpX(0), m_vpY(0), m_vpW(0), m_vpH(0)
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
    int viewportW, int viewportH
) {
    m_vpX = viewportX;
    m_vpY = viewportY;
    m_vpW = viewportW;
    m_vpH = viewportH;
    
    // Render in order: grid, rooms, tiles, doors, markers
    if (showGrid) {
        RenderGrid(renderer, model.grid);
    }
    RenderRooms(renderer, model);
    RenderTiles(renderer, model);
    RenderDoors(renderer, model);
    RenderMarkers(renderer, model);
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
    
    for (const auto& room : model.rooms) {
        if (!IsVisible(room.rect, tileWidth, tileHeight)) {
            continue;
        }
        
        // Convert to world space
        float wx = room.rect.x * tileWidth;
        float wy = room.rect.y * tileHeight;
        float ww = room.rect.w * tileWidth;
        float wh = room.rect.h * tileHeight;
        
        // Convert to screen space
        float sx, sy;
        WorldToScreen(wx, wy, &sx, &sy);
        float sw = ww * zoom;
        float sh = wh * zoom;
        
        // Draw fill with room color
        Color fillColor = room.color;
        fillColor.a *= 0.3f;  // Semi-transparent
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
    
    for (const auto& row : model.tiles) {
        const Room* room = model.FindRoom(row.roomId);
        if (!room) continue;
        
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
                int tx = room->rect.x + run.startX + i;
                int ty = room->rect.y + row.y;
                
                float wx, wy;
                TileToWorld(tx, ty, tileWidth, tileHeight, &wx, &wy);
                
                float sx, sy;
                WorldToScreen(wx, wy, &sx, &sy);
                float sw = tileWidth * zoom;
                float sh = tileHeight * zoom;
                
                renderer.DrawRect(sx, sy, sw, sh, tileColor);
            }
        }
    }
}

void Canvas::RenderDoors(IRenderer& renderer, const Model& model) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    const Color doorColor(1.0f, 0.8f, 0.2f, 1.0f);
    
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

