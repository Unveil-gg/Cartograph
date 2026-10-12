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

void CanvasPanel::HandleRoomSelectDrag(
    Model& model,
    Canvas& /*canvas*/,
    History& history
) {
        // Handle drag: initiate drag-drop to hierarchy
        if (!canvasDragRoomId.empty() && 
            ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            isCanvasDraggingRoom = true;
            
            // Use SourceExtern for drag sources outside normal ImGui items
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern)) {
                ImGui::SetDragDropPayload(
                    "ROOM_DRAG", 
                    canvasDragRoomId.c_str(), 
                    canvasDragRoomId.size()
                );
                
                // Show drag tooltip with room name
                const Room* room = model.FindRoom(canvasDragRoomId);
                if (room) {
                    ImGui::Text("Move %s", room->name.c_str());
                } else {
                    ImGui::Text("Move room");
                }
                ImGui::EndDragDropSource();
            }
        }
        
        // Handle release: clear drag state
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            canvasDragRoomId.clear();
            isCanvasDraggingRoom = false;
        }
        
        // Render context menu (outside hover check so it stays open)
        if (ImGui::BeginPopup("RoomSelectContextMenu")) {
            Room* room = model.FindRoom(selectedRoomId);
            if (room) {
                // Room name header
                ImGui::TextDisabled("%s", room->name.c_str());
                ImGui::Separator();
                
                // Rename
                if (ImGui::MenuItem("Rename...")) {
                    modals->showRenameRoomDialog = true;
                    modals->editingRoomId = room->id;
                    strncpy(modals->renameBuffer, room->name.c_str(),
                           sizeof(modals->renameBuffer) - 1);
                    modals->renameBuffer[sizeof(modals->renameBuffer) - 1] = '\0';
                }
                
                // Move to Region submenu (or disabled item if no regions)
                bool hasRegions = !model.regionGroups.empty();
                if (hasRegions) {
                    if (ImGui::BeginMenu("Move to Region")) {
                        for (auto& region : model.regionGroups) {
                            // Skip current region if already in one
                            bool isCurrent = (room->parentRegionGroupId == region.id);
                            if (isCurrent) {
                                ImGui::BeginDisabled();
                            }
                            if (ImGui::MenuItem(region.name.c_str())) {
                                room->parentRegionGroupId = region.id;
                                model.MarkDirty();
                            }
                            if (isCurrent) {
                                ImGui::EndDisabled();
                            }
                        }
                        ImGui::EndMenu();
                    }
                } else {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem("Move to Region");
                    ImGui::EndDisabled();
                }
                
                // Remove from Region (only if in a region)
                if (!room->parentRegionGroupId.empty()) {
                    if (ImGui::MenuItem("Remove from Region...")) {
                        modals->showRemoveFromRegionDialog = true;
                        modals->editingRoomId = room->id;
                    }
                }
                
                // Add Perimeter Walls
                if (ImGui::MenuItem("Add Perimeter Walls")) {
                    auto changes = model.ComputeRoomPerimeterWallChanges(room->id);
                    if (!changes.empty()) {
                        std::vector<ModifyEdgesCommand::EdgeChange> ec;
                        for (const auto& [eId, oldS, newS] : changes) {
                            ModifyEdgesCommand::EdgeChange c;
                            c.edgeId = eId;
                            c.oldState = oldS;
                            c.newState = newS;
                            ec.push_back(c);
                        }
                        auto cmd = std::make_unique<ModifyEdgesCommand>(ec);
                        history.AddCommand(std::move(cmd), model, true);
                    }
                }
                
                ImGui::Separator();
                
                // Delete Room
                if (ImGui::MenuItem("Delete Room...")) {
                    modals->showDeleteRoomDialog = true;
                    modals->editingRoomId = room->id;
                }
            }
            ImGui::EndPopup();
        }
}

}  // namespace Cartograph
