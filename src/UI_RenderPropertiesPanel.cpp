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


void UI::RenderPropertiesPanel(Model& model, IconManager& /*icons*/,
                               JobQueue& /*jobs*/,
                               History& history, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize;
    
    ImGui::Begin("Cartograph/Hierarchy", nullptr, flags);
    
    ImGui::Text("Hierarchy");
    ImGui::Separator();
    
    // ========================================================================
    // HIERARCHY SECTION
    // ========================================================================
    
    // Toolbar buttons
    if (ImGui::Button("+ Room")) {
        // Create new room via command (undoable)
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
        AddConsoleMessage("Created " + newRoom.name, MessageType::Success);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("+ Region")) {
        // Create new region group via command (undoable)
        RegionGroup newRegion;
        newRegion.id = model.GenerateRegionGroupId();
        newRegion.name = "Region " + std::to_string(model.regionGroups.size() + 1);
        
        auto cmd = std::make_unique<CreateRegionCommand>(newRegion);
        history.AddCommand(std::move(cmd), model);
        
        AddConsoleMessage("Created " + newRegion.name, MessageType::Success);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Detect Rooms")) {
        // Create undoable command for room detection
        auto cmd = std::make_unique<DetectRoomsCommand>();
        cmd->Execute(model);
        
        int created = cmd->GetCreatedCount();
        int splitCount = cmd->GetSplitCount();
        
        // Build result message
        std::string message;
        if (splitCount > 0 && created > 0) {
            message = "Split " + std::to_string(splitCount) + 
                " disconnected region(s), created " + 
                std::to_string(created) + " new room(s)";
            AddConsoleMessage(message, MessageType::Success);
        } else if (splitCount > 0) {
            message = "Split " + std::to_string(splitCount) + 
                " disconnected region(s)";
            AddConsoleMessage(message, MessageType::Success);
        } else if (created > 0) {
            message = "Detected and created " + 
                std::to_string(created) + " room(s)";
            AddConsoleMessage(message, MessageType::Success);
        } else {
            AddConsoleMessage("No rooms to detect or split", 
                MessageType::Info);
        }
        
        // Only add to history if something changed
        if (created > 0 || splitCount > 0) {
            history.AddCommand(std::move(cmd), model, false);
        }
    }
    
    ImGui::SameLine();
    
    // Add Perimeter Walls button (greyed out if no rooms need walls)
    // Check if any room actually has edges that need walls added
    bool hasRooms = !model.rooms.empty();
    bool anyRoomNeedsWalls = false;
    if (hasRooms) {
        for (const auto& room : model.rooms) {
            auto changes = model.ComputeRoomPerimeterWallChanges(room.id);
            if (!changes.empty()) {
                anyRoomNeedsWalls = true;
                break;  // Found at least one room needing walls
            }
        }
    }
    
    bool canAddWalls = hasRooms && anyRoomNeedsWalls;
    
    if (!canAddWalls) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("Add Walls")) {
        // Collect all edge changes across all rooms for undo support
        std::vector<ModifyEdgesCommand::EdgeChange> allChanges;
        std::unordered_set<EdgeId, EdgeIdHash> seenEdges;  // Avoid duplicates
        
        for (const auto& room : model.rooms) {
            auto roomChanges = model.ComputeRoomPerimeterWallChanges(room.id);
            for (const auto& [edgeId, oldState, newState] : roomChanges) {
                if (seenEdges.insert(edgeId).second) {
                    ModifyEdgesCommand::EdgeChange change;
                    change.edgeId = edgeId;
                    change.oldState = oldState;
                    change.newState = newState;
                    allChanges.push_back(change);
                }
            }
        }
        
        if (!allChanges.empty()) {
            auto cmd = std::make_unique<ModifyEdgesCommand>(allChanges);
            history.AddCommand(std::move(cmd), model, true);
            AddConsoleMessage("Added " + std::to_string(allChanges.size()) + 
                " wall segment(s)", MessageType::Success);
        } else {
            AddConsoleMessage("No walls to add - rooms already have walls", 
                MessageType::Info);
        }
    }
    
    if (!canAddWalls) {
        ImGui::EndDisabled();
    }
    
    // Tooltip for the button
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!hasRooms) {
            ImGui::SetTooltip("No rooms detected yet.\n"
                            "Click 'Detect Rooms' first.");
        } else if (!anyRoomNeedsWalls) {
            ImGui::SetTooltip("All rooms already have walls.");
        } else {
            ImGui::SetTooltip("Add walls around all room boundaries\n"
                            "where there are no existing walls or doors.");
        }
    }
    
    if (ImGui::Button("Validate Gates", ImVec2(-1, 0))) {
        m_validation.open = !m_validation.open;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Check that every room can be reached with the\n"
                          "ability order, and none is a softlock.");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Toggle room overlays (at top for easy access)
    ImGui::Checkbox("Show Room Overlays", &m_canvasPanel.showRoomOverlays);
    ImGui::Checkbox("Show Interior", &canvas.showInterior);
    ImGui::Spacing();
    
    // Search/filter box
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search...", searchBuffer, 
                            sizeof(searchBuffer));
    std::string searchTerm(searchBuffer);
    
    // ========================================================================
    // UNITY-STYLE FLAT HIERARCHY LIST
    // ========================================================================
    
    // Track drag-and-drop state
    static std::string draggedRoomId = "";
    static std::string dragTargetRegionId = "";
    
    // Calculate dynamic height: reserve space for properties section
    // -50 for overlays toggle, -350 for properties section (dynamic based on selection)
    float hierarchyHeight = -400.0f;
    if (!m_canvasPanel.selectedRoomId.empty() || !m_canvasPanel.selectedRegionGroupId.empty()) {
        hierarchyHeight = -450.0f;  // More space when properties visible
    }
    
    // Begin child region for scrollable hierarchy
    ImGui::BeginChild("HierarchyList", ImVec2(0, hierarchyHeight), true);
    
    // Auto-scroll when dragging rooms near edges of the list
    ImGuiHelpers::HandleDragDropAutoScroll();
    
    // Context menu on empty space for creating new items
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu")) {
        if (ImGui::MenuItem("Create Room")) {
            Room newRoom;
            newRoom.id = model.GenerateRoomId();
            newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
            newRoom.regionId = -1;
            newRoom.color = model.GenerateDistinctRoomColor();
            newRoom.cellsCacheDirty = true;
            newRoom.connectionsDirty = true;
            
            auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
            history.AddCommand(std::move(cmd), model);
            
            m_canvasPanel.selectedRoomId = newRoom.id;
            AddConsoleMessage("Created " + newRoom.name, MessageType::Success);
        }
        if (ImGui::MenuItem("Create Region")) {
            RegionGroup newRegion;
            newRegion.id = model.GenerateRegionGroupId();
            newRegion.name = "Region " + std::to_string(model.regionGroups.size() + 1);
            
            auto cmd = std::make_unique<CreateRegionCommand>(newRegion);
            history.AddCommand(std::move(cmd), model);
            
            m_canvasPanel.selectedRegionGroupId = newRegion.id;
            AddConsoleMessage("Created " + newRegion.name, MessageType::Success);
        }
        ImGui::EndPopup();
    }
    
    // Render all regions and unassigned rooms in a flat list
    bool hasAnyItems = false;

    RenderRegionNodes(model, history, canvas, searchTerm, hasAnyItems);
    RenderUnassignedRooms(model, history, canvas, searchTerm, hasAnyItems);
    // Show message if no items
    if (!hasAnyItems) {
        ImGui::TextDisabled("No rooms or regions");
        ImGui::TextDisabled("Right-click to create");
    }
    
    ImGui::EndChild();  // End HierarchyList
    
    // ========================================================================
    // PROPERTIES SECTION (for selected room/region)
    // ========================================================================
    
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::BeginChild("PropertiesDetail", ImVec2(0, 0), true);
    ImGui::PopStyleColor();
    
    // Selected room details
    if (!m_canvasPanel.selectedRoomId.empty()) {
        Room* room = model.FindRoom(m_canvasPanel.selectedRoomId);
        if (room) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "ROOM PROPERTIES");
            ImGui::Separator();
            ImGui::Spacing();
            
            {
                RenderRoomFields(model, history, room);
                RenderRoomProps(model, history, room);
                RenderRoomDetails(model, history, canvas, room);
            }
        }
    }
    
    // Selected region details
    if (!m_canvasPanel.selectedRegionGroupId.empty()) {
        RenderRegionProperties(model, history, canvas);
    }
    
    // Show message if nothing selected
    if (m_canvasPanel.selectedRoomId.empty() && m_canvasPanel.selectedRegionGroupId.empty()) {
        ImGui::TextDisabled("No room or region selected");
        ImGui::TextDisabled("Select an item from the hierarchy above");
    }
    
    ImGui::EndChild();  // End PropertiesDetail
    
    // New room dialog
    if (m_modals.showNewRoomDialog) {
        ImGui::OpenPopup("Create New Room");
        m_modals.showNewRoomDialog = false;
    }
    
    if (ImGui::BeginPopupModal("Create New Room", nullptr, 
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Room Name", m_modals.newRoomName, sizeof(m_modals.newRoomName));
        ImGui::ColorEdit3("Room Color", m_modals.newRoomColor);
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Create room via command (undoable)
            Room newRoom;
            newRoom.id = model.GenerateRoomId();
            newRoom.name = m_modals.newRoomName;
            newRoom.color = Color(m_modals.newRoomColor[0], m_modals.newRoomColor[1], 
                                 m_modals.newRoomColor[2], 1.0f);
            newRoom.regionId = -1;
            newRoom.cellsCacheDirty = true;
            newRoom.connectionsDirty = true;
            
            auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
            history.AddCommand(std::move(cmd), model);
            
            m_canvasPanel.selectedRoomId = newRoom.id;
            
            // Reset form
            strncpy(m_modals.newRoomName, "New Room", 
                    sizeof(m_modals.newRoomName) - 1);
            m_modals.newRoomName[sizeof(m_modals.newRoomName) - 1] = '\0';
            m_modals.newRoomColor[0] = 1.0f;
            m_modals.newRoomColor[1] = 0.5f;
            m_modals.newRoomColor[2] = 0.5f;
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Note: Marker tool settings moved to Tools panel (shown when Marker 
    // tool is selected)
    
    ImGui::End();
}




} // namespace Cartograph
