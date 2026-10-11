#include "UI.h"
#include "Config.h"
#include "App.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "Jobs.h"
#include "render/Renderer.h"
#include "IOJson.h"
#include "Package.h"
#include "ProjectFolder.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include "platform/Time.h"
#include "platform/System.h"
#include "platform/NativeMenu.h"
#include "platform/NativeMenu_imgui.h"
#include "UI/ImGuiHelpers.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_surface.h>
#include <nlohmann/json.hpp>
#include <stb/stb_image.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <cstring>
#include <set>
#include <map>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// SDL_CursorDeleter implementation (outside namespace)


namespace Cartograph {

void UI::RenderRoomToolOptions(
    Model& model,
    History& history
) {
        ImGui::Spacing();
        ImGui::Text("Target Room:");
        ImGui::Separator();
        
        // Calculate height for scrollable area (max 10 rooms visible)
        const float itemHeight = 28.0f;  // Height per room row
        const float maxVisibleRooms = 10.0f;
        const float addButtonHeight = 28.0f;
        float listHeight = std::min(
            static_cast<float>(model.rooms.size()) * itemHeight + addButtonHeight,
            maxVisibleRooms * itemHeight + addButtonHeight
        );
        // Minimum height when empty
        if (model.rooms.empty()) {
            listHeight = addButtonHeight + 8.0f;
        }
        
        // Scrollable room list
        if (ImGui::BeginChild("##roomList", ImVec2(-1, listHeight), true)) {
            // Display rooms as list (similar to paint palette)
            for (const auto& room : model.rooms) {
                ImGui::PushID(room.id.c_str());
                
                bool isSelected = (m_canvasPanel.activeRoomId == room.id);
                ImVec4 roomColor = room.color.ToImVec4();
                
                // Highlight selected room row
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, 
                        ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                        ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
                }
                
                // Color swatch button
                if (ImGui::ColorButton("##roomColor", roomColor,
                                      ImGuiColorEditFlags_NoTooltip |
                                      ImGuiColorEditFlags_NoPicker,
                                      ImVec2(20, 20))) {
                    m_canvasPanel.activeRoomId = room.id;
                }
                
                ImGui::SameLine();
                
                // Room name button (full width)
                float availWidth = ImGui::GetContentRegionAvail().x;
                if (ImGui::Button(room.name.c_str(), ImVec2(availWidth, 20))) {
                    m_canvasPanel.activeRoomId = room.id;
                }
                
                if (isSelected) {
                    ImGui::PopStyleColor(2);
                }
                
                // Tooltip with room info
                if (ImGui::IsItemHovered()) {
                    auto cells = model.GetRoomCells(room.id);
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", room.name.c_str());
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "%zu cells", cells.size());
                    ImGui::EndTooltip();
                }
                
                // Right-click context menu
                if (ImGui::BeginPopupContextItem("room_context")) {
                    ImGui::TextDisabled("%s", room.name.c_str());
                    ImGui::Separator();
                    
                    if (ImGui::MenuItem("Rename...")) {
                        m_modals.showRenameRoomDialog = true;
                        m_modals.editingRoomId = room.id;
                        strncpy(m_modals.renameBuffer, room.name.c_str(),
                               sizeof(m_modals.renameBuffer) - 1);
                        m_modals.renameBuffer[
                            sizeof(m_modals.renameBuffer) - 1] = '\0';
                    }
                    
                    if (ImGui::MenuItem("Add Perimeter Walls")) {
                        auto changes = model.ComputeRoomPerimeterWallChanges(
                            room.id);
                        if (!changes.empty()) {
                            std::vector<ModifyEdgesCommand::EdgeChange> edgeChanges;
                            for (const auto& [edgeId, oldState, newState] : changes) {
                                ModifyEdgesCommand::EdgeChange ec;
                                ec.edgeId = edgeId;
                                ec.oldState = oldState;
                                ec.newState = newState;
                                edgeChanges.push_back(ec);
                            }
                            auto cmd = std::make_unique<ModifyEdgesCommand>(
                                edgeChanges);
                            history.AddCommand(std::move(cmd), model, true);
                            AddConsoleMessage("Added " + 
                                std::to_string(changes.size()) + 
                                " wall segment(s) to " + room.name,
                                MessageType::Success);
                        } else {
                            AddConsoleMessage("No walls to add - " + room.name +
                                " already has perimeter walls",
                                MessageType::Info);
                        }
                    }
                    
                    ImGui::Separator();
                    
                    if (ImGui::MenuItem("Delete Room...")) {
                        m_modals.showDeleteRoomDialog = true;
                        m_modals.editingRoomId = room.id;
                    }
                    
                    ImGui::EndPopup();
                }
                
                ImGui::PopID();
            }
            
            // "+ Add Room" button at the end
            ImGui::Spacing();
            if (ImGui::Button("+ Add Room", ImVec2(-1, 0))) {
                Room newRoom;
                newRoom.id = model.GenerateRoomId();
                newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
                newRoom.regionId = -1;
                newRoom.color = model.GenerateDistinctRoomColor();
                newRoom.cellsCacheDirty = true;
                newRoom.connectionsDirty = true;
                
                auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
                history.AddCommand(std::move(cmd), model);
                
                m_canvasPanel.activeRoomId = newRoom.id;
                AddConsoleMessage("Created " + newRoom.name, 
                    MessageType::Success);
            }
        }
        ImGui::EndChild();  // Must be called regardless of BeginChild return
}

}  // namespace Cartograph
