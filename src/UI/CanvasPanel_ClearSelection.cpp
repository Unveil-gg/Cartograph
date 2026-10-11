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

void CanvasPanel::ClearSelection() {
    currentSelection.Clear();
    hasSelection = false;
    hasSelectedDoor = false;
    placingSpawn = 0;
    // Keep screen coordinates but mark selection as inactive
    isSelecting = false;
}

void CanvasPanel::SelectAll(const Model& model) {
    // Clear previous selection
    currentSelection.Clear();
    
    const std::string globalRoomId = "";
    
    // Calculate content bounds to find all content
    ContentBounds bounds = model.CalculateContentBounds();
    
    if (bounds.isEmpty) {
        // No content to select
        hasSelection = false;
        return;
    }
    
    // Add margin around content
    int minX = bounds.minX - 1;
    int minY = bounds.minY - 1;
    int maxX = bounds.maxX + 1;
    int maxY = bounds.maxY + 1;
    
    // Store bounding box
    currentSelection.bounds.x = minX;
    currentSelection.bounds.y = minY;
    currentSelection.bounds.w = maxX - minX + 1;
    currentSelection.bounds.h = maxY - minY + 1;
    
    // Collect all tiles if enabled
    if (selectTiles) {
        for (const auto& row : model.tiles) {
            for (const auto& run : row.runs) {
                for (int x = run.startX; x < run.startX + run.count; ++x) {
                    if (run.tileId != 0) {
                        currentSelection.tiles[{x, row.y}] = run.tileId;
                    }
                }
            }
        }
    }
    
    // Collect all edges if enabled
    if (selectEdges) {
        for (const auto& edgePair : model.edges) {
            if (edgePair.second != EdgeState::None) {
                currentSelection.edges[edgePair.first] = edgePair.second;
            }
        }
    }
    
    // Collect all markers if enabled
    if (selectMarkers) {
        for (const auto& marker : model.markers) {
            currentSelection.markerIds.push_back(marker.id);
        }
    }
    
    // Update bounding box to encompass all selected content
    if (!currentSelection.tiles.empty()) {
        int minTileX = INT_MAX, minTileY = INT_MAX;
        int maxTileX = INT_MIN, maxTileY = INT_MIN;
        
        for (const auto& tilePair : currentSelection.tiles) {
            minTileX = std::min(minTileX, tilePair.first.first);
            minTileY = std::min(minTileY, tilePair.first.second);
            maxTileX = std::max(maxTileX, tilePair.first.first);
            maxTileY = std::max(maxTileY, tilePair.first.second);
        }
        
        currentSelection.bounds.x = minTileX;
        currentSelection.bounds.y = minTileY;
        currentSelection.bounds.w = maxTileX - minTileX + 1;
        currentSelection.bounds.h = maxTileY - minTileY + 1;
    }
    
    // Mark selection as valid if we found anything
    hasSelection = !currentSelection.IsEmpty();
}

}  // namespace Cartograph
