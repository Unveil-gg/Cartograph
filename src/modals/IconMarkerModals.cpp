#include "Modals.h"
#include "../UI.h"
#include "../ui/CanvasPanel.h"
#include "../Icons.h"
#include <imgui.h>
#include <algorithm>

namespace Cartograph {

void Modals::RenderRenameIconModal(Model& model, IconManager& icons, 
                                   std::string& selectedIconName) {
    // Only call OpenPopup once when modal is first shown
    if (!renameIconModalOpened) {
        ImGui::OpenPopup("Rename Icon");
        renameIconModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Rename Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Current name: %s", renameIconOldName.c_str());
        ImGui::Spacing();
        
        // New name input
        ImGui::InputText("New name", renameIconNewName, 
                        sizeof(renameIconNewName));
        
        ImGui::Spacing();
        ImGui::Separator();
        
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            std::string newName(renameIconNewName);
            std::string errorMsg;
            
            // Validate and rename
            if (icons.RenameIcon(renameIconOldName, newName, errorMsg)) {
                // Update all markers using this icon
                int count = model.UpdateMarkerIconNames(
                    renameIconOldName, newName);
                
                // Update selected icon if it was the renamed one
                if (selectedIconName == renameIconOldName) {
                    selectedIconName = newName;
                }
                
                showRenameIconModal = false;
                renameIconModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                std::string msg = "Icon renamed";
                if (count > 0) {
                    msg += " (" + std::to_string(count) + " marker";
                    if (count > 1) msg += "s";
                    msg += " updated)";
                }
                m_ui.ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                m_ui.ShowToast("Rename failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRenameIconModal = false;
            renameIconModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderDeleteIconModal(Model& model, IconManager& icons, 
                                   History& history, 
                                   std::string& selectedIconName,
                                   std::string& selectedMarkerId) {
    // Only call OpenPopup once when modal is first shown
    if (!deleteIconModalOpened) {
        ImGui::OpenPopup("Delete Icon");
        deleteIconModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Delete Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Delete Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Icon: %s", deleteIconName.c_str());
        ImGui::Spacing();
        
        // Show warning if markers are using this icon
        if (deleteIconMarkerCount > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.7f, 0.2f, 1.0f));  // Orange warning
            ImGui::TextWrapped(
                "Warning: This icon is used by %d marker%s.", 
                deleteIconMarkerCount,
                deleteIconMarkerCount == 1 ? "" : "s"
            );
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            
            // Show affected markers (limit to first 10)
            ImGui::TextWrapped("Affected markers:");
            ImGui::Indent();
            
            int displayCount = std::min(
                static_cast<int>(deleteIconAffectedMarkers.size()), 10
            );
            for (int i = 0; i < displayCount; ++i) {
                const std::string& markerId = deleteIconAffectedMarkers[i];
                const Marker* marker = model.FindMarker(markerId);
                if (marker) {
                    ImGui::BulletText("%s at (%.1f, %.1f)", 
                        marker->label.empty() ? 
                            marker->id.c_str() : marker->label.c_str(),
                        marker->x, marker->y
                    );
                }
            }
            
            if (deleteIconAffectedMarkers.size() > 10) {
                ImGui::BulletText("... and %zu more", 
                    deleteIconAffectedMarkers.size() - 10);
            }
            
            ImGui::Unindent();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.5f, 0.5f, 1.0f));  // Red warning
            ImGui::TextWrapped(
                "Deleting this icon will also remove all markers using it."
            );
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
        } else {
            // No markers using this icon - safe to delete
            ImGui::TextWrapped(
                "Are you sure you want to delete this icon?"
            );
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), 
                "No markers are using this icon.");
            ImGui::Spacing();
        }
        
        // Remind user that deletion is undoable
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), 
            "Tip: You can undo this with Cmd+Z");
        ImGui::Spacing();
        
        ImGui::Separator();
        
        // Delete button (destructive action, so place on left)
        bool confirmDelete = false;
        if (deleteIconMarkerCount > 0) {
            // Destructive action - use warning color
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.3f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(1.0f, 0.4f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.9f, 0.2f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete Icon & Markers", ImVec2(200, 0))) {
                confirmDelete = true;
            }
            
            ImGui::PopStyleColor(3);
        } else {
            // Safe deletion
            if (ImGui::Button("Delete Icon", ImVec2(120, 0))) {
                confirmDelete = true;
            }
        }
        
        if (confirmDelete) {
            // Create delete command with icon manager reference
            auto cmd = std::make_unique<DeleteIconCommand>(
                deleteIconName, 
                deleteIconMarkerCount > 0,  // Remove markers if any exist
                icons                       // Icon manager for undo restoration
            );
            
            // Capture icon state before deletion
            cmd->CaptureIconState();
            
            // Delete icon from IconManager
            std::string errorMsg;
            if (icons.DeleteIcon(deleteIconName, errorMsg)) {
                // Rebuild atlas
                icons.BuildAtlas();
                
                // Execute command (handles marker deletion)
                history.AddCommand(std::move(cmd), model, true);
                
                // Update selected icon if it was the deleted one
                if (selectedIconName == deleteIconName) {
                    // Try to select another icon if available
                    auto remainingIcons = 
                        icons.GetIconNamesByCategory("marker");
                    if (!remainingIcons.empty()) {
                        selectedIconName = remainingIcons[0];
                    } else {
                        selectedIconName = "";
                    }
                }
                
                // Deselect marker if it was using this icon
                Marker* selMarker = model.FindMarker(selectedMarkerId);
                if (selMarker && selMarker->icon == deleteIconName) {
                    selectedMarkerId.clear();
                }
                
                showDeleteIconModal = false;
                deleteIconModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                // Show success message
                std::string msg = "Icon deleted";
                if (deleteIconMarkerCount > 0) {
                    msg += " (" + std::to_string(deleteIconMarkerCount) + 
                           " marker";
                    if (deleteIconMarkerCount > 1) msg += "s";
                    msg += " removed)";
                }
                m_ui.ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                m_ui.ShowToast("Delete failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        // Cancel button (safe action, default)
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || 
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showDeleteIconModal = false;
            deleteIconModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderMarkerLabelRenameModal(Model& model, History& history) {
    static bool modalOpened = false;
    
    if (!modalOpened) {
        ImGui::OpenPopup("Rename Markers");
        modalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Rename Markers", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "You changed the label for a marker style with %d markers.",
            markerLabelRenameCount);
        ImGui::Spacing();
        ImGui::TextWrapped("New label: \"%s\"", 
                          markerLabelRenameNewLabel.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("What would you like to do?");
        ImGui::Spacing();
        
        // Rename All button
        if (ImGui::Button("Rename All", ImVec2(180, 0))) {
            // Parse style key to get icon and color
            size_t colonPos = markerLabelRenameStyleKey.find(':');
            if (colonPos != std::string::npos) {
                std::string styleIcon = 
                    markerLabelRenameStyleKey.substr(0, colonPos);
                std::string styleColorHex = 
                    markerLabelRenameStyleKey.substr(colonPos + 1);
                
                // Update all markers with this style
                for (auto& marker : model.markers) {
                    if (marker.icon == styleIcon && 
                        marker.color.ToHex(false) == styleColorHex) {
                        marker.label = markerLabelRenameNewLabel;
                        marker.showLabel = !markerLabelRenameNewLabel.empty();
                    }
                }
                model.MarkDirty();
                
                // Note: For simplicity, we don't create individual undo 
                // commands for batch rename. Could be added if needed.
            }
            
            // Clear state and close
            showMarkerLabelRenameModal = false;
            modalOpened = false;
            m_ui.m_selectedPaletteStyleKey.clear();
            m_ui.m_paletteStyleMarkerCount = 0;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Update the label for all %d markers\n"
                             "with this icon and color", 
                             markerLabelRenameCount);
        }
        
        ImGui::SameLine();
        
        // Create New button
        if (ImGui::Button("Just This One", ImVec2(180, 0))) {
            // Only update template for new markers - don't modify existing
            // The label is already set in m_canvasPanel.markerLabel
            // If there was a selected marker, update just that one
            Marker* selMarker = 
                model.FindMarker(m_ui.m_canvasPanel.selectedMarkerId);
            if (selMarker) {
                selMarker->label = markerLabelRenameNewLabel;
                selMarker->showLabel = !markerLabelRenameNewLabel.empty();
                model.MarkDirty();
            }
            
            // Clear state and close
            showMarkerLabelRenameModal = false;
            modalOpened = false;
            m_ui.m_selectedPaletteStyleKey.clear();
            m_ui.m_paletteStyleMarkerCount = 0;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Only update the selected marker (if any)\n"
                             "or set as template for new markers");
        }
        
        ImGui::Spacing();
        
        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            // Revert the label change
            size_t colonPos = markerLabelRenameStyleKey.find(':');
            if (colonPos != std::string::npos) {
                // Find original label from first marker with this style
                std::string styleIcon = 
                    markerLabelRenameStyleKey.substr(0, colonPos);
                std::string styleColorHex = 
                    markerLabelRenameStyleKey.substr(colonPos + 1);
                
                for (const auto& marker : model.markers) {
                    if (marker.icon == styleIcon && 
                        marker.color.ToHex(false) == styleColorHex) {
                        m_ui.m_canvasPanel.markerLabel = marker.label;
                        break;
                    }
                }
            }
            
            showMarkerLabelRenameModal = false;
            modalOpened = false;
            m_ui.m_selectedPaletteStyleKey.clear();
            m_ui.m_paletteStyleMarkerCount = 0;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph
