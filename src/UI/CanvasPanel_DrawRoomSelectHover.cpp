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

void CanvasPanel::DrawRoomSelectHover(
    Model& model,
    Canvas& canvas
) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Look up which room this cell belongs to
        std::string roomAtCursor = model.GetCellRoom(tx, ty);
        
        if (!roomAtCursor.empty()) {
            // Set hovered room for canvas highlight
            hoveredRoomId = roomAtCursor;
            
            // Find the room to get its name for the tooltip
            const Room* room = model.FindRoom(roomAtCursor);
            if (room) {
                ImGui::BeginTooltip();
                ImGui::Text("Room: %s", room->name.c_str());
                ImGui::TextDisabled("Click to select");
                ImGui::EndTooltip();
            }
        }
}

}  // namespace Cartograph
