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
void CanvasPanel::MoveSelection(
    Model& model, 
    History& history, 
    int dx, 
    int dy
) {
    if (!hasSelection || currentSelection.IsEmpty()) return;
    if (dx == 0 && dy == 0) return;
    
    const std::string globalRoomId = "";
    
    // Step 1: Collect all content to move
    auto tilesToMove = currentSelection.tiles;
    auto edgesToMove = currentSelection.edges;
    auto markersToMove = currentSelection.markerIds;
    
    // Step 2: Delete original content (tiles and edges)
    std::vector<PaintTilesCommand::TileChange> deleteTileChanges;
    for (const auto& tilePair : tilesToMove) {
        PaintTilesCommand::TileChange change;
        change.roomId = globalRoomId;
        change.x = tilePair.first.first;
        change.y = tilePair.first.second;
        change.oldTileId = tilePair.second;
        change.newTileId = 0;
        deleteTileChanges.push_back(change);
        model.SetTileAt(globalRoomId, change.x, change.y, 0);
    }
    
    std::vector<ModifyEdgesCommand::EdgeChange> deleteEdgeChanges;
    for (const auto& edgePair : edgesToMove) {
        ModifyEdgesCommand::EdgeChange change;
        change.edgeId = edgePair.first;
        change.oldState = edgePair.second;
        change.newState = EdgeState::None;
        deleteEdgeChanges.push_back(change);
        model.SetEdgeState(edgePair.first, EdgeState::None);
    }
    
    // Step 3: Place content at new positions
    std::vector<PaintTilesCommand::TileChange> placeTileChanges;
    for (const auto& tilePair : tilesToMove) {
        int newX = tilePair.first.first + dx;
        int newY = tilePair.first.second + dy;
        int oldTileId = model.GetTileAt(globalRoomId, newX, newY);
        
        PaintTilesCommand::TileChange change;
        change.roomId = globalRoomId;
        change.x = newX;
        change.y = newY;
        change.oldTileId = oldTileId;
        change.newTileId = tilePair.second;
        placeTileChanges.push_back(change);
        model.SetTileAt(globalRoomId, newX, newY, tilePair.second);
    }
    
    std::vector<ModifyEdgesCommand::EdgeChange> placeEdgeChanges;
    for (const auto& edgePair : edgesToMove) {
        EdgeId newEdgeId(
            edgePair.first.x1 + dx, edgePair.first.y1 + dy,
            edgePair.first.x2 + dx, edgePair.first.y2 + dy
        );
        EdgeState oldState = model.GetEdgeState(newEdgeId);
        
        ModifyEdgesCommand::EdgeChange change;
        change.edgeId = newEdgeId;
        change.oldState = oldState;
        change.newState = edgePair.second;
        placeEdgeChanges.push_back(change);
        model.SetEdgeState(newEdgeId, edgePair.second);
    }
    
    // Step 4: Move markers
    for (const auto& markerId : markersToMove) {
        Marker* marker = model.FindMarker(markerId);
        if (marker) {
            float oldX = marker->x;
            float oldY = marker->y;
            marker->x += dx;
            marker->y += dy;
            
            auto cmd = std::make_unique<MoveMarkersCommand>(
                markerId, oldX, oldY, marker->x, marker->y
            );
            history.AddCommand(std::move(cmd), model, false);
        }
    }
    
    // Add tile commands to history
    if (!deleteTileChanges.empty()) {
        auto cmd = std::make_unique<PaintTilesCommand>(deleteTileChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    if (!placeTileChanges.empty()) {
        auto cmd = std::make_unique<PaintTilesCommand>(placeTileChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Add edge commands to history
    if (!deleteEdgeChanges.empty()) {
        auto cmd = std::make_unique<ModifyEdgesCommand>(deleteEdgeChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    if (!placeEdgeChanges.empty()) {
        auto cmd = std::make_unique<ModifyEdgesCommand>(placeEdgeChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Step 5: Update selection bounds
    currentSelection.bounds.x += dx;
    currentSelection.bounds.y += dy;
    
    // Update selection tiles map with new positions
    std::unordered_map<std::pair<int, int>, int, PairHash> newTiles;
    for (const auto& tilePair : currentSelection.tiles) {
        newTiles[{tilePair.first.first + dx, tilePair.first.second + dy}] = 
            tilePair.second;
    }
    currentSelection.tiles = std::move(newTiles);
    
    // Update selection edges map with new positions
    std::unordered_map<EdgeId, EdgeState, EdgeIdHash> newEdges;
    for (const auto& edgePair : currentSelection.edges) {
        EdgeId newEdgeId(
            edgePair.first.x1 + dx, edgePair.first.y1 + dy,
            edgePair.first.x2 + dx, edgePair.first.y2 + dy
        );
        newEdges[newEdgeId] = edgePair.second;
    }
    currentSelection.edges = std::move(newEdges);
    
    model.MarkDirty();
}

void CanvasPanel::EnterFloatingMode() {
    if (!hasSelection || currentSelection.IsEmpty()) return;
    if (isFloatingSelection) return;  // Already floating
    
    // Store origin position
    floatingOriginX = currentSelection.bounds.x;
    floatingOriginY = currentSelection.bounds.y;
    
    // Copy selection content to floating buffer
    floatingContent = currentSelection;
    
    // Reset drag offset
    dragOffsetX = 0;
    dragOffsetY = 0;
    
    isFloatingSelection = true;
}

void CanvasPanel::CommitFloatingSelection(Model& model, History& history) {
    if (!isFloatingSelection || floatingContent.IsEmpty()) {
        CancelFloatingSelection();
        return;
    }
    
    const std::string globalRoomId = "";
    
    // Calculate final position
    int finalX = floatingOriginX + dragOffsetX;
    int finalY = floatingOriginY + dragOffsetY;
    
    // Step 1: Delete content at original positions
    std::vector<PaintTilesCommand::TileChange> deleteTileChanges;
    for (const auto& tilePair : floatingContent.tiles) {
        int x = tilePair.first.first;
        int y = tilePair.first.second;
        
        PaintTilesCommand::TileChange change;
        change.roomId = globalRoomId;
        change.x = x;
        change.y = y;
        change.oldTileId = model.GetTileAt(globalRoomId, x, y);
        change.newTileId = 0;
        deleteTileChanges.push_back(change);
        model.SetTileAt(globalRoomId, x, y, 0);
    }
    
    std::vector<ModifyEdgesCommand::EdgeChange> deleteEdgeChanges;
    for (const auto& edgePair : floatingContent.edges) {
        ModifyEdgesCommand::EdgeChange change;
        change.edgeId = edgePair.first;
        change.oldState = model.GetEdgeState(edgePair.first);
        change.newState = EdgeState::None;
        deleteEdgeChanges.push_back(change);
        model.SetEdgeState(edgePair.first, EdgeState::None);
    }
    
    // Step 2: Place content at new positions
    std::vector<PaintTilesCommand::TileChange> placeTileChanges;
    for (const auto& tilePair : floatingContent.tiles) {
        int newX = tilePair.first.first + dragOffsetX;
        int newY = tilePair.first.second + dragOffsetY;
        int oldTileId = model.GetTileAt(globalRoomId, newX, newY);
        
        PaintTilesCommand::TileChange change;
        change.roomId = globalRoomId;
        change.x = newX;
        change.y = newY;
        change.oldTileId = oldTileId;
        change.newTileId = tilePair.second;
        placeTileChanges.push_back(change);
        model.SetTileAt(globalRoomId, newX, newY, tilePair.second);
    }
    
    std::vector<ModifyEdgesCommand::EdgeChange> placeEdgeChanges;
    for (const auto& edgePair : floatingContent.edges) {
        EdgeId newEdgeId(
            edgePair.first.x1 + dragOffsetX, edgePair.first.y1 + dragOffsetY,
            edgePair.first.x2 + dragOffsetX, edgePair.first.y2 + dragOffsetY
        );
        EdgeState oldState = model.GetEdgeState(newEdgeId);
        
        ModifyEdgesCommand::EdgeChange change;
        change.edgeId = newEdgeId;
        change.oldState = oldState;
        change.newState = edgePair.second;
        placeEdgeChanges.push_back(change);
        model.SetEdgeState(newEdgeId, edgePair.second);
    }
    
    // Step 3: Move markers
    for (const auto& markerId : floatingContent.markerIds) {
        Marker* marker = model.FindMarker(markerId);
        if (marker) {
            float oldX = marker->x;
            float oldY = marker->y;
            marker->x += dragOffsetX;
            marker->y += dragOffsetY;
            
            auto cmd = std::make_unique<MoveMarkersCommand>(
                markerId, oldX, oldY, marker->x, marker->y
            );
            history.AddCommand(std::move(cmd), model, false);
        }
    }
    
    // Add commands to history
    if (!deleteTileChanges.empty()) {
        auto cmd = std::make_unique<PaintTilesCommand>(deleteTileChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    if (!placeTileChanges.empty()) {
        auto cmd = std::make_unique<PaintTilesCommand>(placeTileChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    if (!deleteEdgeChanges.empty()) {
        auto cmd = std::make_unique<ModifyEdgesCommand>(deleteEdgeChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    if (!placeEdgeChanges.empty()) {
        auto cmd = std::make_unique<ModifyEdgesCommand>(placeEdgeChanges);
        history.AddCommand(std::move(cmd), model, false);
    }
    
    // Update selection to new position
    currentSelection.bounds.x = finalX;
    currentSelection.bounds.y = finalY;
    
    std::unordered_map<std::pair<int, int>, int, PairHash> newTiles;
    for (const auto& tilePair : floatingContent.tiles) {
        newTiles[{tilePair.first.first + dragOffsetX, 
                  tilePair.first.second + dragOffsetY}] = tilePair.second;
    }
    currentSelection.tiles = std::move(newTiles);
    
    std::unordered_map<EdgeId, EdgeState, EdgeIdHash> newEdges;
    for (const auto& edgePair : floatingContent.edges) {
        EdgeId newEdgeId(
            edgePair.first.x1 + dragOffsetX, edgePair.first.y1 + dragOffsetY,
            edgePair.first.x2 + dragOffsetX, edgePair.first.y2 + dragOffsetY
        );
        newEdges[newEdgeId] = edgePair.second;
    }
    currentSelection.edges = std::move(newEdges);
    
    // Exit floating mode
    isFloatingSelection = false;
    floatingContent.Clear();
    dragOffsetX = 0;
    dragOffsetY = 0;
    
    model.MarkDirty();
}

void CanvasPanel::CancelFloatingSelection() {
    // Simply exit floating mode - content was never removed from model
    isFloatingSelection = false;
    floatingContent.Clear();
    dragOffsetX = 0;
    dragOffsetY = 0;
}


} // namespace Cartograph
