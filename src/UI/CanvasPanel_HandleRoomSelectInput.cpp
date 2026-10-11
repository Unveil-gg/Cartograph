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

void CanvasPanel::HandleRoomSelectInput(
    Model& model,
    Canvas& canvas
) {
            // Room Select tool: Click cell to select its room, drag to hierarchy
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert mouse position to tile coordinates
            int tx, ty;
            canvas.ScreenToTile(
                mousePos.x, mousePos.y,
                model.grid.tileWidth, model.grid.tileHeight,
                &tx, &ty
            );
            
            // Look up which room this cell belongs to
            std::string roomId = model.GetCellRoom(tx, ty);
            
            // Handle left click: select room and record as drag candidate
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (!roomId.empty()) {
                    // Select the room
                    selectedRoomId = roomId;
                    activeRoomId = roomId;
                    selectedRegionGroupId = "";  // Clear region selection
                    
                    // Record as potential drag candidate
                    canvasDragRoomId = roomId;
                    
                    // Center and zoom to fit the selected room
                    auto cells = model.GetRoomCells(roomId);
                    if (!cells.empty()) {
                        int minX = INT_MAX, minY = INT_MAX;
                        int maxX = INT_MIN, maxY = INT_MIN;
                        for (const auto& cell : cells) {
                            minX = std::min(minX, cell.first);
                            maxX = std::max(maxX, cell.first);
                            minY = std::min(minY, cell.second);
                            maxY = std::max(maxY, cell.second);
                        }
                        canvas.FocusOnRect(minX, minY, maxX, maxY,
                                          model.grid.tileWidth,
                                          model.grid.tileHeight);
                    }
                } else {
                    // Clicked on empty cell - clear drag candidate
                    canvasDragRoomId.clear();
                }
            }
            
            // Handle right click: open context menu for room
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !roomId.empty()) {
                selectedRoomId = roomId;
                activeRoomId = roomId;
                selectedRegionGroupId = "";
                ImGui::OpenPopup("RoomSelectContextMenu");
            }
}

}  // namespace Cartograph
