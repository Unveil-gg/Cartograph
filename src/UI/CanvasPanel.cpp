#include "CanvasPanel.h"
#include "../App.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <set>
#include <cstring>

namespace Cartograph {

// ============================================================================
// Helper functions
// ============================================================================

/**
 * Get all tiles along a line from (x0, y0) to (x1, y1).
 * Uses Bresenham's line algorithm to ensure continuous painting.
 */
std::vector<std::pair<int, int>> GetTilesAlongLine(
    int x0, int y0, 
    int x1, int y1
) {
    std::vector<std::pair<int, int>> tiles;
    
    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0;
    int y = y0;
    
    while (true) {
        tiles.push_back({x, y});
        
        if (x == x1 && y == y1) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return tiles;
}

// ============================================================================
// CanvasPanel implementation
// ============================================================================

CanvasPanel::CanvasPanel() {
}

CanvasPanel::~CanvasPanel() {
}

bool CanvasPanel::DetectEdgeHover(
    float mouseX, float mouseY,
    const Canvas& canvas,
    const GridConfig& grid,
    EdgeId* outEdgeId,
    EdgeSide* outEdgeSide
) {
    // Convert mouse to world coordinates
    float worldX, worldY;
    canvas.ScreenToWorld(mouseX, mouseY, &worldX, &worldY);
    
    // Calculate which tile we're in (using floor for proper tile indexing)
    int tx = static_cast<int>(std::floor(worldX / grid.tileWidth));
    int ty = static_cast<int>(std::floor(worldY / grid.tileHeight));
    
    // Calculate position within the tile (0.0 to 1.0)
    float tileWorldX = tx * grid.tileWidth;
    float tileWorldY = ty * grid.tileHeight;
    float relX = (worldX - tileWorldX) / grid.tileWidth;
    float relY = (worldY - tileWorldY) / grid.tileHeight;
    
    // Clamp to [0, 1] (should already be, but just in case)
    relX = std::max(0.0f, std::min(1.0f, relX));
    relY = std::max(0.0f, std::min(1.0f, relY));
    
    // Threshold for edge detection (configurable)
    float threshold = grid.edgeHoverThreshold;
    
    // Find the closest edge
    float distToNorth = relY;
    float distToSouth = 1.0f - relY;
    float distToWest = relX;
    float distToEast = 1.0f - relX;
    
    float minDist = std::min({distToNorth, distToSouth, distToWest, distToEast});
    
    // Only trigger if within threshold
    if (minDist > threshold) {
        return false;
    }
    
    // Return the closest edge
    if (minDist == distToNorth) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::North);
        *outEdgeSide = EdgeSide::North;
        return true;
    } else if (minDist == distToSouth) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::South);
        *outEdgeSide = EdgeSide::South;
        return true;
    } else if (minDist == distToWest) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::West);
        *outEdgeSide = EdgeSide::West;
        return true;
    } else {  // distToEast
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::East);
        *outEdgeSide = EdgeSide::East;
        return true;
    }
}

void CanvasPanel::Render(
    IRenderer& renderer,
    Model& model, 
    Canvas& canvas, 
    History& history,
    IconManager& icons,
    KeymapManager& keymap
) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Cartograph/Canvas", nullptr, flags);
    ImGui::PopStyleVar();
    
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    
    // Reserve space for canvas
    ImGui::InvisibleButton("canvas", canvasSize);
    
    // Accept drag-drop of marker icons
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARKER_ICON")) {
            // Get the icon name from payload
            const char* droppedIconName = (const char*)payload->Data;
            
            // Get mouse position
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert to world coordinates
            float wx, wy;
            canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
            
            // Convert to fractional tile coordinates
            float tileX = wx / model.grid.tileWidth;
            float tileY = wy / model.grid.tileHeight;
            
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
            
            // Place marker at drop location
            Marker newMarker;
            newMarker.id = model.GenerateMarkerId();
            newMarker.roomId = "";
            newMarker.x = tileX;
            newMarker.y = tileY;
            newMarker.kind = "custom";
            newMarker.label = markerLabel;
            newMarker.icon = droppedIconName;
            newMarker.color = markerColor;
            newMarker.size = 0.6f;
            newMarker.showLabel = !markerLabel.empty();
            
            auto cmd = std::make_unique<PlaceMarkerCommand>(newMarker, true);
            history.AddCommand(std::move(cmd), model);
        }
        ImGui::EndDragDropTarget();
    }
    
    // Global keyboard shortcuts for tool switching (work even when not hovering)
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        // Toggle properties panel
        if (keymap.IsActionTriggered("togglePropertiesPanel")) {
            *showPropertiesPanel = !(*showPropertiesPanel);
            *layoutInitialized = false;  // Trigger layout rebuild
        }
        
        // Tool switching shortcuts
        if (keymap.IsActionTriggered("toolMove")) {
            currentTool = Tool::Move;
        }
        if (keymap.IsActionTriggered("toolSelect")) {
            currentTool = Tool::Select;
        }
        if (keymap.IsActionTriggered("toolPaint")) {
            currentTool = Tool::Paint;
        }
        if (keymap.IsActionTriggered("toolErase")) {
            currentTool = Tool::Erase;
        }
        if (keymap.IsActionTriggered("toolFill")) {
            currentTool = Tool::Fill;
        }
        if (keymap.IsActionTriggered("toolEyedropper")) {
            currentTool = Tool::Eyedropper;
        }
    }
    
    // Handle input
    if (ImGui::IsItemHovered()) {
        // Mouse wheel zoom (available in all tools)
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float zoomFactor = (wheel > 0) ? 1.1f : 0.9f;
            canvas.SetZoom(canvas.zoom * zoomFactor);
        }
        
        // Middle mouse button panning (universal shortcut)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            canvas.Pan(-delta.x, -delta.y);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }
        
        // Tool-specific input handling
        if (currentTool == Tool::Move) {
            // Move tool: Left mouse drag to pan
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                canvas.Pan(-delta.x, -delta.y);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
        }
        else if (currentTool == Tool::Select) {
            // Select tool: Left mouse drag to create selection rectangle
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Start selection
                ImVec2 mousePos = ImGui::GetMousePos();
                selectionStartX = mousePos.x;
                selectionStartY = mousePos.y;
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                isSelecting = true;
            }
            
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isSelecting) {
                // Update selection end position
                ImVec2 mousePos = ImGui::GetMousePos();
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
            }
            
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isSelecting) {
                // Finish selection
                ImVec2 mousePos = ImGui::GetMousePos();
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                // Note: Keep selection visible until a different action
            }
        }
        else if (currentTool == Tool::Paint) {
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
                
                // When mouse is released, commit edge changes
                if (isModifyingEdges && 
                    ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
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
        else if (currentTool == Tool::Erase) {
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
        else if (currentTool == Tool::Fill) {
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
                // TODO: Implement flood-fill with wall boundaries
                const std::string globalRoomId = "";
                
                // Get the original tile ID to replace
                int originalTileId = model.GetTileAt(globalRoomId, tx, ty);
                
                // Only fill if we're changing to a different tile
                if (originalTileId != selectedTileId) {
                    // Perform flood fill using BFS
                    std::vector<PaintTilesCommand::TileChange> fillChanges;
                    std::vector<std::pair<int, int>> toVisit;
                    std::set<std::pair<int, int>> visited;
                    
                    toVisit.push_back({tx, ty});
                    
                    while (!toVisit.empty()) {
                        auto [x, y] = toVisit.back();
                        toVisit.pop_back();
                        
                        // Skip if already visited
                        if (visited.count({x, y})) {
                            continue;
                        }
                        visited.insert({x, y});
                        
                        // Check grid bounds
                        if (x < 0 || x >= model.grid.cols ||
                            y < 0 || y >= model.grid.rows) {
                            continue;
                        }
                        
                        // Get current tile
                        int currentTile = model.GetTileAt(globalRoomId, x, y);
                        
                        // Skip if not matching original tile
                        if (currentTile != originalTileId) {
                            continue;
                        }
                        
                        // Add this tile to changes
                        PaintTilesCommand::TileChange change;
                        change.roomId = globalRoomId;
                        change.x = x;
                        change.y = y;
                        change.oldTileId = originalTileId;
                        change.newTileId = selectedTileId;
                        fillChanges.push_back(change);
                        
                        // Add neighbors to visit (4-way connectivity)
                        toVisit.push_back({x + 1, y});
                        toVisit.push_back({x - 1, y});
                        toVisit.push_back({x, y + 1});
                        toVisit.push_back({x, y - 1});
                    }
                    
                    // Apply all changes and add to history
                    if (!fillChanges.empty()) {
                        // Apply changes immediately
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
        else if (currentTool == Tool::Eyedropper) {
            // Eyedropper tool: Left-click to pick tile color/ID
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Pick tile globally (using "" as roomId)
                const std::string globalRoomId = "";
                int pickedTileId = model.GetTileAt(globalRoomId, tx, ty);
                
                // Only pick non-empty tiles
                if (pickedTileId != 0) {
                    selectedTileId = pickedTileId;
                    
                    // Find color name for toast message
                    std::string colorName = "Unknown";
                    for (const auto& tile : model.palette) {
                        if (tile.id == pickedTileId) {
                            colorName = tile.name;
                            break;
                        }
                    }
                    
                    // TODO: Re-enable toast: ShowToast("Picked: " + colorName, Toast::Type::Success, 1.5f);
                    
                    // Auto-switch to Paint tool if toggle is enabled
                    if (eyedropperAutoSwitchToPaint) {
                        currentTool = Tool::Paint;
                    }
                }
                // Note: "No tile to pick" message shown in console only
            }
        }
        else if (currentTool == Tool::Marker) {
            // Marker tool: Left-click to place/edit markers
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert to world coordinates
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                // Convert to fractional tile coordinates
                float tileX = wx / model.grid.tileWidth;
                float tileY = wy / model.grid.tileHeight;
                
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
                    
                    selectedMarker = nullptr;  // Clear selection
                } else if (clickedMarker) {
                    // Click existing marker: Select and start drag
                    selectedMarker = clickedMarker;
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
                
                // Convert to fractional tile coordinates
                float tileX = wx / model.grid.tileWidth;
                float tileY = wy / model.grid.tileHeight;
                
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
                    if (selectedMarker && selectedMarker->id == clickedMarker->id) {
                        selectedMarker = nullptr;
                    }
                }
            }
            
            // Handle marker dragging
            if (isDraggingMarker && selectedMarker) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    // Update marker position while dragging
                    ImVec2 mousePos = ImGui::GetMousePos();
                    
                    float wx, wy;
                    canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                    
                    float tileX = wx / model.grid.tileWidth;
                    float tileY = wy / model.grid.tileHeight;
                    
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
        else if (currentTool == Tool::RoomPaint) {
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
        else if (currentTool == Tool::RoomErase) {
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
                    
                    // Erase all cells along the line
                    for (const auto& cell : cellsToErase) {
                        int cellX = cell.first;
                        int cellY = cell.second;
                        
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
        else if (currentTool == Tool::RoomFill) {
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
                
                // Check bounds
                if (tx >= 0 && tx < model.grid.cols && 
                    ty >= 0 && ty < model.grid.rows) {
                    
                    std::string startRoomId = model.GetCellRoom(tx, ty);
                    
                    // Only fill if starting from empty cell or cell in a different room
                    if (startRoomId != activeRoomId) {
                        // Perform flood fill using BFS
                        std::vector<ModifyRoomAssignmentsCommand::CellAssignment> 
                            fillAssignments;
                        std::vector<std::pair<int, int>> toVisit;
                        std::set<std::pair<int, int>> visited;
                        
                        toVisit.push_back({tx, ty});
                        
                        const size_t MAX_FILL_SIZE = 10000;  // Safety limit
                        
                        while (!toVisit.empty() && visited.size() < MAX_FILL_SIZE) {
                            auto [x, y] = toVisit.back();
                            toVisit.pop_back();
                            
                            // Skip if already visited
                            if (visited.count({x, y})) {
                                continue;
                            }
                            visited.insert({x, y});
                            
                            // Check grid bounds
                            if (x < 0 || x >= model.grid.cols ||
                                y < 0 || y >= model.grid.rows) {
                                continue;
                            }
                            
                            // Get current room
                            std::string currentRoom = model.GetCellRoom(x, y);
                            
                            // Skip if not matching start condition
                            if (currentRoom != startRoomId) {
                                continue;
                            }
                            
                            // Add this cell to fill
                            ModifyRoomAssignmentsCommand::CellAssignment assignment;
                            assignment.x = x;
                            assignment.y = y;
                            assignment.oldRoomId = startRoomId;
                            assignment.newRoomId = activeRoomId;
                            fillAssignments.push_back(assignment);
                            
                            // Apply immediately for visual feedback
                            model.SetCellRoom(x, y, activeRoomId);
                            
                            // Check all 4 neighbors - only cross if no wall
                            EdgeSide sides[] = {
                                EdgeSide::North, EdgeSide::South,
                                EdgeSide::East, EdgeSide::West
                            };
                            int dx[] = {0, 0, 1, -1};
                            int dy[] = {-1, 1, 0, 0};
                            
                            for (int i = 0; i < 4; ++i) {
                                EdgeId edgeId = MakeEdgeId(x, y, sides[i]);
                                EdgeState edgeState = model.GetEdgeState(edgeId);
                                
                                // Can only cross if edge is None or Door
                                // Wall blocks fill
                                if (edgeState == EdgeState::None || 
                                    edgeState == EdgeState::Door) {
                                    int nx = x + dx[i];
                                    int ny = y + dy[i];
                                    
                                    if (!visited.count({nx, ny})) {
                                        toVisit.push_back({nx, ny});
                                    }
                                }
                            }
                        }
                        
                        // Create command if we filled anything
                        if (!fillAssignments.empty()) {
                            auto cmd = std::make_unique<ModifyRoomAssignmentsCommand>(
                                fillAssignments
                            );
                            history.AddCommand(std::move(cmd), model, false);
                            
                            // Generate perimeter walls if enabled
                            if (model.autoGenerateRoomWalls && !activeRoomId.empty()) {
                                model.GenerateRoomPerimeterWalls(activeRoomId);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Handle mouse release for Paint/Erase tools (outside hover check)
    // This ensures releases are detected even if mouse drifts outside canvas
    if (currentTool == Tool::Paint || currentTool == Tool::Erase) {
        // Check for paint/erase tile release
        bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                             ImGui::IsMouseReleased(ImGuiMouseButton_Right);
        
        if (isPainting && mouseReleased) {
            if (!currentPaintChanges.empty()) {
                auto cmd = std::make_unique<PaintTilesCommand>(
                    currentPaintChanges
                );
                // Changes already applied, just store for undo/redo
                history.AddCommand(std::move(cmd), model, false);
                currentPaintChanges.clear();
            }
            isPainting = false;
            lastPaintedTileX = -1;
            lastPaintedTileY = -1;
            twoFingerEraseActive = false;
        }
        
        // Check for edge modification release (Erase tool)
        if (isModifyingEdges && mouseReleased) {
            if (!currentEdgeChanges.empty()) {
                auto cmd = std::make_unique<ModifyEdgesCommand>(
                    currentEdgeChanges
                );
                history.AddCommand(std::move(cmd), model, false);
                currentEdgeChanges.clear();
            }
            isModifyingEdges = false;
        }
    }
    
    // Clear selection if we click outside canvas
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
        !ImGui::IsItemHovered() && isSelecting) {
        isSelecting = false;
    }
    
    // Keyboard shortcuts for markers
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // Copy selected marker
        if (keymap.IsActionTriggered("copy")) {
            if (selectedMarker) {
                copiedMarkers.clear();
                copiedMarkers.push_back(*selectedMarker);
            }
        }
        
        // Paste marker
        if (keymap.IsActionTriggered("paste")) {
            if (!copiedMarkers.empty()) {
                // Paste at mouse position or offset from original
                ImVec2 mousePos = ImGui::GetMousePos();
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                float tileX = wx / model.grid.tileWidth;
                float tileY = wy / model.grid.tileHeight;
                
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
        if (selectedMarker && 
            (keymap.IsActionTriggered("delete") || 
             keymap.IsActionTriggered("deleteAlt"))) {
            auto cmd = std::make_unique<DeleteMarkerCommand>(
                selectedMarker->id
            );
            history.AddCommand(std::move(cmd), model);   
            selectedMarker = nullptr;
        }
    }
    
    // Draw canvas overlays using window DrawList
    // (renders above canvas texture but below UI elements like tooltips/menus)
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Clip all canvas drawing to window bounds (prevent overlap with other panels)
    ImVec2 canvasMin = ImGui::GetWindowPos();
    ImVec2 canvasMax = ImVec2(canvasMin.x + ImGui::GetWindowSize().x,
                               canvasMin.y + ImGui::GetWindowSize().y);
    drawList->PushClipRect(canvasMin, canvasMax, true);
    
    // Draw background
    ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    drawList->AddRectFilled(canvasPos, 
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
        bgColor);
    
    // Update hovered tile coordinates (for status bar)
    if (ImGui::IsItemHovered()) {
        isHoveringCanvas = true;
        ImVec2 mousePos = ImGui::GetMousePos();
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &hoveredTileX, &hoveredTileY
        );
    } else {
        isHoveringCanvas = false;
        hoveredTileX = -1;
        hoveredTileY = -1;
    }
    
    // Update hovered marker (if Marker tool is active)
    if (currentTool == Tool::Marker && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float wx, wy;
        canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
        
        float tileX = wx / model.grid.tileWidth;
        float tileY = wy / model.grid.tileHeight;
        
        hoveredMarker = model.FindMarkerNear(tileX, tileY, 0.5f);
        
        // Show tooltip for hovered marker
        if (hoveredMarker && !isDraggingMarker) {
            ImGui::BeginTooltip();
            ImGui::Text("Marker: %s", hoveredMarker->label.empty() ? 
                       "(no label)" : hoveredMarker->label.c_str());
            ImGui::TextDisabled("Icon: %s", hoveredMarker->icon.c_str());
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               hoveredMarker->x, hoveredMarker->y);
            ImGui::Separator();
            ImGui::TextDisabled("Click: Select/Move");
            ImGui::TextDisabled("Shift+Click: Delete");
            ImGui::EndTooltip();
        }
    } else {
        hoveredMarker = nullptr;
    }
    
    // Render the actual canvas content (grid, tiles, rooms, doors, markers, overlays)
    canvas.Render(
        renderer,
        model,
        &icons,
        static_cast<int>(canvasPos.x),
        static_cast<int>(canvasPos.y),
        static_cast<int>(canvasSize.x),
        static_cast<int>(canvasSize.y),
        isHoveringEdge ? &hoveredEdge : nullptr,
        showRoomOverlays,  // Pass room overlay toggle state
        selectedMarker,    // Pass selected marker for highlight
        hoveredMarker      // Pass hovered marker for highlight
    );
    
    // Note: Thumbnail capture moved to App::Render() after ImGui draw data
    // is rendered to ensure pixels are actually in the framebuffer
    
    // Draw selection rectangle if Select tool is active
    if (currentTool == Tool::Select && isSelecting) {
        // Calculate rectangle bounds
        float minX = std::min(selectionStartX, selectionEndX);
        float minY = std::min(selectionStartY, selectionEndY);
        float maxX = std::max(selectionStartX, selectionEndX);
        float maxY = std::max(selectionStartY, selectionEndY);
        
        // Draw semi-transparent fill
        ImU32 fillColor = ImGui::GetColorU32(ImVec4(0.3f, 0.6f, 1.0f, 0.2f));
        drawList->AddRectFilled(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            fillColor
        );
        
        // Draw border
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.3f, 0.6f, 1.0f, 0.8f));
        drawList->AddRect(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            borderColor,
            0.0f,
            0,
            2.0f
        );
    }
    
    // Draw paint cursor preview if Paint or Erase tool is active
    if ((currentTool == Tool::Paint || currentTool == Tool::Erase) && 
        ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Convert back to screen coordinates (snapped to grid)
        float wx, wy;
        canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                          model.grid.tileHeight, &wx, &wy);
        
        float sx, sy;
        canvas.WorldToScreen(wx, wy, &sx, &sy);
        
        float sw = model.grid.tileWidth * canvas.zoom;
        float sh = model.grid.tileHeight * canvas.zoom;
        
        // Draw preview based on tool and state
        // TODO: Make these colors customizable in theme/settings
        if (currentTool == Tool::Erase || 
            (currentTool == Tool::Paint && ImGui::IsKeyDown(ImGuiKey_E))) {
            // Erase preview (red fill + red outline)
            ImU32 eraseColor = ImGui::GetColorU32(
                ImVec4(1.0f, 0.3f, 0.3f, 0.6f)
            );
            
            // Calculate brush area for multi-tile eraser
            int halfBrush = eraserBrushSize / 2;
            int startTileX = tx - halfBrush;
            int startTileY = ty - halfBrush;
            int endTileX = tx + halfBrush;
            int endTileY = ty + halfBrush;
            
            // Convert start tile to screen coords
            float startWx, startWy;
            canvas.TileToWorld(startTileX, startTileY, 
                             model.grid.tileWidth, model.grid.tileHeight,
                             &startWx, &startWy);
            float startSx, startSy;
            canvas.WorldToScreen(startWx, startWy, &startSx, &startSy);
            
            // Calculate total size of brush area in screen space
            float totalWidth = sw * eraserBrushSize;
            float totalHeight = sh * eraserBrushSize;
            
            // Draw filled semi-transparent rectangle for brush area
            ImU32 eraseFillColor = ImGui::GetColorU32(
                ImVec4(1.0f, 0.3f, 0.3f, 0.3f)
            );
            drawList->AddRectFilled(
                ImVec2(startSx, startSy),
                ImVec2(startSx + totalWidth, startSy + totalHeight),
                eraseFillColor
            );
            
            // Draw outline
            drawList->AddRect(
                ImVec2(startSx, startSy),
                ImVec2(startSx + totalWidth, startSy + totalHeight),
                eraseColor,
                0.0f,
                0,
                2.0f
            );
        } else {
            // Paint preview (brightened tile color + white border)
            Color tileColor(0.8f, 0.8f, 0.8f, 0.4f);
            for (const auto& tile : model.palette) {
                if (tile.id == selectedTileId) {
                    tileColor = tile.color;
                    break;
                }
            }
            
            // Brighten color by 30% for visibility
            // TODO: Make brightness boost customizable
            float brightenAmount = 0.3f;
            Color brightened;
            brightened.r = std::min(tileColor.r + brightenAmount, 1.0f);
            brightened.g = std::min(tileColor.g + brightenAmount, 1.0f);
            brightened.b = std::min(tileColor.b + brightenAmount, 1.0f);
            brightened.a = 0.6f;  // Semi-transparent
            
            // Draw preview fill
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                brightened.ToU32()
            );
            
            // Draw white border for visibility
            // TODO: Make border color/thickness customizable
            ImU32 borderColor = ImGui::GetColorU32(
                ImVec4(1.0f, 1.0f, 1.0f, 0.9f)
            );
            float borderThickness = 3.0f;
            drawList->AddRect(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                borderColor,
                0.0f,
                0,
                borderThickness
            );
        }
        
        // Draw edge hover preview for Paint tool
        if (currentTool == Tool::Paint && isHoveringEdge) {
            // Calculate edge line endpoints based on hovered edge
            int x1 = hoveredEdge.x1;
            int y1 = hoveredEdge.y1;
            int x2 = hoveredEdge.x2;
            int y2 = hoveredEdge.y2;
            
            // Determine if this is a vertical or horizontal edge
            bool isVertical = (x1 != x2);
            
            float wx1, wy1, wx2, wy2;
            if (isVertical) {
                // Vertical edge
                wx1 = std::max(x1, x2) * model.grid.tileWidth;
                wx2 = wx1;
                wy1 = std::min(y1, y2) * model.grid.tileHeight;
                wy2 = wy1 + model.grid.tileHeight;
            } else {
                // Horizontal edge
                wy1 = std::max(y1, y2) * model.grid.tileHeight;
                wy2 = wy1;
                wx1 = std::min(x1, x2) * model.grid.tileWidth;
                wx2 = wx1 + model.grid.tileWidth;
            }
            
            // Convert to screen coordinates
            float esx1, esy1, esx2, esy2;
            canvas.WorldToScreen(wx1, wy1, &esx1, &esy1);
            canvas.WorldToScreen(wx2, wy2, &esx2, &esy2);
            
            // Get current edge state
            EdgeState currentState = model.GetEdgeState(hoveredEdge);
            
            // Draw ghost preview showing what will happen
            ImU32 edgePreviewColor;
            if (currentState == EdgeState::None) {
                // No edge exists - show wall preview (green)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(0.3f, 1.0f, 0.3f, 0.7f)
                );
            } else if (currentState == EdgeState::Wall) {
                // Wall exists - show door preview (blue)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(0.3f, 0.6f, 1.0f, 0.7f)
                );
            } else {
                // Door exists - show none/delete preview (red)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(1.0f, 0.3f, 0.3f, 0.7f)
                );
            }
            
            // Draw preview line (thicker to be visible)
            drawList->AddLine(
                ImVec2(esx1, esy1),
                ImVec2(esx2, esy2),
                edgePreviewColor,
                4.0f * canvas.zoom
            );
        }
    }
    
    // Draw fill cursor preview if Fill tool is active
    if (currentTool == Tool::Fill && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Convert back to screen coordinates (snapped to grid)
        float wx, wy;
        canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                          model.grid.tileHeight, &wx, &wy);
        
        float sx, sy;
        canvas.WorldToScreen(wx, wy, &sx, &sy);
        
        float sw = model.grid.tileWidth * canvas.zoom;
        float sh = model.grid.tileHeight * canvas.zoom;
        
        // Get selected tile color for preview
        Color tileColor(0.8f, 0.8f, 0.8f, 0.6f);
        for (const auto& tile : model.palette) {
            if (tile.id == selectedTileId) {
                tileColor = tile.color;
                tileColor.a = 0.6f;  // More opaque for fill preview
                break;
            }
        }
        
        // Draw bucket icon indicator (center cross)
        float centerX = sx + sw / 2.0f;
        float centerY = sy + sh / 2.0f;
        float crossSize = std::min(sw, sh) * 0.3f;
        ImU32 crossColor = ImGui::GetColorU32(
            ImVec4(1.0f, 1.0f, 1.0f, 0.8f)
        );
        
        // Vertical line of cross
        drawList->AddLine(
            ImVec2(centerX, centerY - crossSize),
            ImVec2(centerX, centerY + crossSize),
            crossColor,
            2.0f
        );
        
        // Horizontal line of cross
        drawList->AddLine(
            ImVec2(centerX - crossSize, centerY),
            ImVec2(centerX + crossSize, centerY),
            crossColor,
            2.0f
        );
        
        // Draw semi-transparent fill preview
        ImU32 previewColor = tileColor.ToU32();
        drawList->AddRectFilled(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            previewColor
        );
        
        // Draw border
        drawList->AddRect(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.6f)),
            0.0f,
            0,
            2.0f
        );
    }
    
    // Draw eyedropper hover highlight if Eyedropper tool is active
    if (currentTool == Tool::Eyedropper && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Check if there's a tile at this position
        const std::string globalRoomId = "";
        int hoveredTileId = model.GetTileAt(globalRoomId, tx, ty);
        
        // Only show highlight if there's a non-empty tile
        if (hoveredTileId != 0) {
            // Convert back to screen coordinates (snapped to grid)
            float wx, wy;
            canvas.TileToWorld(
                tx, ty, model.grid.tileWidth, 
                model.grid.tileHeight, &wx, &wy
            );
            
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            float sw = model.grid.tileWidth * canvas.zoom;
            float sh = model.grid.tileHeight * canvas.zoom;
            
            // Find the tile color for the highlight
            Color tileColor(0.8f, 0.8f, 0.8f, 0.3f);
            for (const auto& tile : model.palette) {
                if (tile.id == hoveredTileId) {
                    tileColor = tile.color;
                    tileColor.a = 0.3f;  // Semi-transparent overlay
                    break;
                }
            }
            
            // Draw semi-transparent color overlay
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                tileColor.ToU32()
            );
            
            // Draw bright cyan border to indicate hovering
            ImU32 borderColor = ImGui::GetColorU32(
                ImVec4(0.0f, 0.8f, 1.0f, 1.0f)
            );
            drawList->AddRect(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                borderColor,
                0.0f,
                0,
                3.0f  // Thick border for visibility
            );
        }
    }
    
    // Draw marker snap point preview if Marker tool is active
    if (currentTool == Tool::Marker && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to world coordinates
        float wx, wy;
        canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
        
        // Convert to fractional tile coordinates
        float tileX = wx / model.grid.tileWidth;
        float tileY = wy / model.grid.tileHeight;
        
        // Get base tile
        int baseTileX = static_cast<int>(std::floor(tileX));
        int baseTileY = static_cast<int>(std::floor(tileY));
        float fractionalX = tileX - baseTileX;
        float fractionalY = tileY - baseTileY;
        
        // Get snap points for current preset
        auto snapPoints = model.GetMarkerSnapPoints();
        
        // Find nearest snap point
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
        
        // Calculate final snapped position
        float snappedTileX = baseTileX + bestSnapX;
        float snappedTileY = baseTileY + bestSnapY;
        
        // Convert to world coordinates
        float snappedWx = snappedTileX * model.grid.tileWidth;
        float snappedWy = snappedTileY * model.grid.tileHeight;
        
        // Convert to screen coordinates
        float snappedSx, snappedSy;
        canvas.WorldToScreen(snappedWx, snappedWy, &snappedSx, &snappedSy);
        
        // Draw all snap points for current tile (subtle indicators)
        for (const auto& snap : snapPoints) {
            float snapWx = (baseTileX + snap.first) * model.grid.tileWidth;
            float snapWy = (baseTileY + snap.second) * model.grid.tileHeight;
            
            float snapSx, snapSy;
            canvas.WorldToScreen(snapWx, snapWy, &snapSx, &snapSy);
            
            // Draw small dot at snap point
            float dotRadius = 3.0f;
            ImU32 dotColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.4f));
            drawList->AddCircleFilled(
                ImVec2(snapSx, snapSy),
                dotRadius,
                dotColor,
                8
            );
        }
        
        // Draw ghost marker at snapped position (larger, highlighted)
        float minDim = static_cast<float>(std::min(model.grid.tileWidth, model.grid.tileHeight));
        float markerSize = minDim * canvas.zoom * 0.6f;
        
        // Draw ghost marker with pulsing effect
        ImU32 ghostColor = ImGui::GetColorU32(ImVec4(
            markerColor.r, 
            markerColor.g, 
            markerColor.b, 
            0.5f  // Semi-transparent
        ));
        
        drawList->AddCircleFilled(
            ImVec2(snappedSx, snappedSy),
            markerSize / 2.0f,
            ghostColor,
            16
        );
        
        // Draw border around ghost marker
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
        drawList->AddCircle(
            ImVec2(snappedSx, snappedSy),
            markerSize / 2.0f,
            borderColor,
            16,
            2.0f
        );
        
        // Draw crosshair at snap point for precision
        float crossSize = 8.0f;
        ImU32 crossColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.8f));
        
        drawList->AddLine(
            ImVec2(snappedSx - crossSize, snappedSy),
            ImVec2(snappedSx + crossSize, snappedSy),
            crossColor,
            1.5f
        );
        drawList->AddLine(
            ImVec2(snappedSx, snappedSy - crossSize),
            ImVec2(snappedSx, snappedSy + crossSize),
            crossColor,
            1.5f
        );
    }
    
    // Draw drag-drop preview when dragging icon over canvas
    if (ImGui::IsDragDropActive() && ImGui::IsItemHovered()) {
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        if (payload && payload->IsDataType("MARKER_ICON")) {
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert to world coordinates
            float wx, wy;
            canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
            
            // Convert to fractional tile coordinates
            float tileX = wx / model.grid.tileWidth;
            float tileY = wy / model.grid.tileHeight;
            
            // Snap to nearest snap point
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
            
            float snappedTileX = baseTileX + bestSnapX;
            float snappedTileY = baseTileY + bestSnapY;
            
            // Convert to screen coordinates
            float snappedWx = snappedTileX * model.grid.tileWidth;
            float snappedWy = snappedTileY * model.grid.tileHeight;
            float snappedSx, snappedSy;
            canvas.WorldToScreen(snappedWx, snappedWy, &snappedSx, &snappedSy);
            
            // Draw ghost marker preview
            float minDim = static_cast<float>(std::min(model.grid.tileWidth, model.grid.tileHeight));
            float markerSize = minDim * canvas.zoom * 0.6f;
            
            ImU32 ghostColor = ImGui::GetColorU32(ImVec4(
                markerColor.r, markerColor.g, markerColor.b, 0.5f
            ));
            
            drawList->AddCircleFilled(
                ImVec2(snappedSx, snappedSy),
                markerSize / 2.0f,
                ghostColor,
                16
            );
            
            // Draw border
            ImU32 borderColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
            drawList->AddCircle(
                ImVec2(snappedSx, snappedSy),
                markerSize / 2.0f,
                borderColor,
                16,
                2.0f
            );
            
            // Draw "drop here" text
            const char* dropText = "Drop to place marker";
            ImVec2 textSize = ImGui::CalcTextSize(dropText);
            ImVec2 textPos(snappedSx - textSize.x / 2.0f, 
                          snappedSy + markerSize / 2.0f + 8.0f);
            
            // Text background
            drawList->AddRectFilled(
                ImVec2(textPos.x - 4, textPos.y - 2),
                ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
                ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f))
            );
            
            drawList->AddText(textPos, 
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 
                dropText);
        }
    }
    
    // Pop clip rect before ending window
    drawList->PopClipRect();
    
    ImGui::End();
}

} // namespace Cartograph
