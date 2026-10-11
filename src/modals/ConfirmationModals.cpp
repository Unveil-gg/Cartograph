#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../UI/CanvasPanel.h"
#include "../Jobs.h"
#include "../Icons.h"
#include "../IOJson.h"
#include "../platform/Paths.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Cartograph {

void Modals::RenderFillConfirmationModal(Model& model, History& history) {
    // Only call OpenPopup once when modal is first shown
    if (!fillConfirmationModalOpened) {
        ImGui::OpenPopup("Large Fill Warning");
        fillConfirmationModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Large Fill Warning", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Warning: Large Fill Operation");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "This fill operation will affect %zu cells. "
            "Large fills may indicate an accidental click outside "
            "your intended area.",
            pendingFillCellCount
        );
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Tip: Make sure your shape is fully enclosed by walls."
        );
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            // Clear pending fill
            CanvasPanel& canvas = m_ui.GetCanvasPanel();
            canvas.hasPendingTileFill = false;
            canvas.pendingTileFillChanges.clear();
            canvas.hasPendingRoomFill = false;
            canvas.pendingRoomFillAssignments.clear();
            canvas.pendingRoomFillActiveRoomId.clear();
            
            pendingFillType = PendingFillType::None;
            pendingFillCellCount = 0;
            fillConfirmed = false;
            showFillConfirmationModal = false;
            fillConfirmationModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Confirm button (styled as primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Fill Anyway", ImVec2(buttonWidth, 0))) {
            CanvasPanel& canvas = m_ui.GetCanvasPanel();
            
            // Apply the pending fill
            if (canvas.hasPendingTileFill && 
                !canvas.pendingTileFillChanges.empty()) {
                // Apply tile fill
                for (const auto& change : canvas.pendingTileFillChanges) {
                    model.SetTileAt(
                        change.roomId, 
                        change.x, 
                        change.y, 
                        change.newTileId
                    );
                }
                
                auto cmd = std::make_unique<FillTilesCommand>(
                    canvas.pendingTileFillChanges
                );
                history.AddCommand(std::move(cmd), model, false);
                
                canvas.hasPendingTileFill = false;
                canvas.pendingTileFillChanges.clear();
            }
            
            if (canvas.hasPendingRoomFill && 
                !canvas.pendingRoomFillAssignments.empty()) {
                // Apply room fill
                for (const auto& assignment : 
                     canvas.pendingRoomFillAssignments) {
                    model.SetCellRoom(
                        assignment.x, 
                        assignment.y, 
                        assignment.newRoomId
                    );
                }
                
                auto cmd = std::make_unique<ModifyRoomAssignmentsCommand>(
                    canvas.pendingRoomFillAssignments
                );
                history.AddCommand(std::move(cmd), model, false);
                
                // Generate perimeter walls if enabled
                if (model.autoGenerateRoomWalls && 
                    !canvas.pendingRoomFillActiveRoomId.empty()) {
                    model.GenerateRoomPerimeterWalls(
                        canvas.pendingRoomFillActiveRoomId
                    );
                }
                
                canvas.hasPendingRoomFill = false;
                canvas.pendingRoomFillAssignments.clear();
                canvas.pendingRoomFillActiveRoomId.clear();
            }
            
            pendingFillType = PendingFillType::None;
            pendingFillCellCount = 0;
            fillConfirmed = true;
            showFillConfirmationModal = false;
            fillConfirmationModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}

void Modals::RenderAutosaveRecoveryModal(App& app, Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!autosaveRecoveryModalOpened) {
        ImGui::OpenPopup("Autosave Recovery");
        autosaveRecoveryModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Autosave Recovery", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Unsaved Work Detected");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "Cartograph detected unsaved work from a previous session. "
            "Would you like to recover it?");
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Note: Recovering will load the autosaved data. You can "
            "manually save it when ready.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        if (ImGui::Button("Recover", ImVec2(buttonWidth, 0))) {
            // Load autosave
            std::string autosaveDir = Platform::GetAutosaveDir();
            std::string autosavePath = autosaveDir + "autosave.json";
            std::string metadataPath = autosaveDir + "metadata.json";
            
            Model recoveredModel;
            if (IOJson::LoadFromFile(autosavePath, recoveredModel)) {
                model = recoveredModel;
                model.MarkDirty();  // Mark as dirty so user must save
                
                // Restore project path from metadata if available
                std::ifstream metaFile(metadataPath);
                if (metaFile.is_open()) {
                    try {
                        nlohmann::json meta = nlohmann::json::parse(metaFile);
                        std::string projectPath = meta.value("projectPath", "");
                        if (!projectPath.empty()) {
                            app.SetCurrentFilePath(projectPath);
                        }
                    } catch (...) {
                        // Metadata parse error - continue without path
                    }
                }
                
                m_ui.ShowToast("Recovered from autosave", Toast::Type::Success);
                app.ShowEditor();
            } else {
                m_ui.ShowToast("Failed to load autosave", Toast::Type::Error);
            }
            showAutosaveRecoveryModal = false;
            autosaveRecoveryModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        if (ImGui::Button("Discard", ImVec2(buttonWidth, 0))) {
            // Clean up autosave and continue
            std::string autosaveDir = Platform::GetAutosaveDir();
            try {
                std::filesystem::remove(autosaveDir + "autosave.json");
                std::filesystem::remove(autosaveDir + "metadata.json");
            } catch (...) {
                // Ignore errors
            }
            showAutosaveRecoveryModal = false;
            autosaveRecoveryModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderLoadingModal(App& app, Model& model, JobQueue& jobs, 
                            IconManager& icons) {
    if (!showLoadingModal) {
        loadingModalOpened = false;
        return;
    }
    
    // Only call OpenPopup once when modal is first shown
    if (!loadingModalOpened) {
        ImGui::OpenPopup("Loading Project");
        loadingModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Loading Project", nullptr,
                               ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoCollapse)) {
        ImGui::Spacing();
        
        // Title
        ImGui::TextColored(
            ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
            "Opening Project"
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // File name (truncated if too long)
        std::string displayName = loadingFileName;
        if (displayName.length() > 45) {
            displayName = "..." + displayName.substr(
                displayName.length() - 42
            );
        }
        ImGui::Text("%s", displayName.c_str());
        
        ImGui::Spacing();
        
        // Indeterminate progress bar (pulsing animation)
        float time = static_cast<float>(ImGui::GetTime());
        float progress = (sinf(time * 3.0f) + 1.0f) * 0.5f;  // 0.0-1.0 pulse
        
        ImGui::ProgressBar(
            progress,
            ImVec2(-1, 0),
            ""  // No text overlay
        );
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Cancel button
        float buttonWidth = 120.0f;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - buttonWidth) * 0.5f
        );
        
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            loadingCancelled = true;
            loadingModalOpened = false;
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderQuitConfirmationModal(App& app, Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!quitConfirmationModalOpened) {
        ImGui::OpenPopup("Unsaved Changes");
        quitConfirmationModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Appearing);
    
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
            "before quitting?");
        ImGui::Spacing();
        
        if (app.GetState() != AppState::Editor) {
            ImGui::TextDisabled("Current project has not been saved.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showQuitConfirmationModal = false;
            quitConfirmationModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            // Quit without saving - clean up autosave since user explicitly
            // chose to discard changes (recovery is for unexpected quits only)
            app.CleanupAutosave();
            showQuitConfirmationModal = false;
            quitConfirmationModalOpened = false;
            ImGui::CloseCurrentPopup();
            app.ForceQuit();
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            // Save then quit
            app.SaveProject();
            
            // Check if save was successful (dirty flag should be cleared)
            if (!model.dirty) {
                showQuitConfirmationModal = false;
                quitConfirmationModalOpened = false;
                ImGui::CloseCurrentPopup();
                app.ForceQuit();
            } else {
                // Save failed or was canceled, stay in modal
                m_ui.ShowToast("Please save the project before quitting", 
                         Toast::Type::Warning);
            }
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph
