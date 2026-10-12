#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../ProjectFolder.h"
#include "../Preferences.h"
#include "../platform/Paths.h"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace Cartograph {


void Modals::RenderProjectActionModal(
    std::vector<RecentProject>& recentProjects) {
    // Only call OpenPopup once when modal is first shown
    if (!projectActionModalOpened) {
        ImGui::OpenPopup("Project Options");
        projectActionModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Project Options", &showProjectActionModal,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        
        // Project name (prominent)
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                          "Project: %s", projectActionName.c_str());
        
        // Project path (dimmed, wrapped for long paths)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("%s", projectActionPath.c_str());
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Prompt
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                          "Choose an action:");
        ImGui::Spacing();
        
        // Delete Project button (destructive)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        
        if (ImGui::Button("Delete Project", ImVec2(-1, 0))) {
            // Show confirmation popup
            ImGui::OpenPopup("Confirm Delete");
        }
        
        ImGui::PopStyleColor(3);
        
        // Confirmation popup for delete (centered)
        ImVec2 confirmCenter = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(confirmCenter, ImGuiCond_Always, 
                               ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Are you sure you want to delete this project?");
            ImGui::Spacing();
            ImGui::TextWrapped("This will permanently delete the project "
                             "files from disk. This cannot be undone.");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "%s", projectActionPath.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Button, 
                                 ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                 ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                 ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete Permanently", ImVec2(140, 0))) {
                // Delete from filesystem
                namespace fs = std::filesystem;
                std::error_code ec;
                
                if (fs::exists(projectActionPath, ec)) {
                    if (fs::is_directory(projectActionPath, ec)) {
                        fs::remove_all(projectActionPath, ec);
                    } else {
                        fs::remove(projectActionPath, ec);
                    }
                }
                
                // Remove from recent projects
                RecentProjects::Remove(projectActionPath);
                
                // Remove from in-memory list
                auto it = std::remove_if(recentProjects.begin(), 
                    recentProjects.end(),
                    [this](const RecentProject& p) {
                        return p.path == projectActionPath;
                    });
                recentProjects.erase(it, recentProjects.end());
                
                m_ui.ShowToast("Project deleted", Toast::Type::Info);
                
                showProjectActionModal = false;
                projectActionModalOpened = false;
                ImGui::CloseCurrentPopup();
                ImGui::CloseCurrentPopup();  // Close both popups
            }
            
            ImGui::PopStyleColor(3);
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel##delete", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::Spacing();
        
        // Remove from list button (non-destructive)
        if (ImGui::Button("Remove from List", ImVec2(-1, 0))) {
            // Remove from recent projects only (don't delete files)
            RecentProjects::Remove(projectActionPath);
            
            // Remove from in-memory list
            auto it = std::remove_if(recentProjects.begin(), 
                recentProjects.end(),
                [this](const RecentProject& p) {
                    return p.path == projectActionPath;
                });
            recentProjects.erase(it, recentProjects.end());
            
            m_ui.ShowToast("Project removed from list", Toast::Type::Info);
            
            showProjectActionModal = false;
            projectActionModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Remove from recent projects list\n"
                            "(project files are not deleted)");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            showProjectActionModal = false;
            projectActionModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Reset flag if modal was closed
    if (!showProjectActionModal) {
        projectActionModalOpened = false;
    }
}


void Modals::RenderSaveBeforeActionModal(App& app, Model& model) {
    const char* actionName = "";
    switch (pendingAction) {
        case PendingAction::NewProject:
            actionName = "creating a new project";
            break;
        case PendingAction::OpenProject:
            actionName = "opening a project";
            break;
        default:
            actionName = "continuing";
            break;
    }
    
    // Only call OpenPopup once when modal is first shown
    if (!saveBeforeActionModalOpened) {
        ImGui::OpenPopup("Unsaved Changes");
        saveBeforeActionModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Warning: Unsaved Changes");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "You have unsaved changes. Do you want to save your work "
            "before %s?", actionName);
        ImGui::Spacing();
        
        // Show current project name if available
        if (!app.GetCurrentFilePath().empty()) {
            // Extract filename from path
            std::string filename = app.GetCurrentFilePath();
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            ImGui::TextDisabled("Current project: %s", filename.c_str());
        } else {
            ImGui::TextDisabled("Current project: Untitled");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            pendingAction = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            // Execute the pending action without saving
            if (pendingAction == PendingAction::NewProject) {
                app.ShowNewProjectDialog();
            } else if (pendingAction == PendingAction::OpenProject) {
                app.ShowOpenProjectDialog();
            }
            
            pendingAction = PendingAction::None;
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.15f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            // Save then execute the pending action
            app.SaveProject();
            
            // Only proceed if save succeeded (model should be clean now)
            if (!model.dirty) {
                if (pendingAction == PendingAction::NewProject) {
                    app.ShowNewProjectDialog();
                } else if (pendingAction == PendingAction::OpenProject) {
                    app.ShowOpenProjectDialog();
                }
            }
            
            pendingAction = PendingAction::None;
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}


void Modals::ApplyTemplate(ProjectTemplate tmpl) {
    switch (tmpl) {
        case ProjectTemplate::Small:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 128;
            newProjectConfig.mapHeight = 128;
            break;
            
        case ProjectTemplate::Medium:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Large:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 512;
            newProjectConfig.mapHeight = 512;
            break;
            
        case ProjectTemplate::Metroidvania:
            newProjectConfig.gridPreset = GridPreset::Rectangle;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Custom:
        default:
            // Keep current settings
            break;
    }
}


void Modals::BeginNewProject() {
    showNewProjectModal = true;
    // Reset config to defaults
    std::strncpy(newProjectConfig.projectName, "New Map",
                 sizeof(newProjectConfig.projectName) - 1);
    newProjectConfig.projectName[
        sizeof(newProjectConfig.projectName) - 1] = '\0';
    newProjectConfig.gridPreset = GridPreset::Square;
    newProjectConfig.mapWidth = 100;
    newProjectConfig.mapHeight = 100;
    
    // Initialize default save path
    newProjectConfig.saveDirectory = Platform::GetDefaultProjectsDir();
    UpdateNewProjectPath();
    
    // Ensure default directory exists
    Platform::EnsureDirectoryExists(newProjectConfig.saveDirectory);
}


void Modals::UpdateNewProjectPath() {
    // Use centralized sanitization helper
    std::string sanitized = ProjectFolder::SanitizeProjectName(
        newProjectConfig.projectName);
    
    // Build full path with .cartproj extension
    if (!newProjectConfig.saveDirectory.empty()) {
        // Ensure directory path ends with separator
        std::string dir = newProjectConfig.saveDirectory;
        if (dir.back() != '/' && dir.back() != '\\') {
#ifdef _WIN32
            dir += '\\';
#else
            dir += '/';
#endif
        }
        
        // Append .cartproj extension for new projects
        newProjectConfig.fullSavePath = dir + sanitized + CARTPROJ_EXTENSION;
    }
}


} // namespace Cartograph
