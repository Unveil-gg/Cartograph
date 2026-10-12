#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../ProjectFolder.h"
#include "../Preferences.h"
#include "../platform/Paths.h"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace Cartograph {


void Modals::RenderNewProjectModal(App& app, Model& /*model*/) {
    // Only call OpenPopup once when modal is first shown
    if (!newProjectModalOpened) {
        ImGui::OpenPopup("New Project");
        newProjectModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New Project", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Project name
        ImGui::Text("Project Name:");
        if (ImGui::InputText("##projectname", newProjectConfig.projectName, 
            sizeof(newProjectConfig.projectName))) {
            // Update path when project name changes
            UpdateNewProjectPath();
        }
        
        ImGui::Spacing();
        
        // Save location
        ImGui::Text("Save Location:");
        
        // Display path in a framed box (styled like input field)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, 
                             ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("##savelocationdisplay", 
                         ImVec2(450, 30), 
                         true, 
                         ImGuiWindowFlags_NoScrollbar);
        
        ImGui::PushStyleColor(ImGuiCol_Text, 
                             ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::TextWrapped("%s", newProjectConfig.fullSavePath.c_str());
        ImGui::PopStyleColor();
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Choose Different Location...", 
                         ImVec2(240, 0))) {
            ShowNewProjectFolderPicker();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map Style Selection
        ImGui::Text("Choose your map style:");
        ImGui::Spacing();
        
        // Grid preset cards - side by side
        ImGui::BeginGroup();
        
        // Square preset card
        bool isSquareSelected = 
            (newProjectConfig.gridPreset == GridPreset::Square);
        if (isSquareSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                                 ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                                 ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Square\n16×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Square;
        }
        ImGui::PopStyleVar();
        
        if (isSquareSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 20);
        
        // Rectangle preset card
        bool isRectangleSelected = 
            (newProjectConfig.gridPreset == GridPreset::Rectangle);
        if (isRectangleSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                                 ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                                 ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Rectangle\n32×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Rectangle;
        }
        ImGui::PopStyleVar();
        
        if (isRectangleSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::EndGroup();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map dimensions with validation
        ImGui::PushItemWidth(150.0f);
        ImGui::InputInt("Map Width (cells)", &newProjectConfig.mapWidth);
        if (newProjectConfig.mapWidth < 16) newProjectConfig.mapWidth = 16;
        if (newProjectConfig.mapWidth > 1024) 
            newProjectConfig.mapWidth = 1024;
        
        ImGui::InputInt("Map Height (cells)", &newProjectConfig.mapHeight);
        if (newProjectConfig.mapHeight < 16) 
            newProjectConfig.mapHeight = 16;
        if (newProjectConfig.mapHeight > 1024) 
            newProjectConfig.mapHeight = 1024;
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        
        // Preview info
        int totalCells = newProjectConfig.mapWidth * 
            newProjectConfig.mapHeight;
        
        // Get cell dimensions from preset
        int cellWidth = 
            (newProjectConfig.gridPreset == GridPreset::Square) ? 16 : 32;
        int cellHeight = 16;
        
        int pixelWidth = newProjectConfig.mapWidth * cellWidth;
        int pixelHeight = newProjectConfig.mapHeight * cellHeight;
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Total cells: %d | Canvas size: %dx%d px",
            totalCells, pixelWidth, pixelHeight);
        
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Validate path
            if (newProjectConfig.fullSavePath.empty()) {
                m_ui.ShowToast("Please select a save location", 
                         Toast::Type::Error);
            } else {
                showNewProjectModal = false;
                newProjectModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                // Create project with configuration
                // NewProject() handles InitDefaults() + applies config
                app.NewProject(
                    newProjectConfig.fullSavePath,
                    std::string(newProjectConfig.projectName),
                    newProjectConfig.gridPreset,
                    newProjectConfig.mapWidth,
                    newProjectConfig.mapHeight
                );
                
                // Transition to editor
                app.ShowEditor();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProjectModal = false;
            newProjectModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}


} // namespace Cartograph
