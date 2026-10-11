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

void CanvasPanel::HandleRoomEraseInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Room Erase tool: Click/drag to remove cells from rooms
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert mouse position to tile coordinates
            int tx, ty;
            canvas.ScreenToTile(
                mousePos.x, mousePos.y,
                model.grid.tileWidth, model.grid.tileHeight,
                &tx, &ty
            );
            
            // Left click: remove cell from any room
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                // Start erasing if first click
                if (!isPaintingRoomCells) {
                    isPaintingRoomCells = true;
                    lastRoomPaintX = tx;
                    lastRoomPaintY = ty;
                }
                
                // Erase if we moved to a new cell
                if (tx != lastRoomPaintX || ty != lastRoomPaintY) {
                    // Get all cells along the line
                    std::vector<std::pair<int, int>> cellsToErase;
                    
                    // Simple line rasterization
                    int dx = abs(tx - lastRoomPaintX);
                    int dy = abs(ty - lastRoomPaintY);
                    int sx = (lastRoomPaintX < tx) ? 1 : -1;
                    int sy = (lastRoomPaintY < ty) ? 1 : -1;
                    int err = dx - dy;
                    
                    int cx = lastRoomPaintX;
                    int cy = lastRoomPaintY;
                    
                    while (true) {
                        cellsToErase.push_back({cx, cy});
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
                    
                    // Erase all cells along the line with brush size
                    int brushRadius = (eraserBrushSize - 1) / 2;
                    for (const auto& cell : cellsToErase) {
                        int centerX = cell.first;
                        int centerY = cell.second;
                        
                        // Erase in brush area
                        for (int dy = -brushRadius; dy <= brushRadius; ++dy) {
                            for (int dx = -brushRadius; dx <= brushRadius; ++dx) {
                                int cellX = centerX + dx;
                                int cellY = centerY + dy;
                                
                                std::string oldRoomId = model.GetCellRoom(
                                    cellX, cellY
                                );
                                
                                // Only erase if cell has a room
                                if (!oldRoomId.empty()) {
                                    ModifyRoomAssignmentsCommand::CellAssignment 
                                        assignment;
                                    assignment.x = cellX;
                                    assignment.y = cellY;
                                    assignment.oldRoomId = oldRoomId;
                                    assignment.newRoomId = "";  // Empty = unassign
                                    
                                    currentRoomAssignments.push_back(assignment);
                                    
                                    // Apply immediately for visual feedback
                                    model.ClearCellRoom(cellX, cellY);
                                }
                            }
                        }
                    }
                    
                    lastRoomPaintX = tx;
                    lastRoomPaintY = ty;
                }
            }
            else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                // Finish erasing
                if (isPaintingRoomCells && !currentRoomAssignments.empty()) {
                    auto cmd = std::make_unique<ModifyRoomAssignmentsCommand>(
                        currentRoomAssignments
                    );
                    history.AddCommand(std::move(cmd), model, false);
                    currentRoomAssignments.clear();
                }
                isPaintingRoomCells = false;
                lastRoomPaintX = -1;
                lastRoomPaintY = -1;
            }
}

}  // namespace Cartograph
