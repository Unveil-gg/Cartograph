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


Canvas::Canvas()
    : offsetX(0.0f), offsetY(0.0f), zoom(2.5f), showGrid(true),
      cachedThumbnailWidth(0), cachedThumbnailHeight(0),
      hasCachedThumbnail(false),
      m_vpX(0), m_vpY(0), m_vpW(0), m_vpH(0)
{
}


void Canvas::Update(Model& /*model*/, float /*deltaTime*/) {
    // Input handling will be managed by UI layer
    // This is just a placeholder for any canvas-specific updates
}


void Canvas::Render(
    IRenderer& renderer,
    const Model& model,
    IconManager* icons,
    int viewportX, int viewportY,
    int viewportW, int viewportH,
    const EdgeId* hoveredEdge,
    bool showRoomOverlays,
    const Marker* selectedMarker,
    const Marker* hoveredMarker,
    const RenderContext* context,
    const std::string& hoveredRoomId,
    const std::string& selectedRoomId,
    const std::string& selectedRegionGroupId
) {
    m_vpX = viewportX;
    m_vpY = viewportY;
    m_vpW = viewportW;
    m_vpH = viewportH;
    
    // Apply pending focus once viewport is properly initialized
    // Require minimum 100x100 to avoid applying with placeholder/uninitialized size
    // (when launched via Finder/open, first few renders may have tiny viewport)
    if (m_pendingFocus.pending && m_vpW >= 100 && m_vpH >= 100) {
        m_pendingFocus.pending = false;
        FocusOnRect(m_pendingFocus.minTx, m_pendingFocus.minTy,
                    m_pendingFocus.maxTx, m_pendingFocus.maxTy,
                    m_pendingFocus.tileWidth, m_pendingFocus.tileHeight,
                    m_pendingFocus.padding);
    }
    
    // Only use ImGui clip rect if not in export mode
    ImDrawList* dl = nullptr;
    if (!context || !context->skipImGui) {
        dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(
            ImVec2(static_cast<float>(viewportX), 
                   static_cast<float>(viewportY)),
            ImVec2(static_cast<float>(viewportX + viewportW), 
                   static_cast<float>(viewportY + viewportH)),
            true  // Intersect with current clip rect
        );
    }
    
    // Determine layer visibility from context or defaults
    bool showGridLayer = context ? context->showGrid : showGrid;
    bool showRoomsLayer = context ? context->showRooms : showRoomOverlays;
    bool showTilesLayer = context ? context->showTiles : true;
    bool showEdgesLayer = context ? context->showEdges : true;
    bool showMarkersLayer = context ? context->showMarkers : true;
    
    // Render in order: rooms, tiles, edges, doors, markers, grid, overlays
    // Grid is rendered after tiles so it appears on top of painted cells
    if (showRoomsLayer) {
        RenderRooms(renderer, model, context);
    }
    if (showTilesLayer) {
        RenderTiles(renderer, model);
    }
    if (showTilesLayer) {
        RenderInterior(renderer, model);
    }
    if (showEdgesLayer) {
        RenderEdges(renderer, model, hoveredEdge);
        RenderDoors(renderer, model);
        RenderDoorGates(renderer, model);
    }
    if (showMarkersLayer) {
        RenderMarkers(renderer, model, icons, selectedMarker, hoveredMarker);
    }
    if (showGridLayer) {
        RenderGrid(renderer, model.grid);
    }
    if (!context || !context->skipImGui) {
        RenderValidationOverlay(renderer, model);
    }
    if (showRoomsLayer && showRoomOverlays) {
        RenderRoomOverlays(renderer, model, hoveredRoomId, selectedRoomId,
                          selectedRegionGroupId);
    }
    
    // Pop clip rect only if we pushed it
    if (dl) {
        dl->PopClipRect();
    }
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
    // Y-up coordinate system: negate the floor result, not the input
    // Note: floor(-x) != -floor(x) for non-integers
    *ty = -static_cast<int>(std::floor(wy / tileHeight));
}


void Canvas::TileToWorld(
    int tx, int ty, 
    int tileWidth, int tileHeight, 
    float* wx, float* wy
) const {
    *wx = tx * tileWidth;
    // Y-up coordinate system: negate tile Y for world coordinates
    *wy = -ty * tileHeight;
}


void Canvas::SetZoom(float newZoom) {
    zoom = std::clamp(newZoom, 0.25f, 25.0f);
}


void Canvas::ZoomToPoint(float newZoom, float screenX, float screenY) {
    // Get world position under cursor before zoom
    float worldX, worldY;
    ScreenToWorld(screenX, screenY, &worldX, &worldY);
    
    // Apply new zoom
    float clampedZoom = std::clamp(newZoom, 0.25f, 25.0f);
    
    // Adjust offset so world position stays under cursor
    // After zoom: screenX = (worldX - offsetX) * newZoom + vpX
    // Solve for offsetX: offsetX = worldX - (screenX - vpX) / newZoom
    offsetX = worldX - (screenX - m_vpX) / clampedZoom;
    offsetY = worldY - (screenY - m_vpY) / clampedZoom;
    
    zoom = clampedZoom;
}


void Canvas::Pan(float dx, float dy) {
    offsetX += dx / zoom;
    offsetY += dy / zoom;
}


void Canvas::FocusOnTile(int tx, int ty, int tileWidth, int tileHeight) {
    offsetX = tx * tileWidth - m_vpW / (2.0f * zoom);
    // Y-up coordinate system: negate tile Y for world offset
    offsetY = -ty * tileHeight - m_vpH / (2.0f * zoom);
}


void Canvas::FocusOnRect(int minTx, int minTy, int maxTx, int maxTy,
                         int tileWidth, int tileHeight, float padding) {
    // If viewport not yet known, defer focus to first render
    if (m_vpW <= 0 || m_vpH <= 0) {
        m_pendingFocus.pending = true;
        m_pendingFocus.minTx = minTx;
        m_pendingFocus.minTy = minTy;
        m_pendingFocus.maxTx = maxTx;
        m_pendingFocus.maxTy = maxTy;
        m_pendingFocus.tileWidth = tileWidth;
        m_pendingFocus.tileHeight = tileHeight;
        m_pendingFocus.padding = padding;
        return;
    }
    
    // Calculate content dimensions in world pixels
    float contentW = (maxTx - minTx + 1) * tileWidth;
    float contentH = (maxTy - minTy + 1) * tileHeight;
    
    // Calculate center of content area in world pixels
    // Y-up coordinate system: negate tile Y for world center
    float centerX = (minTx + maxTx + 1) * 0.5f * tileWidth;
    float centerY = -(minTy + maxTy + 1) * 0.5f * tileHeight;
    
    // Calculate zoom to fit content with padding
    float zoomX = m_vpW / (contentW * padding);
    float zoomY = m_vpH / (contentH * padding);
    float fitZoom = std::min(zoomX, zoomY);
    
    // Clamp zoom to reasonable range
    zoom = std::clamp(fitZoom, 0.5f, DEFAULT_ZOOM * 1.5f);
    
    // Center offset on the bounding box center
    offsetX = centerX - m_vpW / (2.0f * zoom);
    offsetY = centerY - m_vpH / (2.0f * zoom);
}


bool Canvas::IsVisible(
    const Rect& rect, 
    int tileWidth, int tileHeight
) const {
    // Convert tile rect to world space
    // Y-up coordinate system: negate tile Y for world coordinates
    float wx1 = rect.x * tileWidth;
    float wy1 = -rect.y * tileHeight;
    float wx2 = (rect.x + rect.w) * tileWidth;
    float wy2 = -(rect.y + rect.h) * tileHeight;
    
    // Ensure wy1 < wy2 for proper overlap check (since Y is negated)
    if (wy1 > wy2) std::swap(wy1, wy2);
    
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
    
    // Calculate visible tile range from viewport (infinite grid - no clamping)
    int tx1, ty1, tx2, ty2;
    ScreenToTile(m_vpX, m_vpY, tileWidth, tileHeight, &tx1, &ty1);
    ScreenToTile(m_vpX + m_vpW, m_vpY + m_vpH, tileWidth, tileHeight, 
                 &tx2, &ty2);
    
    // With Y-up coords, ty1 (top of screen) > ty2 (bottom), so swap if needed
    int minTx = std::min(tx1, tx2);
    int maxTx = std::max(tx1, tx2);
    int minTy = std::min(ty1, ty2);
    int maxTy = std::max(ty1, ty2);
    
    // Add padding for edge visibility
    minTx -= 1;
    minTy -= 1;
    maxTx += 1;
    maxTy += 1;
    
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


void Canvas::RenderRooms(IRenderer& renderer, const Model& model,
                        const RenderContext* context) {
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
        // Y-up coordinate system: negate tile Y and adjust for height
        float wx = bbox.x * tileWidth;
        float wy = -(bbox.y + bbox.h) * tileHeight;
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
        
        // Draw outline (thin so walls/doors are visible)
        renderer.DrawRectOutline(sx, sy, sw, sh, room.color, 1.0f);
        
        // Draw room name only if not in export mode
        if (zoom > 0.5f && (!context || !context->skipImGui)) {
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


} // namespace Cartograph

