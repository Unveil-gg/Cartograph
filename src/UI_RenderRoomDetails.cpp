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

void UI::RenderRoomDetails(
    Model& model,
    History& history,
    Canvas& canvas,
    Room* room
) {
                // Cell count (read-only)
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                const auto& cells = model.GetRoomCells(room->id);
                ImGui::Text("Cell Count: %zu", cells.size());
                
                // Calculate bounding box
                if (!cells.empty()) {
                    int minX = INT_MAX, minY = INT_MAX;
                    int maxX = INT_MIN, maxY = INT_MIN;
                    for (const auto& cell : cells) {
                        minX = std::min(minX, cell.first);
                        minY = std::min(minY, cell.second);
                        maxX = std::max(maxX, cell.first);
                        maxY = std::max(maxY, cell.second);
                    }
                    int width = maxX - minX + 1;
                    int height = maxY - minY + 1;
                    ImGui::Text("Dimensions: %d x %d", width, height);
                }
                
                // Connected rooms (via doors)
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Text("Connected Rooms:");
                
                // Update connections if dirty
                if (room->connectionsDirty) {
                    model.UpdateRoomConnections(room->id);
                }
                
                if (room->connectedRoomIds.empty()) {
                    ImGui::Indent();
                    ImGui::TextDisabled("No connections");
                    ImGui::Unindent();
                } else {
                    ImGui::Indent();
                    for (const auto& connectedId : room->connectedRoomIds) {
                        Room* connectedRoom = model.FindRoom(connectedId);
                        if (connectedRoom) {
                            ImGui::PushID(connectedId.c_str());
                            
                            // Make entire row clickable with hover effect
                            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,
                                              ImVec2(0.0f, 0.5f));
                            
                            // Show color indicator
                            ImVec4 connColor = connectedRoom->color.ToImVec4();
                            ImGui::ColorButton("##connColor", connColor,
                                             ImGuiColorEditFlags_NoTooltip |
                                             ImGuiColorEditFlags_NoPicker,
                                             ImVec2(12, 12));
                            ImGui::SameLine();
                            
                            // Clickable room name
                            if (ImGui::Selectable(connectedRoom->name.c_str(),
                                                 false,
                                                 ImGuiSelectableFlags_None)) {
                                m_canvasPanel.selectedRoomId = connectedId;
                                m_canvasPanel.activeRoomId = connectedId;
                                
                                // Navigate camera to the connected room
                                auto cells = model.GetRoomCells(connectedId);
                                if (!cells.empty()) {
                                    int minX = INT_MAX, minY = INT_MAX;
                                    int maxX = INT_MIN, maxY = INT_MIN;
                                    for (const auto& cell : cells) {
                                        minX = std::min(minX, cell.first);
                                        maxX = std::max(maxX, cell.first);
                                        minY = std::min(minY, cell.second);
                                        maxY = std::max(maxY, cell.second);
                                    }
                                    int centerX = (minX + maxX) / 2;
                                    int centerY = (minY + maxY) / 2;
                                    // Zoom to 110% and focus on room center
                                    canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                                    canvas.FocusOnTile(centerX, centerY,
                                        model.grid.tileWidth,
                                        model.grid.tileHeight);
                                }
                            }
                            
                            // Auto-scroll to hovered room (from canvas RoomSelect)
                            if (m_canvasPanel.hoveredRoomId == connectedId) {
                                ImGui::SetScrollHereY(0.5f);
                            }
                            
                            // Show hand cursor and highlight room on hover
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                                m_canvasPanel.hoveredRoomId = connectedId;
                            }
                            
                            ImGui::PopStyleVar();
                            ImGui::PopID();
                        }
                    }
                    ImGui::Unindent();
                }
                
                // Room actions
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                if (ImGui::Button("Edit Interior", ImVec2(-1, 0))) {
                    m_interiorEditor.Open(room->id);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Paint solid, platform, hazard, water,\n"
                        "breakable and ladder tiles inside this room.");
                }
                ImGui::Spacing();

                // Add Perimeter Walls button
                if (ImGui::Button("Add Perimeter Walls", ImVec2(-1, 0))) {
                    auto changes = model.ComputeRoomPerimeterWallChanges(
                        room->id);
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
                        AddConsoleMessage("Added " + 
                            std::to_string(changes.size()) + 
                            " wall segment(s) to " + room->name,
                            MessageType::Success);
                    } else {
                        AddConsoleMessage("No walls to add - " + room->name +
                            " already has perimeter walls", MessageType::Info);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Add walls around room boundaries\n"
                        "where there are no existing walls");
                }
                
                ImGui::Spacing();
                
                // Delete room button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                
                if (ImGui::Button("Delete Room", ImVec2(-1, 0))) {
                    // Confirm deletion
                    m_modals.editingRoomId = room->id;
                    m_modals.showDeleteRoomDialog = true;
                }
                
                ImGui::PopStyleColor(3);
}

}  // namespace Cartograph
