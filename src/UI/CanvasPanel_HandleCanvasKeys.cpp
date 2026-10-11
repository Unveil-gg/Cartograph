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

void CanvasPanel::HandleCanvasKeys(
    Model& model,
    Canvas& canvas,
    History& history,
    KeymapManager& keymap
) {
        // Selection operations take priority when Select tool is active
        if (currentTool == Tool::Select) {
            // Copy selection (Ctrl+C)
            if (keymap.IsActionTriggered("copy") && hasSelection) {
                CopySelection(model);
            }
            
            // Cut selection (Ctrl+X) - copy to clipboard + delete immediately
            if (keymap.IsActionTriggered("cut") && hasSelection) {
                CopySelection(model);
                DeleteSelection(model, history);
            }
            
            // Paste (Ctrl+V) - enter paste mode if clipboard has content
            if (keymap.IsActionTriggered("paste") && !clipboard.IsEmpty()) {
                EnterPasteMode();
            }
            
            // Delete selection (Delete/Backspace)
            if (hasSelection && 
                (keymap.IsActionTriggered("delete") || 
                 keymap.IsActionTriggered("deleteAlt"))) {
                DeleteSelection(model, history);
            }
            
            // Escape cancels paste mode or clears selection
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (isPasteMode) {
                    ExitPasteMode();
                } else if (hasSelection) {
                    ClearSelection();
                }
            }
            
            // Arrow keys nudge selection (moves content immediately)
            if (hasSelection && !isPasteMode) {
                int nudgeX = 0, nudgeY = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) nudgeX = -1;
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nudgeX = 1;
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) nudgeY = -1;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) nudgeY = 1;
                
                if (nudgeX != 0 || nudgeY != 0) {
                    MoveSelection(model, history, nudgeX, nudgeY);
                }
            }
            
            // Select All (Ctrl+A)
            if (keymap.IsActionTriggered("selectAll")) {
                SelectAll(model);
            }
        }
        // Marker copy/paste (when not in Select tool)
        else {
            // Copy selected marker
            if (keymap.IsActionTriggered("copy")) {
                Marker* selMarker = model.FindMarker(selectedMarkerId);
                if (selMarker) {
                    copiedMarkers.clear();
                    copiedMarkers.push_back(*selMarker);
                }
            }
            
            // Paste marker
            if (keymap.IsActionTriggered("paste")) {
                if (!copiedMarkers.empty()) {
                    // Paste at mouse position or offset from original
                    ImVec2 mousePos = ImGui::GetMousePos();
                    float wx, wy;
                    canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                    
                    // Y-up: negate world Y for tile coordinates
                    float tileX = wx / model.grid.tileWidth;
                    float tileY = -wy / model.grid.tileHeight;
                    
                    // Snap to nearest snap point based on grid preset
                    auto snapPoints = model.GetMarkerSnapPoints();
                    int baseTileX = static_cast<int>(std::floor(tileX));
                    int baseTileY = static_cast<int>(std::floor(tileY));
                    float fractionalX = tileX - baseTileX;
                    float fractionalY = tileY - baseTileY;
                    
                    float minDist = FLT_MAX;
                    float bestSnapX = 0.5f, bestSnapY = 0.5f;
                    
                    for (const auto& snap : snapPoints) {
                        float dx = fractionalX - snap.first;
                        float dy = fractionalY - snap.second;
                        float dist = dx*dx + dy*dy;
                        if (dist < minDist) {
                            minDist = dist;
                            bestSnapX = snap.first;
                            bestSnapY = snap.second;
                        }
                    }
                    
                    tileX = baseTileX + bestSnapX;
                    tileY = baseTileY + bestSnapY;
                    
                    for (const auto& marker : copiedMarkers) {
                        Marker newMarker = marker;
                        newMarker.id = model.GenerateMarkerId();
                        newMarker.x = tileX;
                        newMarker.y = tileY;
                        
                        auto cmd = std::make_unique<PlaceMarkerCommand>(
                            newMarker, true
                        );
                        history.AddCommand(std::move(cmd), model);
                    }
                }
            }
            
            // Delete selected marker
            if (!selectedMarkerId.empty() && 
                (keymap.IsActionTriggered("delete") || 
                 keymap.IsActionTriggered("deleteAlt"))) {
                auto cmd = std::make_unique<DeleteMarkerCommand>(
                    selectedMarkerId
                );
                history.AddCommand(std::move(cmd), model);   
                selectedMarkerId.clear();
            }
        }
}

}  // namespace Cartograph
