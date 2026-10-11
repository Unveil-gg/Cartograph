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

void CanvasPanel::DrawPastePreview(
    Model& model,
    Canvas& canvas,
    ImDrawList* drawList
) {
        // Paste preview colors (using theme)
        ImU32 previewFillColor = ImGui::GetColorU32(
            model.theme.pastePreviewFill.ToImVec4()
        );
        ImU32 previewBorderColor = ImGui::GetColorU32(
            model.theme.pastePreviewBorder.ToImVec4()
        );
        ImU32 previewTileColor = ImGui::GetColorU32(
            ImVec4(model.theme.pastePreviewBorder.r,
                   model.theme.pastePreviewBorder.g,
                   model.theme.pastePreviewBorder.b, 0.4f)
        );
        
        float tileW = model.grid.tileWidth * canvas.zoom;
        float tileH = model.grid.tileHeight * canvas.zoom;
        
        // Draw ghost tiles
        for (const auto& tilePair : clipboard.tiles) {
            int tx = pastePreviewX + tilePair.first.first;
            int ty = pastePreviewY + tilePair.first.second;
            
            float wx, wy;
            canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx, &wy);
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + tileW, sy + tileH),
                previewTileColor
            );
        }
        
        // Draw bounding box outline
        float bx, by;
        canvas.TileToWorld(
            pastePreviewX, pastePreviewY,
            model.grid.tileWidth, model.grid.tileHeight, &bx, &by
        );
        float bsx, bsy;
        canvas.WorldToScreen(bx, by, &bsx, &bsy);
        
        float boxW = clipboard.width * tileW;
        float boxH = clipboard.height * tileH;
        
        // Dashed outline
        float dashLen = 8.0f;
        float time = static_cast<float>(ImGui::GetTime());
        float offset = std::fmod(time * 30.0f, dashLen * 2.0f);
        
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
                        previewBorderColor,
                        2.0f
                    );
                }
                pos += dashLen * 2.0f;
            }
        };
        
        drawDashedLine(ImVec2(bsx, bsy), ImVec2(bsx + boxW, bsy));
        drawDashedLine(ImVec2(bsx + boxW, bsy), ImVec2(bsx + boxW, bsy + boxH));
        drawDashedLine(ImVec2(bsx + boxW, bsy + boxH), ImVec2(bsx, bsy + boxH));
        drawDashedLine(ImVec2(bsx, bsy + boxH), ImVec2(bsx, bsy));
        
        // Draw "Click to paste" hint
        const char* pasteText = "Click to paste";
        ImVec2 textSize = ImGui::CalcTextSize(pasteText);
        ImVec2 textPos(bsx + boxW / 2.0f - textSize.x / 2.0f, 
                      bsy - textSize.y - 8.0f);
        
        drawList->AddRectFilled(
            ImVec2(textPos.x - 4, textPos.y - 2),
            ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
            ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.8f))
        );
        drawList->AddText(textPos, 
            ImGui::GetColorU32(model.theme.pastePreviewBorder.ToImVec4()), 
            pasteText);
}

}  // namespace Cartograph
