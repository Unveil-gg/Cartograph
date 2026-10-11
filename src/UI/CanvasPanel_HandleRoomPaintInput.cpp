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

void CanvasPanel::HandleRoomPaintInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Room Paint tool: Click/drag to assign cells to active room
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert mouse position to tile coordinates
            int tx, ty;
            canvas.ScreenToTile(
                mousePos.x, mousePos.y,
                model.grid.tileWidth, model.grid.tileHeight,
                &tx, &ty
            );
            
            // Left click: assign cell to active room
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                // Start painting if first click
                if (!isPaintingRoomCells) {
                    isPaintingRoomCells = true;
                    lastRoomPaintX = tx;
                    lastRoomPaintY = ty;
                }
                
                // Paint if we moved to a new cell
                if (tx != lastRoomPaintX || ty != lastRoomPaintY) {
                    // Get all cells along the line
                    std::vector<std::pair<int, int>> cellsToAssign;
                    
                    // Simple line rasterization (Bresenham-like)
                    int dx = abs(tx - lastRoomPaintX);
                    int dy = abs(ty - lastRoomPaintY);
                    int sx = (lastRoomPaintX < tx) ? 1 : -1;
                    int sy = (lastRoomPaintY < ty) ? 1 : -1;
                    int err = dx - dy;
                    
                    int cx = lastRoomPaintX;
                    int cy = lastRoomPaintY;
                    
                    while (true) {
                        cellsToAssign.push_back({cx, cy});
                        if (cx == tx && cy == ty) break;
                        int e2 = 2 * err;
                        if (e2 > -dy) {
                            err -= dy;
                            cx += sx;
                        }
                        if (e2 < dx) {
                            err += dx;
                            cy += sy;
                        }
                    }
                    
                    // Assign all cells along the line
                    for (const auto& cell : cellsToAssign) {
                        int cellX = cell.first;
                        int cellY = cell.second;
                        
                        std::string oldRoomId = model.GetCellRoom(
                            cellX, cellY
                        );
                        
                        // Only assign if different
                        if (oldRoomId != activeRoomId) {
                            ModifyRoomAssignmentsCommand::CellAssignment 
                                assignment;
                            assignment.x = cellX;
                            assignment.y = cellY;
                            assignment.oldRoomId = oldRoomId;
                            assignment.newRoomId = activeRoomId;
                            
                            currentRoomAssignments.push_back(assignment);
                            
                            // Apply immediately for visual feedback
                            model.SetCellRoom(cellX, cellY, activeRoomId);
                        }
                    }
                    
                    lastRoomPaintX = tx;
                    lastRoomPaintY = ty;
                }
            }
            else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                // Finish painting
                if (isPaintingRoomCells && !currentRoomAssignments.empty()) {
                    auto cmd = std::make_unique<ModifyRoomAssignmentsCommand>(
                        currentRoomAssignments
                    );
                    history.AddCommand(std::move(cmd), model, false);
                    currentRoomAssignments.clear();
                    
                    // Generate perimeter walls if enabled
                    if (model.autoGenerateRoomWalls && !activeRoomId.empty()) {
                        model.GenerateRoomPerimeterWalls(activeRoomId);
                    }
                }
                isPaintingRoomCells = false;
                lastRoomPaintX = -1;
                lastRoomPaintY = -1;
            }
}

}  // namespace Cartograph
