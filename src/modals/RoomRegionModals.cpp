#include "Modals.h"
#include "../UI.h"
#include "../ui/CanvasPanel.h"
#include <imgui.h>
#include <algorithm>

namespace Cartograph {

void Modals::RenderDeleteRoomModal(Model& model, History& history) {
    // Only call OpenPopup once when modal is first shown
    if (!deleteRoomDialogOpened) {
        ImGui::OpenPopup("Delete Room?");
        deleteRoomDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Delete Room?", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        Room* room = model.FindRoom(editingRoomId);
        if (room) {
            ImGui::Text("Delete room \"%s\"?", room->name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped(
                "This will remove the room and clear all cell assignments."
            );
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showDeleteRoomDialog = false;
                deleteRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                // Save room name for console message
                std::string roomName = room->name;
                
                // Delete room via command (undoable)
                auto cmd = std::make_unique<DeleteRoomCommand>(editingRoomId);
                history.AddCommand(std::move(cmd), model);
                
                // Clear selection if we deleted the selected room
                if (m_ui.GetCanvasPanel().selectedRoomId == editingRoomId) {
                    m_ui.GetCanvasPanel().selectedRoomId.clear();
                }
                if (m_ui.GetCanvasPanel().activeRoomId == editingRoomId) {
                    m_ui.GetCanvasPanel().activeRoomId.clear();
                }
                
                m_ui.AddConsoleMessage("Deleted room \"" + roomName + "\"",
                                     MessageType::Success);
                
                showDeleteRoomDialog = false;
                deleteRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(3);
        } else {
            ImGui::Text("Room not found");
            if (ImGui::Button("Close")) {
                showDeleteRoomDialog = false;
                deleteRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderRemoveFromRegionModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!removeFromRegionDialogOpened) {
        ImGui::OpenPopup("Remove from Region?");
        removeFromRegionDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Remove from Region?", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        Room* room = model.FindRoom(editingRoomId);
        if (room) {
            // Find the region name
            std::string regionName = "Unknown Region";
            RegionGroup* region = 
                model.FindRegionGroup(room->parentRegionGroupId);
            if (region) {
                regionName = region->name;
            }
            
            ImGui::Text("Remove \"%s\" from \"%s\"?", 
                       room->name.c_str(), regionName.c_str());
            ImGui::Separator();
            ImGui::TextWrapped(
                "The room will become unassigned and appear in the "
                "unparented rooms list."
            );
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showRemoveFromRegionDialog = false;
                removeFromRegionDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Remove", ImVec2(120, 0))) {
                std::string roomName = room->name;
                room->parentRegionGroupId = "";
                model.MarkDirty();
                
                m_ui.AddConsoleMessage("Removed \"" + roomName + 
                    "\" from region", MessageType::Success);
                
                showRemoveFromRegionDialog = false;
                removeFromRegionDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Room not found");
            if (ImGui::Button("Close")) {
                showRemoveFromRegionDialog = false;
                removeFromRegionDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderRenameRoomModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!renameRoomDialogOpened) {
        ImGui::OpenPopup("Rename Room");
        renameRoomDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Room", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        Room* room = model.FindRoom(editingRoomId);
        if (room) {
            ImGui::Text("Rename room:");
            ImGui::Spacing();
            
            ImGui::SetNextItemWidth(300);
            bool enterPressed = ImGui::InputText("##rename", renameBuffer, 
                                                sizeof(renameBuffer), 
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showRenameRoomDialog = false;
                renameRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Rename", ImVec2(120, 0)) || enterPressed) {
                std::string newName = renameBuffer;
                if (!newName.empty()) {
                    room->name = newName;
                    model.MarkDirty();
                    m_ui.AddConsoleMessage("Renamed room to \"" + newName + "\"",
                                         MessageType::Success);
                }
                
                showRenameRoomDialog = false;
                renameRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Room not found");
            if (ImGui::Button("Close")) {
                showRenameRoomDialog = false;
                renameRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderRenameRegionModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!renameRegionDialogOpened) {
        ImGui::OpenPopup("Rename Region");
        renameRegionDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Region", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        RegionGroup* region = model.FindRegionGroup(editingRegionId);
        if (region) {
            ImGui::Text("Rename region:");
            ImGui::Spacing();
            
            ImGui::SetNextItemWidth(300);
            bool enterPressed = ImGui::InputText("##rename", renameBuffer, 
                                                sizeof(renameBuffer), 
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showRenameRegionDialog = false;
                renameRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Rename", ImVec2(120, 0)) || enterPressed) {
                std::string newName = renameBuffer;
                if (!newName.empty()) {
                    region->name = newName;
                    model.MarkDirty();
                    m_ui.AddConsoleMessage("Renamed region to \"" + newName + 
                                         "\"", MessageType::Success);
                }
                
                showRenameRegionDialog = false;
                renameRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Region not found");
            if (ImGui::Button("Close")) {
                showRenameRegionDialog = false;
                renameRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderDeleteRegionModal(Model& model, History& history) {
    // Only call OpenPopup once when modal is first shown
    if (!deleteRegionDialogOpened) {
        ImGui::OpenPopup("Delete Region?");
        deleteRegionDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Delete Region?", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        RegionGroup* region = model.FindRegionGroup(editingRegionId);
        if (region) {
            ImGui::Text("Delete region \"%s\"?", region->name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped(
                "Rooms in this region will become unassigned."
            );
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showDeleteRegionDialog = false;
                deleteRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                // Save region name for console message
                std::string regionName = region->name;
                
                // Delete region via command (undoable)
                auto cmd = 
                    std::make_unique<DeleteRegionCommand>(editingRegionId);
                history.AddCommand(std::move(cmd), model);
                
                // Clear selection if we deleted the selected region
                if (m_ui.GetCanvasPanel().selectedRegionGroupId == 
                    editingRegionId) {
                    m_ui.GetCanvasPanel().selectedRegionGroupId.clear();
                }
                
                m_ui.AddConsoleMessage("Deleted region \"" + regionName + "\"",
                                     MessageType::Success);
                
                showDeleteRegionDialog = false;
                deleteRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(3);
        } else {
            ImGui::Text("Region not found");
            if (ImGui::Button("Close")) {
                showDeleteRegionDialog = false;
                deleteRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph
