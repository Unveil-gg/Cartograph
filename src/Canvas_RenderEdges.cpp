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


void Canvas::EdgeToScreen(
    const GridConfig& grid,
    const EdgeId& edgeId,
    float* sx1, float* sy1,
    float* sx2, float* sy2
) const {
    const int tileWidth = grid.tileWidth;
    const int tileHeight = grid.tileHeight;
    float wx1, wy1, wx2, wy2;
    int minY = std::min(edgeId.y1, edgeId.y2);
    
    // Y-up coordinate system: negate tile Y for world coordinates
    if (edgeId.x1 != edgeId.x2) {
        // Vertical edge: spans one tile height between two tiles
        wx1 = std::max(edgeId.x1, edgeId.x2) * tileWidth;
        wx2 = wx1;
        wy1 = -minY * tileHeight;
        wy2 = wy1 + tileHeight;  // + goes down on screen
    } else {
        // Horizontal edge: spans one tile width between two tiles
        wy1 = -minY * tileHeight;
        wy2 = wy1;
        wx1 = std::min(edgeId.x1, edgeId.x2) * tileWidth;
        wx2 = wx1 + tileWidth;
    }
    WorldToScreen(wx1, wy1, sx1, sy1);
    WorldToScreen(wx2, wy2, sx2, sy2);
}


void Canvas::RenderEdges(
    IRenderer& renderer,
    const Model& model,
    const EdgeId* hoveredEdge
) {
    const Color wallColor = model.theme.wallColor;
    const Color doorColor = model.theme.doorColor;
    const Color hoverColor = model.theme.edgeHoverColor;
    
    // Render all edges in the model
    for (const auto& [edgeId, state] : model.edges) {
        if (state == EdgeState::None) continue;
        
        // Edge line endpoints in screen coordinates
        float sx1, sy1, sx2, sy2;
        EdgeToScreen(model.grid, edgeId, &sx1, &sy1, &sx2, &sy2);
        
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
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    
    for (const auto& marker : model.markers) {
        // Convert fractional tile coords to world coords
        // marker.x/y are now floats (e.g., 5.5 = center of tile 5)
        // Y-up coordinate system: negate tile Y for world coordinates
        float wx = marker.x * tileWidth;
        float wy = -marker.y * tileHeight;
        
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
            
            // Draw label based on display mode
            if (zoom > 0.7f && marker.showLabel && !marker.label.empty()) {
                bool shouldShowLabel = false;
                
                switch (markerLabelMode) {
                    case MarkerLabelMode::Always:
                        shouldShowLabel = true;
                        break;
                    case MarkerLabelMode::Never:
                        shouldShowLabel = false;
                        break;
                    case MarkerLabelMode::HoverOnly:
                        shouldShowLabel = isHovered || isSelected;
                        break;
                }
                
                if (shouldShowLabel) {
                    ImVec2 textPos(sx + markerSize/2 + 4, sy - 8);
                    dl->AddText(textPos, marker.color.ToU32(), 
                               marker.label.c_str());
                }
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


} // namespace Cartograph

