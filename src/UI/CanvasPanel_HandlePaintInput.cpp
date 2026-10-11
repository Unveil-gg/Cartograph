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

void CanvasPanel::HandlePaintInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Paint tool: 
            // - Room paint mode: assign/unassign cells to selected room
            // - Hover over edge: highlight and show state
            // - Click edge: cycle through None -> Wall -> Door -> None
            // - W key + click: set to Wall
            // - D key + click: set to Door
            // - Otherwise: paint/erase tiles
            
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Room paint mode: assign cells to selected room
            if (roomPaintMode && !selectedRoomId.empty()) {
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Left click: assign cell to room
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    // Check if moved to new cell or just started
                    if (!isPaintingRoomCells || tx != lastRoomPaintX || 
                        ty != lastRoomPaintY) {
                        
                        // Get cells to assign (interpolate if dragging)
                        std::vector<std::pair<int, int>> cellsToAssign;
                        if (isPaintingRoomCells && lastRoomPaintX >= 0 && 
                            lastRoomPaintY >= 0) {
                            // Interpolate from last position to current
                            cellsToAssign = GetTilesAlongLine(
                                lastRoomPaintX, lastRoomPaintY, tx, ty
                            );
                        } else {
                            // First cell
                            cellsToAssign.push_back({tx, ty});
                        }
                        
                        // Assign all cells along the line
                        for (const auto& cell : cellsToAssign) {
                            int cellX = cell.first;
                            int cellY = cell.second;
                            
                            std::string oldRoomId = model.GetCellRoom(
                                cellX, cellY
                            );
                            
                            // Only assign if different
                            if (oldRoomId != selectedRoomId) {
                                ModifyRoomAssignmentsCommand::CellAssignment 
                                    assignment;
                                assignment.x = cellX;
                                assignment.y = cellY;
                                assignment.oldRoomId = oldRoomId;
                                assignment.newRoomId = selectedRoomId;
                                
                                currentRoomAssignments.push_back(assignment);
                                
                                // Apply immediately for visual feedback
                                model.SetCellRoom(cellX, cellY, selectedRoomId);
                            }
                        }
                        
                        lastRoomPaintX = tx;
                        lastRoomPaintY = ty;
                        isPaintingRoomCells = true;
                    }
                }
                // Right click (two-finger): unassign cell from room
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    // Check if moved to new cell or just started
                    if (!isPaintingRoomCells || tx != lastRoomPaintX || 
                        ty != lastRoomPaintY) {
                        
                        // Get cells to unassign (interpolate if dragging)
                        std::vector<std::pair<int, int>> cellsToUnassign;
                        if (isPaintingRoomCells && lastRoomPaintX >= 0 && 
                            lastRoomPaintY >= 0) {
                            // Interpolate from last position to current
                            cellsToUnassign = GetTilesAlongLine(
                                lastRoomPaintX, lastRoomPaintY, tx, ty
                            );
                        } else {
                            // First cell
                            cellsToUnassign.push_back({tx, ty});
                        }
                        
                        // Unassign all cells along the line
                        for (const auto& cell : cellsToUnassign) {
                            int cellX = cell.first;
                            int cellY = cell.second;
                            
                            std::string currentRoomId = model.GetCellRoom(
                                cellX, cellY
                            );
                            
                            // Only unassign if currently assigned to selected room
                            if (currentRoomId == selectedRoomId) {
                                ModifyRoomAssignmentsCommand::CellAssignment 
                                    assignment;
                                assignment.x = cellX;
                                assignment.y = cellY;
                                assignment.oldRoomId = currentRoomId;
                                assignment.newRoomId = "";  // Empty = unassign
                                
                                currentRoomAssignments.push_back(assignment);
                                
                                // Apply immediately for visual feedback
                                model.ClearCellRoom(cellX, cellY);
                            }
                        }
                        
                        lastRoomPaintX = tx;
                        lastRoomPaintY = ty;
                        isPaintingRoomCells = true;
                    }
                }
                
                // When mouse is released, commit the room assignment command
                bool mouseReleased = 
                    ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                    ImGui::IsMouseReleased(ImGuiMouseButton_Right);
                
                if (isPaintingRoomCells && mouseReleased) {
                    if (!currentRoomAssignments.empty()) {
                        auto cmd = std::make_unique<
                            ModifyRoomAssignmentsCommand
                        >(currentRoomAssignments);
                        // Changes already applied, just store for undo/redo
                        history.AddCommand(std::move(cmd), model, false);
                        currentRoomAssignments.clear();
                    }
                    isPaintingRoomCells = false;
                    lastRoomPaintX = -1;
                    lastRoomPaintY = -1;
                }
            }
            // Regular paint mode: edges and tiles
            else {
                // First, check if we're hovering near an edge
                EdgeId edgeId;
                EdgeSide edgeSide;
                isHoveringEdge = DetectEdgeHover(
                    mousePos.x, mousePos.y, canvas, model.grid,
                    &edgeId, &edgeSide
                );
                
                if (isHoveringEdge) {
                hoveredEdge = edgeId;
                
                // Handle edge clicking
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    EdgeState currentState = model.GetEdgeState(edgeId);
                    EdgeState newState;
                    
                    // Check for direct state shortcuts
                    if (ImGui::IsKeyDown(ImGuiKey_W)) {
                        newState = EdgeState::Wall;
                    } else if (ImGui::IsKeyDown(ImGuiKey_D)) {
                        newState = EdgeState::Door;
                    } else {
                        // Cycle through states
                        newState = model.CycleEdgeState(currentState);
                    }
                    
                    // Only modify if state changed
                    if (newState != currentState) {
                        ModifyEdgesCommand::EdgeChange change;
                        change.edgeId = edgeId;
                        change.oldState = currentState;
                        change.newState = newState;
                        
                        currentEdgeChanges.push_back(change);
                        
                        // Apply immediately for visual feedback
                        model.SetEdgeState(edgeId, newState);
                        
                        // Trigger grid expansion if needed
                        int tx, ty;
                        canvas.ScreenToTile(
                            mousePos.x, mousePos.y,
                            model.grid.tileWidth, model.grid.tileHeight,
                            &tx, &ty
                        );
                        model.ExpandGridIfNeeded(tx, ty);
                        
                        isModifyingEdges = true;
                    }
                }
                // Right-click (two-finger): delete edge
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    EdgeState currentState = model.GetEdgeState(edgeId);
                    
                    // Only delete if there's an edge to delete
                    if (currentState != EdgeState::None) {
                        ModifyEdgesCommand::EdgeChange change;
                        change.edgeId = edgeId;
                        change.oldState = currentState;
                        change.newState = EdgeState::None;
                        
                        currentEdgeChanges.push_back(change);
                        
                        // Apply immediately for visual feedback
                        model.SetEdgeState(edgeId, EdgeState::None);
                        
                        isModifyingEdges = true;
                    }
                }
                
                // When mouse is released, commit edge changes
                if (isModifyingEdges && 
                    (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                     ImGui::IsMouseReleased(ImGuiMouseButton_Right))) {
                    if (!currentEdgeChanges.empty()) {
                        auto cmd = std::make_unique<ModifyEdgesCommand>(
                            currentEdgeChanges
                        );
                        history.AddCommand(std::move(cmd), model, false);
                        currentEdgeChanges.clear();
                    }
                    isModifyingEdges = false;
                }
                } else {
                // Not hovering edge, handle tile painting/erasing
                bool shouldPaint = false;
                bool shouldErase = false;
                
                // Check for two-finger gesture (acts as erase)
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    shouldErase = true;
                    twoFingerEraseActive = true;
                }
                // Check for E key + left mouse (erase modifier)
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && 
                         ImGui::IsKeyDown(ImGuiKey_E)) {
                    shouldErase = true;
                }
                // Check for left mouse button (primary paint input)
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    shouldPaint = true;
                }
                
                if (shouldPaint) {
                    // Convert mouse position to tile coordinates
                    int tx, ty;
                    canvas.ScreenToTile(
                        mousePos.x, mousePos.y,
                        model.grid.tileWidth, model.grid.tileHeight,
                        &tx, &ty
                    );
                    
                    // Check if we've moved to a new tile or just started
                    if (!isPainting || tx != lastPaintedTileX || 
                        ty != lastPaintedTileY) {
                        
                        // Paint tiles globally (using "" as roomId)
                        const std::string globalRoomId = "";
                        
                        // Get tiles to paint (interpolate if dragging)
                        std::vector<std::pair<int, int>> tilesToPaint;
                        if (isPainting && lastPaintedTileX >= 0 && 
                            lastPaintedTileY >= 0) {
                            // Interpolate from last position to current
                            tilesToPaint = GetTilesAlongLine(
                                lastPaintedTileX, lastPaintedTileY, 
                                tx, ty
                            );
                        } else {
                            // First tile
                            tilesToPaint.push_back({tx, ty});
                        }
                        
                        // Paint all tiles along the line
                        for (const auto& tile : tilesToPaint) {
                            int tileX = tile.first;
                            int tileY = tile.second;
                            
                            int oldTileId = model.GetTileAt(
                                globalRoomId, tileX, tileY
                            );
                            
                            // Only paint if the tile is different
                            if (oldTileId != selectedTileId) {
                                PaintTilesCommand::TileChange change;
                                change.roomId = globalRoomId;
                                change.x = tileX;
                                change.y = tileY;
                                change.oldTileId = oldTileId;
                                change.newTileId = selectedTileId;
                                
                                currentPaintChanges.push_back(change);
                                
                                // Apply immediately for visual feedback
                                model.SetTileAt(
                                    globalRoomId, tileX, tileY, selectedTileId
                                );
                            }
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
                
                
                // Handle erase with E+Mouse1 or right-click/two-finger
                if (shouldErase) {
                    // Convert mouse position to tile coordinates
                    int tx, ty;
                    canvas.ScreenToTile(
                        mousePos.x, mousePos.y,
                        model.grid.tileWidth, model.grid.tileHeight,
                        &tx, &ty
                    );
                    
                    // Check if we've moved to a new tile or just started
                    if (!isPainting || tx != lastPaintedTileX || 
                        ty != lastPaintedTileY) {
                        
                        // Erase tiles globally (using "" as roomId)
                        const std::string globalRoomId = "";
                        
                        // Get tiles to erase (interpolate if dragging)
                        std::vector<std::pair<int, int>> tilesToErase;
                        if (isPainting && lastPaintedTileX >= 0 && 
                            lastPaintedTileY >= 0) {
                            // Interpolate from last position to current
                            tilesToErase = GetTilesAlongLine(
                                lastPaintedTileX, lastPaintedTileY, 
                                tx, ty
                            );
                        } else {
                            // First tile
                            tilesToErase.push_back({tx, ty});
                        }
                        
                        // Erase all tiles along the line
                        for (const auto& tile : tilesToErase) {
                            int tileX = tile.first;
                            int tileY = tile.second;
                            
                            int oldTileId = model.GetTileAt(
                                globalRoomId, tileX, tileY
                            );
                            
                            // Only erase if there's something to erase
                            if (oldTileId != 0) {
                                PaintTilesCommand::TileChange change;
                                change.roomId = globalRoomId;
                                change.x = tileX;
                                change.y = tileY;
                                change.oldTileId = oldTileId;
                                change.newTileId = 0;  // 0 = empty/erase
                                
                                currentPaintChanges.push_back(change);
                                
                                // Apply immediately for visual feedback
                                model.SetTileAt(globalRoomId, tileX, tileY, 0);
                            }
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
                }  // End of "not hovering edge" block
            }  // End of "regular paint mode" block
}

}  // namespace Cartograph
