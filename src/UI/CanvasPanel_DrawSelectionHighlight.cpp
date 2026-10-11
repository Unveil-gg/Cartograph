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

void CanvasPanel::DrawSelectionHighlight(
    Model& model,
    Canvas& canvas,
    ImDrawList* drawList
) {
        // Selection colors (using theme)
        ImU32 tileFillColor = ImGui::GetColorU32(
            model.theme.selectionFill.ToImVec4()
        );
        ImU32 tileBorderColor = ImGui::GetColorU32(
            model.theme.selectionBorder.ToImVec4()
        );
        ImU32 edgeColor = ImGui::GetColorU32(
            ImVec4(model.theme.selectionBorder.r, 
                   model.theme.selectionBorder.g + 0.2f,
                   model.theme.selectionBorder.b, 0.9f)
        );
        ImU32 markerColor = ImGui::GetColorU32(
            ImVec4(1.0f, 0.8f, 0.2f, 0.9f)  // Keep distinct marker highlight
        );
        
        float tileW = model.grid.tileWidth * canvas.zoom;
        float tileH = model.grid.tileHeight * canvas.zoom;
        
        // Draw highlight overlay for selected tiles
        for (const auto& tilePair : currentSelection.tiles) {
            int tx = tilePair.first.first;
            int ty = tilePair.first.second;
            
            // Convert tile to screen coordinates
            float wx, wy;
            canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx, &wy);
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            // Draw highlight overlay
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + tileW, sy + tileH),
                tileFillColor
            );
        }
        
        // Draw bounding box around entire selection
        int boundsX = currentSelection.bounds.x;
        int boundsY = currentSelection.bounds.y;
        
        float bx, by;
        canvas.TileToWorld(
            boundsX, boundsY,
            model.grid.tileWidth, model.grid.tileHeight, &bx, &by
        );
        float bsx, bsy;
        canvas.WorldToScreen(bx, by, &bsx, &bsy);
        
        float boxW = currentSelection.bounds.w * tileW;
        float boxH = currentSelection.bounds.h * tileH;
        
        // Draw marching ants border (alternating dash pattern)
        float dashLen = 6.0f;
        float time = static_cast<float>(ImGui::GetTime());
        float offset = std::fmod(time * 20.0f, dashLen * 2.0f);
        
        // Draw dashed rectangle
        auto drawDashedLine = [&](ImVec2 p1, ImVec2 p2) {
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.1f) return;
            
            float ux = dx / len;
            float uy = dy / len;
            
            float pos = -offset;
            while (pos < len) {
                float start = std::max(0.0f, pos);
                float end = std::min(len, pos + dashLen);
                if (end > start) {
                    drawList->AddLine(
                        ImVec2(p1.x + ux * start, p1.y + uy * start),
                        ImVec2(p1.x + ux * end, p1.y + uy * end),
                        tileBorderColor,
                        2.0f
                    );
                }
                pos += dashLen * 2.0f;
            }
        };
        
        // Top, right, bottom, left edges
        drawDashedLine(ImVec2(bsx, bsy), ImVec2(bsx + boxW, bsy));
        drawDashedLine(ImVec2(bsx + boxW, bsy), ImVec2(bsx + boxW, bsy + boxH));
        drawDashedLine(ImVec2(bsx + boxW, bsy + boxH), ImVec2(bsx, bsy + boxH));
        drawDashedLine(ImVec2(bsx, bsy + boxH), ImVec2(bsx, bsy));
        
        // Draw selected edges with highlight
        for (const auto& edgePair : currentSelection.edges) {
            const EdgeId& edge = edgePair.first;
            
            // Calculate edge midpoint and draw highlight
            float wx1, wy1, wx2, wy2;
            canvas.TileToWorld(edge.x1, edge.y1, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx1, &wy1);
            canvas.TileToWorld(edge.x2, edge.y2, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx2, &wy2);
            
            float sx1, sy1, sx2, sy2;
            canvas.WorldToScreen(wx1, wy1, &sx1, &sy1);
            canvas.WorldToScreen(wx2, wy2, &sx2, &sy2);
            
            // Center of each tile
            sx1 += tileW / 2.0f;
            sy1 += tileH / 2.0f;
            sx2 += tileW / 2.0f;
            sy2 += tileH / 2.0f;
            
            // Draw edge midpoint highlight
            float midX = (sx1 + sx2) / 2.0f;
            float midY = (sy1 + sy2) / 2.0f;
            
            drawList->AddCircleFilled(
                ImVec2(midX, midY),
                5.0f,
                edgeColor,
                8
            );
        }
        
        // Draw selected markers with highlight ring
        for (const auto& markerId : currentSelection.markerIds) {
            const Marker* marker = model.FindMarker(markerId);
            if (!marker) continue;
            
            // Convert marker position to screen
            float wx, wy;
            canvas.TileToWorld(
                static_cast<int>(marker->x), 
                static_cast<int>(marker->y),
                model.grid.tileWidth, model.grid.tileHeight, &wx, &wy
            );
            
            // Add fractional offset
            wx += (marker->x - std::floor(marker->x)) * model.grid.tileWidth;
            wy += (marker->y - std::floor(marker->y)) * model.grid.tileHeight;
            
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            // Draw selection ring around marker
            float minDim = static_cast<float>(
                std::min(model.grid.tileWidth, model.grid.tileHeight)
            );
            float markerSize = minDim * canvas.zoom * marker->size;
            
            drawList->AddCircle(
                ImVec2(sx, sy),
                markerSize / 2.0f + 4.0f,
                markerColor,
                16,
                3.0f
            );
        }
}

}  // namespace Cartograph
