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

void CanvasPanel::HandleRoomFillInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Room Fill tool: Click to flood-fill area into active room
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Check bounds (support negative coordinates)
                if (tx >= model.grid.minCol && tx < model.grid.cols && 
                    ty >= model.grid.minRow && ty < model.grid.rows) {
                    
                    std::string startRoomId = model.GetCellRoom(tx, ty);
                    
                    // Only fill if starting from empty or different room
                    if (startRoomId != activeRoomId) {
                        // Calculate fill boundary from content bounds + margin
                        ContentBounds bounds = model.CalculateContentBounds();
                        const int FILL_MARGIN = 20;
                        int boundMinX, boundMinY, boundMaxX, boundMaxY;
                        
                        if (bounds.isEmpty) {
                            boundMinX = tx - FILL_MARGIN;
                            boundMinY = ty - FILL_MARGIN;
                            boundMaxX = tx + FILL_MARGIN;
                            boundMaxY = ty + FILL_MARGIN;
                        } else {
                            boundMinX = bounds.minX - FILL_MARGIN;
                            boundMinY = bounds.minY - FILL_MARGIN;
                            boundMaxX = bounds.maxX + FILL_MARGIN;
                            boundMaxY = bounds.maxY + FILL_MARGIN;
                        }
                        
                        // Clamp to grid bounds (support negative coordinates)
                        boundMinX = std::max(model.grid.minCol, boundMinX);
                        boundMinY = std::max(model.grid.minRow, boundMinY);
                        boundMaxX = std::min(model.grid.cols - 1, boundMaxX);
                        boundMaxY = std::min(model.grid.rows - 1, boundMaxY);
                        
                        // Fill limits
                        const size_t SOFT_LIMIT = 500;
                        const size_t HARD_LIMIT = 10000;
                        
                        // Perform flood fill using BFS (don't apply yet)
                        std::vector<ModifyRoomAssignmentsCommand::CellAssignment> 
                            fillAssignments;
                        std::vector<std::pair<int, int>> toVisit;
                        std::set<std::pair<int, int>> visited;
                        
                        toVisit.push_back({tx, ty});
                        bool exceededHardLimit = false;
                        
                        while (!toVisit.empty()) {
                            auto [x, y] = toVisit.back();
                            toVisit.pop_back();
                            
                            // Skip if already visited
                            if (visited.count({x, y})) {
                                continue;
                            }
                            visited.insert({x, y});
                            
                            // Check fill boundary
                            if (x < boundMinX || x > boundMaxX ||
                                y < boundMinY || y > boundMaxY) {
                                continue;
                            }
                            
                            // Check grid bounds (support negative coordinates)
                            if (x < model.grid.minCol || x >= model.grid.cols ||
                                y < model.grid.minRow || y >= model.grid.rows) {
                                continue;
                            }
                            
                            // Get current room
                            std::string currentRoom = model.GetCellRoom(x, y);
                            
                            // Skip if not matching start condition
                            if (currentRoom != startRoomId) {
                                continue;
                            }
                            
                            // Check hard limit
                            if (fillAssignments.size() >= HARD_LIMIT) {
                                exceededHardLimit = true;
                                break;
                            }
                            
                            // Add this cell to fill
                            ModifyRoomAssignmentsCommand::CellAssignment assignment;
                            assignment.x = x;
                            assignment.y = y;
                            assignment.oldRoomId = startRoomId;
                            assignment.newRoomId = activeRoomId;
                            fillAssignments.push_back(assignment);
                            
                            // Check all 4 neighbors - only cross if no edge
                            EdgeSide sides[] = {
                                EdgeSide::North, EdgeSide::South,
                                EdgeSide::East, EdgeSide::West
                            };
                            int dx[] = {0, 0, 1, -1};
                            int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
                            
                            for (int i = 0; i < 4; ++i) {
                                EdgeId edgeId = MakeEdgeId(x, y, sides[i]);
                                EdgeState edgeState = model.GetEdgeState(edgeId);
                                
                                // Only cross if edge is None (walls/doors block)
                                if (edgeState == EdgeState::None) {
                                    int nx = x + dx[i];
                                    int ny = y + dy[i];
                                    
                                    if (!visited.count({nx, ny})) {
                                        toVisit.push_back({nx, ny});
                                    }
                                }
                            }
                        }
                        
                        // Handle fill based on size
                        if (exceededHardLimit) {
                            // Hard limit exceeded: refuse
                            if (modals) {
                                modals->pendingFillType = 
                                    Modals::PendingFillType::None;
                                modals->pendingFillCellCount = HARD_LIMIT;
                            }
                        } else if (fillAssignments.size() > SOFT_LIMIT) {
                            // Soft limit: ask for confirmation
                            hasPendingRoomFill = true;
                            pendingRoomFillAssignments = 
                                std::move(fillAssignments);
                            pendingRoomFillActiveRoomId = activeRoomId;
                            if (modals) {
                                modals->showFillConfirmationModal = true;
                                modals->pendingFillType = 
                                    Modals::PendingFillType::Room;
                                modals->pendingFillCellCount = 
                                    pendingRoomFillAssignments.size();
                                modals->fillConfirmed = false;
                            }
                        } else if (!fillAssignments.empty()) {
                            // Within limits: apply immediately
                            for (const auto& assignment : fillAssignments) {
                                model.SetCellRoom(
                                    assignment.x, 
                                    assignment.y, 
                                    assignment.newRoomId
                                );
                            }
                            
                            auto cmd = std::make_unique<
                                ModifyRoomAssignmentsCommand>(fillAssignments);
                            history.AddCommand(std::move(cmd), model, false);
                            
                            // Generate perimeter walls if enabled
                            if (model.autoGenerateRoomWalls && 
                                !activeRoomId.empty()) {
                                model.GenerateRoomPerimeterWalls(activeRoomId);
                            }
                        }
                    }
                }
            }
}

}  // namespace Cartograph
