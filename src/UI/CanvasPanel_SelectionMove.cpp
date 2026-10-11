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

// ============================================================================
// Helper functions
// ============================================================================

/**
 * Get all tiles along a line from (x0, y0) to (x1, y1).
 * Uses Bresenham's line algorithm to ensure continuous painting.
 */
void CanvasPanel::PopulateSelectionFromRect(
    const Model& model,
    const Canvas& canvas
) {
    // Clear previous selection
    currentSelection.Clear();
    
    // Convert screen coordinates to tile coordinates
    float minScreenX = std::min(selectionStartX, selectionEndX);
    float minScreenY = std::min(selectionStartY, selectionEndY);
    float maxScreenX = std::max(selectionStartX, selectionEndX);
    float maxScreenY = std::max(selectionStartY, selectionEndY);
    
    // Convert corners to tile coordinates
    int tx1, ty1, tx2, ty2;
    canvas.ScreenToTile(
        minScreenX, minScreenY,
        model.grid.tileWidth, model.grid.tileHeight,
        &tx1, &ty1
    );
    canvas.ScreenToTile(
        maxScreenX, maxScreenY,
        model.grid.tileWidth, model.grid.tileHeight,
        &tx2, &ty2
    );
    
    // With Y-up coords, Y values may be inverted, so use min/max
    int minTileX = std::min(tx1, tx2);
    int maxTileX = std::max(tx1, tx2);
    int minTileY = std::min(ty1, ty2);
    int maxTileY = std::max(ty1, ty2);
    
    // Store bounding box
    currentSelection.bounds.x = minTileX;
    currentSelection.bounds.y = minTileY;
    currentSelection.bounds.w = maxTileX - minTileX + 1;
    currentSelection.bounds.h = maxTileY - minTileY + 1;
    
    // Global room ID for tile storage
    const std::string globalRoomId = "";
    
    // Collect tiles if enabled
    if (selectTiles) {
        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                int tileId = model.GetTileAt(globalRoomId, tx, ty);
                if (tileId != 0) {
                    currentSelection.tiles[{tx, ty}] = tileId;
                }
            }
        }
    }
    
    // Collect edges if enabled
    if (selectEdges) {
        // Iterate through all tiles in selection and check their edges
        for (int ty = minTileY; ty <= maxTileY + 1; ++ty) {
            for (int tx = minTileX; tx <= maxTileX + 1; ++tx) {
                // Check horizontal edge (between ty-1 and ty)
                if (ty > minTileY) {
                    EdgeId hEdge = MakeEdgeId(tx, ty - 1, EdgeSide::South);
                    EdgeState state = model.GetEdgeState(hEdge);
                    if (state != EdgeState::None) {
                        currentSelection.edges[hEdge] = state;
                    }
                }
                
                // Check vertical edge (between tx-1 and tx)
                if (tx > minTileX) {
                    EdgeId vEdge = MakeEdgeId(tx - 1, ty, EdgeSide::East);
                    EdgeState state = model.GetEdgeState(vEdge);
                    if (state != EdgeState::None) {
                        currentSelection.edges[vEdge] = state;
                    }
                }
            }
        }
    }
    
    // Collect markers if enabled
    if (selectMarkers) {
        for (const auto& marker : model.markers) {
            // Check if marker center is within selection bounds
            // Marker coordinates are in tile space (can be fractional)
            if (marker.x >= minTileX && marker.x <= maxTileX + 1 &&
                marker.y >= minTileY && marker.y <= maxTileY + 1) {
                currentSelection.markerIds.push_back(marker.id);
            }
        }
    }
    
    // Mark selection as valid if we found anything
    hasSelection = !currentSelection.IsEmpty();
}

void CanvasPanel::DeleteSelection(Model& model, History& history) {
    if (!hasSelection || currentSelection.IsEmpty()) return;
    
    const std::string globalRoomId = "";
    
    // Build compound command for all deletions
    // Delete tiles (set to empty)
    std::vector<PaintTilesCommand::TileChange> tileChanges;
    for (const auto& tilePair : currentSelection.tiles) {
        PaintTilesCommand::TileChange change;
        change.roomId = globalRoomId;
        change.x = tilePair.first.first;
        change.y = tilePair.first.second;
        change.oldTileId = tilePair.second;
        change.newTileId = 0;  // Empty
        tileChanges.push_back(change);
        
        // Apply immediately
        model.SetTileAt(globalRoomId, change.x, change.y, 0);
    }
    
    if (!tileChanges.empty()) {
        auto cmd = std::make_unique<PaintTilesCommand>(tileChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Delete edges (set to None)
    std::vector<ModifyEdgesCommand::EdgeChange> edgeChanges;
    for (const auto& edgePair : currentSelection.edges) {
        ModifyEdgesCommand::EdgeChange change;
        change.edgeId = edgePair.first;
        change.oldState = edgePair.second;
        change.newState = EdgeState::None;
        edgeChanges.push_back(change);
        
        // Apply immediately
        model.SetEdgeState(edgePair.first, EdgeState::None);
    }
    
    if (!edgeChanges.empty()) {
        auto cmd = std::make_unique<ModifyEdgesCommand>(edgeChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Delete markers
    for (const auto& markerId : currentSelection.markerIds) {
        auto cmd = std::make_unique<DeleteMarkerCommand>(markerId);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Clear selection after delete
    ClearSelection();
    model.MarkDirty();
}

void CanvasPanel::CopySelection(const Model& model) {
    if (!hasSelection || currentSelection.IsEmpty()) return;
    
    clipboard.Clear();
    
    // Store origin (top-left of selection)
    int originX = currentSelection.bounds.x;
    int originY = currentSelection.bounds.y;
    
    // Copy tiles with relative positions
    for (const auto& tilePair : currentSelection.tiles) {
        int dx = tilePair.first.first - originX;
        int dy = tilePair.first.second - originY;
        clipboard.tiles[{dx, dy}] = tilePair.second;
    }
    
    // Copy edges with relative positions
    for (const auto& edgePair : currentSelection.edges) {
        ClipboardData::RelativeEdge relEdge;
        relEdge.dx1 = edgePair.first.x1 - originX;
        relEdge.dy1 = edgePair.first.y1 - originY;
        relEdge.dx2 = edgePair.first.x2 - originX;
        relEdge.dy2 = edgePair.first.y2 - originY;
        relEdge.state = edgePair.second;
        clipboard.edges.push_back(relEdge);
    }
    
    // Copy markers with relative positions
    for (const auto& markerId : currentSelection.markerIds) {
        const Marker* marker = model.FindMarker(markerId);
        if (marker) {
            ClipboardData::RelativeMarker relMarker;
            relMarker.dx = marker->x - originX;
            relMarker.dy = marker->y - originY;
            relMarker.kind = marker->kind;
            relMarker.label = marker->label;
            relMarker.icon = marker->icon;
            relMarker.color = marker->color;
            relMarker.size = marker->size;
            relMarker.showLabel = marker->showLabel;
            clipboard.markers.push_back(relMarker);
        }
    }
    
    // Store dimensions for preview
    clipboard.width = currentSelection.bounds.w;
    clipboard.height = currentSelection.bounds.h;
}

void CanvasPanel::PasteClipboard(
    Model& model, 
    History& history,
    int targetX, 
    int targetY
) {
    if (clipboard.IsEmpty()) return;
    
    const std::string globalRoomId = "";
    
    // Paste tiles
    std::vector<PaintTilesCommand::TileChange> tileChanges;
    for (const auto& tilePair : clipboard.tiles) {
        int x = targetX + tilePair.first.first;
        int y = targetY + tilePair.first.second;
        
        int oldTileId = model.GetTileAt(globalRoomId, x, y);
        
        PaintTilesCommand::TileChange change;
        change.roomId = globalRoomId;
        change.x = x;
        change.y = y;
        change.oldTileId = oldTileId;
        change.newTileId = tilePair.second;
        tileChanges.push_back(change);
        
        // Apply immediately
        model.SetTileAt(globalRoomId, x, y, tilePair.second);
    }
    
    if (!tileChanges.empty()) {
        auto cmd = std::make_unique<PaintTilesCommand>(tileChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Paste edges
    std::vector<ModifyEdgesCommand::EdgeChange> edgeChanges;
    for (const auto& relEdge : clipboard.edges) {
        EdgeId edgeId(
            targetX + relEdge.dx1, targetY + relEdge.dy1,
            targetX + relEdge.dx2, targetY + relEdge.dy2
        );
        
        EdgeState oldState = model.GetEdgeState(edgeId);
        
        ModifyEdgesCommand::EdgeChange change;
        change.edgeId = edgeId;
        change.oldState = oldState;
        change.newState = relEdge.state;
        edgeChanges.push_back(change);
        
        // Apply immediately
        model.SetEdgeState(edgeId, relEdge.state);
    }
    
    if (!edgeChanges.empty()) {
        auto cmd = std::make_unique<ModifyEdgesCommand>(edgeChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Paste markers (create new markers with new IDs)
    for (const auto& relMarker : clipboard.markers) {
        Marker newMarker;
        newMarker.id = model.GenerateMarkerId();
        newMarker.roomId = "";
        newMarker.x = targetX + relMarker.dx;
        newMarker.y = targetY + relMarker.dy;
        newMarker.kind = relMarker.kind;
        newMarker.label = relMarker.label;
        newMarker.icon = relMarker.icon;
        newMarker.color = relMarker.color;
        newMarker.size = relMarker.size;
        newMarker.showLabel = relMarker.showLabel;
        
        auto cmd = std::make_unique<PlaceMarkerCommand>(newMarker, true);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    model.MarkDirty();
}

void CanvasPanel::EnterPasteMode() {
    if (clipboard.IsEmpty()) return;
    isPasteMode = true;
}

void CanvasPanel::ExitPasteMode() {
    isPasteMode = false;
}


} // namespace Cartograph
