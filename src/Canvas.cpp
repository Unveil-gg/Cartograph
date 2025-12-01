#include "Canvas.h"
#include "render/Renderer.h"
#include "render/GlRenderer.h"
#include "Icons.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Cartograph {

Canvas::Canvas()
    : offsetX(0.0f), offsetY(0.0f), zoom(2.5f), showGrid(true),
      m_vpX(0), m_vpY(0), m_vpW(0), m_vpH(0),
      cachedThumbnailWidth(0), cachedThumbnailHeight(0),
      hasCachedThumbnail(false)
{
}

void Canvas::Update(Model& model, float deltaTime) {
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
    const RenderContext* context
) {
    m_vpX = viewportX;
    m_vpY = viewportY;
    m_vpW = viewportW;
    m_vpH = viewportH;
    
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
    if (showEdgesLayer) {
        RenderEdges(renderer, model, hoveredEdge);
        RenderDoors(renderer, model);
    }
    if (showMarkersLayer) {
        RenderMarkers(renderer, model, icons, selectedMarker, hoveredMarker);
    }
    if (showGridLayer) {
        RenderGrid(renderer, model.grid);
    }
    if (showRoomsLayer && showRoomOverlays) {
        RenderRoomOverlays(renderer, model);
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
    zoom = std::clamp(newZoom, 0.25f, 25.0f);
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

void Canvas::RenderMarkers(IRenderer& renderer, const Model& model,
                           IconManager* icons, const Marker* selectedMarker,
                           const Marker* hoveredMarker) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    for (const auto& marker : model.markers) {
        // Convert fractional tile coords to world coords
        // marker.x/y are now floats (e.g., 5.5 = center of tile 5)
        float wx = marker.x * tileWidth;
        float wy = marker.y * tileHeight;
        
        float sx, sy;
        WorldToScreen(wx, wy, &sx, &sy);
        
        // Use minimum dimension to keep markers square and prevent overlap
        float minDim = static_cast<float>(std::min(tileWidth, tileHeight));
        float markerSize = minDim * zoom * marker.size;
        
        // Clamp to prevent overlap (max 80% of min dimension)
        float maxSize = minDim * zoom * 0.8f;
        markerSize = std::min(markerSize, maxSize);
        
        // LOD: Draw full marker if zoomed in enough
        if (zoom >= 0.3f) {
            // Try to get icon texture
            const Icon* icon = nullptr;
            if (icons && !marker.icon.empty()) {
                icon = icons->GetIcon(marker.icon);
            }
            
            if (icon && icons->GetAtlasTexture()) {
                // Draw icon texture with color tint
                ImVec2 pMin(sx - markerSize/2, sy - markerSize/2);
                ImVec2 pMax(sx + markerSize/2, sy + markerSize/2);
                ImVec2 uvMin(icon->u0, icon->v0);
                ImVec2 uvMax(icon->u1, icon->v1);
                
                // Use marker color as tint (or white if transparent)
                ImU32 tintColor = marker.color.a > 0.0f ? 
                    marker.color.ToU32() : IM_COL32(255, 255, 255, 255);
                
                dl->AddImage(
                    icons->GetAtlasTexture(),
                    pMin, pMax,
                    uvMin, uvMax,
                    tintColor
                );
            } else {
                // Fallback: Draw colored square if no icon found
                renderer.DrawRect(
                    sx - markerSize/2, sy - markerSize/2, 
                    markerSize, markerSize, 
                    marker.color
                );
            }
            
            // Draw selection/hover indicators
            bool isSelected = (selectedMarker && 
                             selectedMarker->id == marker.id);
            bool isHovered = (hoveredMarker && 
                            hoveredMarker->id == marker.id);
            
            if (isSelected) {
                // Draw selection indicator (blue outline)
                float padding = 4.0f;
                dl->AddRect(
                    ImVec2(sx - markerSize/2 - padding, 
                          sy - markerSize/2 - padding),
                    ImVec2(sx + markerSize/2 + padding, 
                          sy + markerSize/2 + padding),
                    IM_COL32(100, 150, 255, 255),
                    0.0f,  // No rounding
                    0,     // No flags
                    2.0f   // Thickness
                );
            } else if (isHovered) {
                // Draw hover indicator (light outline)
                float padding = 2.0f;
                dl->AddRect(
                    ImVec2(sx - markerSize/2 - padding, 
                          sy - markerSize/2 - padding),
                    ImVec2(sx + markerSize/2 + padding, 
                          sy + markerSize/2 + padding),
                    IM_COL32(255, 255, 255, 180),
                    0.0f,  // No rounding
                    0,     // No flags
                    1.5f   // Thickness
                );
            }
            
            // Draw label if zoomed in and enabled
            if (zoom > 0.7f && marker.showLabel && !marker.label.empty()) {
                ImVec2 textPos(sx + markerSize/2 + 4, sy - 8);
                dl->AddText(textPos, marker.color.ToU32(), 
                           marker.label.c_str());
            }
        } else {
            // LOD: Draw simplified dot when zoomed out
            float dotSize = 4.0f;  // Fixed pixel size
            renderer.DrawRect(
                sx - dotSize/2, sy - dotSize/2,
                dotSize, dotSize,
                marker.color
            );
            
            // Selection indicator even when zoomed out
            if (selectedMarker && selectedMarker->id == marker.id) {
                dl->AddCircle(
                    ImVec2(sx, sy),
                    6.0f,
                    IM_COL32(100, 150, 255, 255),
                    12,    // Segments
                    2.0f   // Thickness
                );
            }
        }
    }
}

void Canvas::RenderRoomOverlays(IRenderer& renderer, const Model& model) {
    const int tileWidth = model.grid.tileWidth;
    const int tileHeight = model.grid.tileHeight;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
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
        
        // Room overlay color (very transparent)
        Color overlayColor = room.color;
        overlayColor.a = 0.15f;  // Super light overlay
        
        // Room outline color (opaque)
        Color outlineColor = room.color;
        outlineColor.a = 1.0f;
        
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
            
            // Draw overlay
            renderer.DrawRect(sx, sy, sw, sh, overlayColor);
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
                    
                    dl->AddLine(
                        ImVec2(x1, y1), 
                        ImVec2(x2, y2), 
                        outlineColor.ToU32(), 
                        2.0f * std::max(1.0f, zoom)  // Thicker line at higher zoom
                    );
                }
            }
        }
    }
}

void Canvas::CaptureThumbnail(IRenderer& renderer, const Model& model,
                              int viewportX, int viewportY,
                              int viewportW, int viewportH) {
    // Thumbnail dimensions (16:9 aspect ratio)
    const int thumbWidth = 384;
    const int thumbHeight = 216;
    
    // Calculate content bounding box in tile coordinates
    int minTileX = model.grid.cols;
    int minTileY = model.grid.rows;
    int maxTileX = 0;
    int maxTileY = 0;
    bool hasContent = false;
    
    for (const auto& row : model.tiles) {
        if (!row.runs.empty()) {
            for (const auto& run : row.runs) {
                if (run.tileId != 0) {
                    minTileX = std::min(minTileX, run.startX);
                    maxTileX = std::max(maxTileX, run.startX + run.count);
                    minTileY = std::min(minTileY, row.y);
                    maxTileY = std::max(maxTileY, row.y + 1);
                    hasContent = true;
                }
            }
        }
    }
    
    // If no content, capture center of viewport
    if (!hasContent) {
        int centerX = model.grid.cols / 2;
        int centerY = model.grid.rows / 2;
        int viewSize = 20;
        minTileX = std::max(0, centerX - viewSize / 2);
        maxTileX = std::min(model.grid.cols, centerX + viewSize / 2);
        minTileY = std::max(0, centerY - viewSize / 2);
        maxTileY = std::min(model.grid.rows, centerY + viewSize / 2);
    }
    
    // Add padding around content
    int contentWidth = maxTileX - minTileX;
    int contentHeight = maxTileY - minTileY;
    int padding = (contentWidth < 10 || contentHeight < 10) ? 4 : 2;
    
    minTileX = std::max(0, minTileX - padding);
    minTileY = std::max(0, minTileY - padding);
    maxTileX = std::min(model.grid.cols, maxTileX + padding);
    maxTileY = std::min(model.grid.rows, maxTileY + padding);
    
    // Convert content bounds to world coordinates
    float contentMinX = minTileX * model.grid.tileWidth;
    float contentMinY = minTileY * model.grid.tileHeight;
    float contentMaxX = maxTileX * model.grid.tileWidth;
    float contentMaxY = maxTileY * model.grid.tileHeight;
    
    // Convert to screen coordinates
    float screenMinX, screenMinY, screenMaxX, screenMaxY;
    WorldToScreen(contentMinX, contentMinY, &screenMinX, &screenMinY);
    WorldToScreen(contentMaxX, contentMaxY, &screenMaxX, &screenMaxY);
    
    // Calculate capture region (clamped to viewport)
    int captureX = std::max(viewportX, static_cast<int>(screenMinX));
    int captureY = std::max(viewportY, static_cast<int>(screenMinY));
    int captureW = std::min(viewportX + viewportW, 
                           static_cast<int>(screenMaxX)) - captureX;
    int captureH = std::min(viewportY + viewportH,
                           static_cast<int>(screenMaxY)) - captureY;
    
    // Ensure reasonable capture size
    if (captureW <= 0 || captureH <= 0 || 
        captureW > viewportW || captureH > viewportH) {
        // Fall back to full viewport
        captureX = viewportX;
        captureY = viewportY;
        captureW = viewportW;
        captureH = viewportH;
    }
    
    // Read pixels from framebuffer
    GlRenderer* glRenderer = dynamic_cast<GlRenderer*>(&renderer);
    if (!glRenderer) {
        hasCachedThumbnail = false;
        return;
    }
    
    std::vector<uint8_t> capturedPixels(captureW * captureH * 4);
    glRenderer->ReadPixels(captureX, captureY, captureW, captureH,
                          capturedPixels.data());
    
    // Flip vertically (OpenGL reads bottom-up)
    std::vector<uint8_t> flipped(captureW * captureH * 4);
    for (int y = 0; y < captureH; ++y) {
        memcpy(
            flipped.data() + y * captureW * 4,
            capturedPixels.data() + (captureH - 1 - y) * captureW * 4,
            captureW * 4
        );
    }
    
    // Simple resize to thumbnail dimensions using nearest neighbor
    // (Good enough for thumbnails, faster than bilinear)
    cachedThumbnail.resize(thumbWidth * thumbHeight * 4);
    
    float scaleX = static_cast<float>(captureW) / thumbWidth;
    float scaleY = static_cast<float>(captureH) / thumbHeight;
    
    for (int y = 0; y < thumbHeight; ++y) {
        for (int x = 0; x < thumbWidth; ++x) {
            int srcX = std::min(static_cast<int>(x * scaleX), captureW - 1);
            int srcY = std::min(static_cast<int>(y * scaleY), captureH - 1);
            
            int srcIdx = (srcY * captureW + srcX) * 4;
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

