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

void CanvasPanel::HandleEraseInput(
    Model& model,
    Canvas& canvas
) {
            // Erase tool: Left mouse to erase (primary input)
            // - Hover over edge: highlight and erase on click
            // - Otherwise: erase tiles
            // Right-click also supported for consistency
            
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // First, check if we're hovering near an edge
            EdgeId edgeId;
            EdgeSide edgeSide;
            isHoveringEdge = DetectEdgeHover(
                mousePos.x, mousePos.y, canvas, model.grid,
                &edgeId, &edgeSide
            );
            
            if (isHoveringEdge) {
                hoveredEdge = edgeId;
                
                // Handle edge deletion via hover (precise mode)
                bool shouldEraseEdge = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                                      ImGui::IsMouseDown(ImGuiMouseButton_Right);
                
                if (shouldEraseEdge) {
                    EdgeState currentState = model.GetEdgeState(edgeId);
                    
                    // Only delete if there's an edge to delete
                    if (currentState != EdgeState::None) {
                        ModifyEdgesCommand::EdgeChange change;
                        change.edgeId = edgeId;
                        change.oldState = currentState;
                        change.newState = EdgeState::None;  // Delete edge
                        
                        currentEdgeChanges.push_back(change);
                        
                        // Apply immediately for visual feedback
                        model.SetEdgeState(edgeId, EdgeState::None);
                        
                        isModifyingEdges = true;
                    }
                }
            }
            // Handle tile erasing (only when NOT hovering edge)
            else {
            bool shouldErase = false;
            
            // Check for left mouse button (primary erase input)
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                shouldErase = true;
            }
            // Also support right-click for two-finger trackpad gestures
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                shouldErase = true;
            }
            
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
                            lastPaintedTileX, lastPaintedTileY, tx, ty
                        );
                    } else {
                        // First tile
                        tilesToErase.push_back({tx, ty});
                    }
                    
                    // Track previous tile for edge crossing detection
                    int prevX = lastPaintedTileX;
                    int prevY = lastPaintedTileY;
                    
                    // Erase all tiles along the line with brush size
                    for (const auto& tile : tilesToErase) {
                        int centerX = tile.first;
                        int centerY = tile.second;
                        
                        // Calculate brush area (NxN tiles centered on cursor)
                        int halfBrush = eraserBrushSize / 2;
                        int startX = centerX - halfBrush;
                        int startY = centerY - halfBrush;
                        int endX = centerX + halfBrush;
                        int endY = centerY + halfBrush;
                        
                        // Erase all tiles in brush area
                        for (int brushY = startY; brushY <= endY; brushY++) {
                            for (int brushX = startX; brushX <= endX; brushX++) {
                                int oldTileId = model.GetTileAt(
                                    globalRoomId, brushX, brushY
                                );
                                
                                // Only erase if there's something to erase
                                if (oldTileId != 0) {
                                    PaintTilesCommand::TileChange change;
                                    change.roomId = globalRoomId;
                                    change.x = brushX;
                                    change.y = brushY;
                                    change.oldTileId = oldTileId;
                                    change.newTileId = 0;  // 0 = empty/erase
                                    
                                    currentPaintChanges.push_back(change);
                                    
                                    // Apply immediately for visual feedback
                                    model.SetTileAt(globalRoomId, brushX, 
                                                   brushY, 0);
                                }
                            }
                        }
                        
                        // Check for crossed edges when dragging
                        if (prevX >= 0 && prevY >= 0) {
                            // Moved horizontally - crossed vertical edge
                            if (centerX != prevX) {
                                EdgeSide side = (centerX > prevX) ? 
                                    EdgeSide::East : EdgeSide::West;
                                EdgeId crossedEdge = MakeEdgeId(
                                    prevX, prevY, side
                                );
                                EdgeState edgeState = model.GetEdgeState(
                                    crossedEdge
                                );
                                
                                if (edgeState != EdgeState::None) {
                                    ModifyEdgesCommand::EdgeChange change;
                                    change.edgeId = crossedEdge;
                                    change.oldState = edgeState;
                                    change.newState = EdgeState::None;
                                    
                                    currentEdgeChanges.push_back(change);
                                    model.SetEdgeState(
                                        crossedEdge, EdgeState::None
                                    );
                                }
                            }
                            
                            // Moved vertically - crossed horizontal edge
                            if (centerY != prevY) {
                                EdgeSide side = (centerY > prevY) ? 
                                    EdgeSide::South : EdgeSide::North;
                                EdgeId crossedEdge = MakeEdgeId(
                                    prevX, prevY, side
                                );
                                EdgeState edgeState = model.GetEdgeState(
                                    crossedEdge
                                );
                                
                                if (edgeState != EdgeState::None) {
                                    ModifyEdgesCommand::EdgeChange change;
                                    change.edgeId = crossedEdge;
                                    change.oldState = edgeState;
                                    change.newState = EdgeState::None;
                                    
                                    currentEdgeChanges.push_back(change);
                                    model.SetEdgeState(
                                        crossedEdge, EdgeState::None
                                    );
                                }
                            }
                        }
                        
                        // Update prev for next iteration
                        prevX = centerX;
                        prevY = centerY;
                    }
                    
                    lastPaintedTileX = tx;
                    lastPaintedTileY = ty;
                    isPainting = true;
                }
            }
            }  // End of else (tile erasing when not hovering edge)
}

}  // namespace Cartograph
