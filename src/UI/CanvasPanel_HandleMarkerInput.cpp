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

void CanvasPanel::HandleMarkerInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Marker tool: Left-click to place/edit markers
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert to world coordinates
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                // Convert to fractional tile coordinates (Y-up: negate world Y)
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
                
                // Check if we clicked near an existing marker
                Marker* clickedMarker = 
                    model.FindMarkerNear(tileX, tileY, 0.5f);
                
                if (clickedMarker && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                    // Shift+Click: Delete marker
                    auto cmd = std::make_unique<DeleteMarkerCommand>(
                        clickedMarker->id
                    );
                    history.AddCommand(std::move(cmd), model);
                    
                    selectedMarkerId.clear();  // Clear selection
                } else if (clickedMarker) {
                    // Click existing marker: Select and start drag
                    selectedMarkerId = clickedMarker->id;
                    isDraggingMarker = true;
                    dragStartX = clickedMarker->x;
                    dragStartY = clickedMarker->y;
                    
                    // Load marker properties into UI
                    selectedIconName = clickedMarker->icon;
                    markerLabel = clickedMarker->label;
                    markerColor = clickedMarker->color;
                    
                    // Update hex input
                    std::string hexStr = markerColor.ToHex(false);
                    std::strncpy(markerColorHex, hexStr.c_str(), 
                                sizeof(markerColorHex) - 1);
                    markerColorHex[sizeof(markerColorHex) - 1] = '\0';
                } else {
                    // Place new marker
                    Marker newMarker;
                    newMarker.id = model.GenerateMarkerId();
                    newMarker.roomId = "";  // Global for now
                    newMarker.x = tileX;
                    newMarker.y = tileY;
                    newMarker.kind = "custom";
                    newMarker.label = markerLabel;
                    newMarker.icon = selectedIconName;
                    newMarker.color = markerColor;
                    newMarker.size = 0.6f;
                    newMarker.showLabel = !markerLabel.empty();
                    
                    auto cmd = std::make_unique<PlaceMarkerCommand>(
                        newMarker, true
                    );
                    history.AddCommand(std::move(cmd), model);
                }
            }
            
            // Right-click (two-finger) to delete marker
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert to world coordinates
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                // Convert to fractional tile coordinates (Y-up: negate world Y)
                float tileX = wx / model.grid.tileWidth;
                float tileY = -wy / model.grid.tileHeight;
                
                // Check if we right-clicked near an existing marker
                Marker* clickedMarker = 
                    model.FindMarkerNear(tileX, tileY, 0.5f);
                
                if (clickedMarker) {
                    // Delete marker
                    auto cmd = std::make_unique<DeleteMarkerCommand>(
                        clickedMarker->id
                    );
                    history.AddCommand(std::move(cmd), model);
                    
                    // Clear selection if we deleted the selected marker
                    if (selectedMarkerId == clickedMarker->id) {
                        selectedMarkerId.clear();
                    }
                }
            }
            
            // Handle marker dragging
            Marker* selectedMarker = model.FindMarker(selectedMarkerId);
            if (isDraggingMarker && selectedMarker) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    // Update marker position while dragging
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
                    
                    // Update marker position
                    selectedMarker->x = tileX;
                    selectedMarker->y = tileY;
                    model.MarkDirty();
                } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    // Finish dragging, create command for undo
                    if (dragStartX != selectedMarker->x || 
                        dragStartY != selectedMarker->y) {
                        auto cmd = std::make_unique<MoveMarkersCommand>(
                            selectedMarker->id,
                            dragStartX, dragStartY,
                            selectedMarker->x, selectedMarker->y
                        );
                        history.AddCommand(std::move(cmd), model, false);
                    }
                    
                    isDraggingMarker = false;
                }
            }
}

}  // namespace Cartograph
