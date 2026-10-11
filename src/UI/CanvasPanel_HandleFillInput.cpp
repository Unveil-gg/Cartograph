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

void CanvasPanel::HandleFillInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Fill tool: Left-click to flood fill connected tiles
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Fill tiles globally (using "" as roomId)
                const std::string globalRoomId = "";
                
                // Get the original tile ID to replace
                int originalTileId = model.GetTileAt(globalRoomId, tx, ty);
                
                // Only fill if we're changing to a different tile
                if (originalTileId != selectedTileId) {
                    // Calculate fill boundary from content bounds + margin
                    ContentBounds bounds = model.CalculateContentBounds();
                    const int FILL_MARGIN = 20;  // Cells beyond content
                    int boundMinX, boundMinY, boundMaxX, boundMaxY;
                    
                    if (bounds.isEmpty) {
                        // No content: use small area around click point
                        boundMinX = tx - FILL_MARGIN;
                        boundMinY = ty - FILL_MARGIN;
                        boundMaxX = tx + FILL_MARGIN;
                        boundMaxY = ty + FILL_MARGIN;
                    } else {
                        // Content exists: use bounds + margin
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
                    const size_t SOFT_LIMIT = 500;    // Confirm above this
                    const size_t HARD_LIMIT = 10000;  // Refuse above this
                    
                    // Perform flood fill using BFS with bounds
                    std::vector<PaintTilesCommand::TileChange> fillChanges;
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
                        
                        // Check fill boundary (content bounds + margin)
                        if (x < boundMinX || x > boundMaxX ||
                            y < boundMinY || y > boundMaxY) {
                            continue;
                        }
                        
                        // Check grid bounds (support negative coordinates)
                        if (x < model.grid.minCol || x >= model.grid.cols ||
                            y < model.grid.minRow || y >= model.grid.rows) {
                            continue;
                        }
                        
                        // Get current tile
                        int currentTile = model.GetTileAt(globalRoomId, x, y);
                        
                        // Skip if not matching original tile
                        if (currentTile != originalTileId) {
                            continue;
                        }
                        
                        // Check hard limit
                        if (fillChanges.size() >= HARD_LIMIT) {
                            exceededHardLimit = true;
                            break;
                        }
                        
                        // Add this tile to changes
                        PaintTilesCommand::TileChange change;
                        change.roomId = globalRoomId;
                        change.x = x;
                        change.y = y;
                        change.oldTileId = originalTileId;
                        change.newTileId = selectedTileId;
                        fillChanges.push_back(change);
                        
                        // Add neighbors - only cross if no wall or door
                        EdgeSide sides[] = {
                            EdgeSide::East, EdgeSide::West,
                            EdgeSide::South, EdgeSide::North
                        };
                        int dx[] = {1, -1, 0, 0};
                        int dy[] = {0, 0, 1, -1};
                        
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
                        // Hard limit exceeded: refuse and clear
                        if (modals) {
                            modals->pendingFillType = 
                                Modals::PendingFillType::None;
                            modals->pendingFillCellCount = HARD_LIMIT;
                        }
                        // Note: UI will show toast via checking
                        // fillExceededHardLimit flag if needed
                    } else if (fillChanges.size() > SOFT_LIMIT) {
                        // Soft limit exceeded: ask for confirmation
                        hasPendingTileFill = true;
                        pendingTileFillChanges = std::move(fillChanges);
                        if (modals) {
                            modals->showFillConfirmationModal = true;
                            modals->pendingFillType = 
                                Modals::PendingFillType::Tile;
                            modals->pendingFillCellCount = 
                                pendingTileFillChanges.size();
                            modals->fillConfirmed = false;
                        }
                    } else if (!fillChanges.empty()) {
                        // Within limits: apply immediately
                        for (const auto& change : fillChanges) {
                            model.SetTileAt(
                                change.roomId, 
                                change.x, 
                                change.y, 
                                change.newTileId
                            );
                        }
                        
                        // Add to history
                        auto cmd = std::make_unique<FillTilesCommand>(
                            fillChanges
                        );
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
            }
}

}  // namespace Cartograph
