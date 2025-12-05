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
    // Clean up SDL cursors
    if (m_eyedropperCursor) {
        SDL_DestroyCursor(m_eyedropperCursor);
        m_eyedropperCursor = nullptr;
    }
    if (m_zoomCursor) {
        SDL_DestroyCursor(m_zoomCursor);
        m_zoomCursor = nullptr;
    }
    // Don't destroy default cursor - it's owned by SDL
}

void CanvasPanel::InitCursors() {
    if (m_cursorsInitialized) return;
    
    // Get assets directory - try multiple paths for dev vs installed
    std::string assetsDir = Platform::GetAssetsDir();
    std::string toolsDir = assetsDir + "tools/";
    
    // Check if tools directory exists, fallback to source assets for dev
    if (!std::filesystem::exists(toolsDir)) {
        // Try source assets directory (for development)
        const char* basePath = SDL_GetBasePath();
        if (basePath) {
            std::string devPath = std::string(basePath) + 
                "../../../assets/tools/";
            if (std::filesystem::exists(devPath)) {
                toolsDir = devPath;
            }
        }
    }
    
    // Store default cursor for restoration
    m_defaultCursor = SDL_GetDefaultCursor();
    
    // Cursor fill modes
    enum class FillMode {
        None,              // Keep original colors
        WhiteInteriorFill  // Fill interior (enclosed) areas with white
    };
    
    // Helper lambda to load cursor from PNG with fill options
    auto loadCursor = [](const std::string& path, FillMode fillMode, 
                        int hotX, int hotY) -> SDL_Cursor* {
        int width, height, channels;
        unsigned char* pixels = stbi_load(
            path.c_str(), &width, &height, &channels, 4  // Force RGBA
        );
        
        if (!pixels) {
            return nullptr;
        }
        
        // Apply fill mode
        if (fillMode == FillMode::WhiteInteriorFill) {
            // Fill only INTERIOR transparent pixels with white
            // Exterior (connected to edges) stays transparent
            
            // Create visited mask for flood-fill from edges
            std::vector<bool> exterior(width * height, false);
            std::vector<std::pair<int, int>> queue;
            
            // Helper to check if pixel is transparent
            auto isTransparent = [&](int x, int y) {
                if (x < 0 || x >= width || y < 0 || y >= height) 
                    return false;
                return pixels[(y * width + x) * 4 + 3] < 128;
            };
            
            // Seed flood-fill from all transparent edge pixels
            for (int x = 0; x < width; ++x) {
                if (isTransparent(x, 0)) {
                    exterior[x] = true;
                    queue.push_back({x, 0});
                }
                if (isTransparent(x, height - 1)) {
                    exterior[(height - 1) * width + x] = true;
                    queue.push_back({x, height - 1});
                }
            }
            for (int y = 0; y < height; ++y) {
                if (isTransparent(0, y)) {
                    exterior[y * width] = true;
                    queue.push_back({0, y});
                }
                if (isTransparent(width - 1, y)) {
                    exterior[y * width + width - 1] = true;
                    queue.push_back({width - 1, y});
                }
            }
            
            // Flood-fill to mark all exterior transparent pixels
            while (!queue.empty()) {
                auto [cx, cy] = queue.back();
                queue.pop_back();
                
                // Check 4-connected neighbors
                const int dx[] = {-1, 1, 0, 0};
                const int dy[] = {0, 0, -1, 1};
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int idx = ny * width + nx;
                        if (!exterior[idx] && isTransparent(nx, ny)) {
                            exterior[idx] = true;
                            queue.push_back({nx, ny});
                        }
                    }
                }
            }
            
            // Fill interior transparent pixels with white
            for (int i = 0; i < width * height; ++i) {
                unsigned char a = pixels[i * 4 + 3];
                if (a < 128 && !exterior[i]) {
                    // Interior transparent pixel - fill with white
                    pixels[i * 4 + 0] = 255;  // R
                    pixels[i * 4 + 1] = 255;  // G
                    pixels[i * 4 + 2] = 255;  // B
                    pixels[i * 4 + 3] = 255;  // A (fully opaque)
                }
                // Keep strokes and exterior transparent pixels as-is
            }
        }
        // FillMode::None - keep original colors
        
        // Create SDL surface from pixel data
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            width, height,
            SDL_PIXELFORMAT_RGBA32,
            pixels,
            width * 4  // pitch = width * 4 bytes per pixel
        );
        
        if (!surface) {
            stbi_image_free(pixels);
            return nullptr;
        }
        
        // Create cursor from surface
        SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX, hotY);
        
        // Clean up surface (cursor has its own copy)
        SDL_DestroySurface(surface);
        stbi_image_free(pixels);
        
        return cursor;
    };
    
    // Load eyedropper cursor (white interior fill, hotspot at tip)
    // Icons are 32x32, pipette tip is at bottom-left (~x=4, y=28)
    m_eyedropperCursor = loadCursor(
        toolsDir + "pipette.png", 
        FillMode::WhiteInteriorFill,  // White fill inside outlines only
        4, 28   // Hotspot at pipette tip
    );
    
    // Load zoom cursor (keep original black, hotspot at center)
    // Icons are 32x32, center of magnifying glass (~x=12, y=12)
    m_zoomCursor = loadCursor(
        toolsDir + "zoom-in.png",
        FillMode::None,  // Keep original black color
        12, 12  // Hotspot at center of lens
    );
    
    m_cursorsInitialized = true;
}

void CanvasPanel::UpdateCursor() {
    // Only update cursor if we have initialized them
    if (!m_cursorsInitialized) {
        InitCursors();
    }
    
    // Determine which cursor to use
    SDL_Cursor* desiredCursor = m_defaultCursor;
    
    if (isHoveringCanvas) {
        if (currentTool == Tool::Eyedropper && m_eyedropperCursor) {
            desiredCursor = m_eyedropperCursor;
        } else if (currentTool == Tool::Zoom && m_zoomCursor) {
            desiredCursor = m_zoomCursor;
        }
    }
    
    // Only call SDL_SetCursor if cursor needs to change
    // (SDL_GetCursor comparison to avoid unnecessary calls)
    if (SDL_GetCursor() != desiredCursor) {
        SDL_SetCursor(desiredCursor);
    }
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
        // Toggle hierarchy panel
        if (keymap.IsActionTriggered("togglePropertiesPanel")) {
            *showPropertiesPanel = !(*showPropertiesPanel);
            *layoutInitialized = false;  // Trigger layout rebuild
        }
        
        // Tool switching shortcuts
        Tool prevTool = currentTool;
        
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
        if (keymap.IsActionTriggered("toolZoom")) {
            currentTool = Tool::Zoom;
        }
        if (keymap.IsActionTriggered("toolMarker")) {
            currentTool = Tool::Marker;
        }
        
        // Clear selection when switching away from Select tool
        if (prevTool == Tool::Select && currentTool != Tool::Select) {
            ClearSelection();
        }
        
        // Escape clears selection when Select tool is active
        if (currentTool == Tool::Select && 
            ImGui::IsKeyPressed(ImGuiKey_Escape) && hasSelection) {
            ClearSelection();
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
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Update paste preview position
            if (isPasteMode) {
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &pastePreviewX, &pastePreviewY
                );
                
                // Click to commit paste
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    PasteClipboard(model, history, pastePreviewX, pastePreviewY);
                    ExitPasteMode();
                }
            }
            // Check if clicking inside existing selection (keep selection active)
            // or outside (start new selection)
            else if (hasSelection && 
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                int clickTileX, clickTileY;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &clickTileX, &clickTileY
                );
                
                // Check if click is inside selection bounds
                bool insideBounds = 
                    currentSelection.bounds.Contains(clickTileX, clickTileY);
                
                if (!insideBounds) {
                    // Clicking outside: clear selection and start new
                    ClearSelection();
                    selectionStartX = mousePos.x;
                    selectionStartY = mousePos.y;
                    selectionEndX = mousePos.x;
                    selectionEndY = mousePos.y;
                    isSelecting = true;
                }
                // Clicking inside: do nothing, keep selection active
                // (use Cut/Copy/Paste to move content)
            }
            // Start new selection (no existing selection)
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ClearSelection();
                selectionStartX = mousePos.x;
                selectionStartY = mousePos.y;
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                isSelecting = true;
            }
            
            // Update selection rectangle while dragging
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isSelecting) {
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
            }
            
            // Finish selection rectangle
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isSelecting) {
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                isSelecting = false;
                
                // Populate selection with content from the rectangle
                PopulateSelectionFromRect(model, canvas);
            }
            
            // Right-click context menu for selection operations
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && 
                !isSelecting && !isPasteMode) {
                ImGui::OpenPopup("SelectionContextMenu");
            }
            
            if (ImGui::BeginPopup("SelectionContextMenu")) {
                bool selectionActive = hasSelection && 
                                       !currentSelection.IsEmpty();
                bool clipboardHasContent = !clipboard.IsEmpty();
                
                // Cut (copy + delete)
                if (ImGui::MenuItem("Cut", Platform::FormatShortcut("X").c_str(),
                                   false, selectionActive)) {
                    CopySelection(model);
                    DeleteSelection(model, history);
                }
                
                // Copy
                if (ImGui::MenuItem("Copy", Platform::FormatShortcut("C").c_str(),
                                   false, selectionActive)) {
                    CopySelection(model);
                }
                
                // Paste
                if (ImGui::MenuItem("Paste", 
                                   Platform::FormatShortcut("V").c_str(),
                                   false, clipboardHasContent)) {
                    EnterPasteMode();
                }
                
                ImGui::Separator();
                
                // Delete
                if (ImGui::MenuItem("Delete", "Del", false, selectionActive)) {
                    DeleteSelection(model, history);
                }
                
                ImGui::Separator();
                
                // Select All
                if (ImGui::MenuItem("Select All", 
                                   Platform::FormatShortcut("A").c_str())) {
                    SelectAll(model);
                }
                
                // Deselect
                if (ImGui::MenuItem("Deselect", "Esc", false, selectionActive)) {
                    ClearSelection();
                }
                
                ImGui::EndPopup();
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
                    
                    // Clamp to grid bounds
                    boundMinX = std::max(0, boundMinX);
                    boundMinY = std::max(0, boundMinY);
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
                        
                        // Add neighbors to visit (4-way connectivity)
                        toVisit.push_back({x + 1, y});
                        toVisit.push_back({x - 1, y});
                        toVisit.push_back({x, y + 1});
                        toVisit.push_back({x, y - 1});
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
        else if (currentTool == Tool::Zoom) {
            // Zoom tool: 
            // - Left-click: Zoom in centered on click point
            // - Right-click: Zoom out centered on click point
            
            // Zoom presets (display percentages)
            static const int ZOOM_PRESETS[] = {
                10, 25, 50, 75, 100, 150, 200, 400, 800, 1000
            };
            static const int ZOOM_PRESET_COUNT = 
                sizeof(ZOOM_PRESETS) / sizeof(ZOOM_PRESETS[0]);
            
            // Helper lambdas for zoom calculations
            auto displayToInternal = [](int pct) {
                return (pct / 100.0f) * Canvas::DEFAULT_ZOOM;
            };
            auto internalToDisplay = [](float zoom) {
                return static_cast<int>(
                    (zoom / Canvas::DEFAULT_ZOOM) * 100.0f
                );
            };
            auto getNextPreset = [&](int currentPct, bool zoomIn) {
                if (zoomIn) {
                    for (int i = 0; i < ZOOM_PRESET_COUNT; ++i) {
                        if (ZOOM_PRESETS[i] > currentPct) {
                            return ZOOM_PRESETS[i];
                        }
                    }
                    return ZOOM_PRESETS[ZOOM_PRESET_COUNT - 1];
                } else {
                    for (int i = ZOOM_PRESET_COUNT - 1; i >= 0; --i) {
                        if (ZOOM_PRESETS[i] < currentPct) {
                            return ZOOM_PRESETS[i];
                        }
                    }
                    return ZOOM_PRESETS[0];
                }
            };
            
            ImVec2 mousePos = ImGui::GetMousePos();
            int currentPercent = internalToDisplay(canvas.zoom);
            float oldZoom = canvas.zoom;
            
            // Left-click: Zoom in
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                int newPercent = getNextPreset(currentPercent, true);
                if (newPercent != currentPercent) {
                    float newZoom = displayToInternal(newPercent);
                    
                    // Zoom centered on click point
                    canvas.ZoomToPoint(newZoom, mousePos.x, mousePos.y);
                    
                    // Add to history
                    auto cmd = std::make_unique<SetZoomCommand>(
                        canvas, oldZoom, newZoom, newPercent
                    );
                    history.AddCommand(std::move(cmd), model, false);
                }
            }
            // Right-click: Zoom out
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                int newPercent = getNextPreset(currentPercent, false);
                if (newPercent != currentPercent) {
                    float newZoom = displayToInternal(newPercent);
                    
                    // Zoom centered on click point
                    canvas.ZoomToPoint(newZoom, mousePos.x, mousePos.y);
                    
                    // Add to history
                    auto cmd = std::make_unique<SetZoomCommand>(
                        canvas, oldZoom, newZoom, newPercent
                    );
                    history.AddCommand(std::move(cmd), model, false);
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
                        
                        // Clamp to grid bounds
                        boundMinX = std::max(0, boundMinX);
                        boundMinY = std::max(0, boundMinY);
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
    
    // Keyboard shortcuts
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // Selection operations take priority when Select tool is active
        if (currentTool == Tool::Select) {
            // Copy selection (Ctrl+C)
            if (keymap.IsActionTriggered("copy") && hasSelection) {
                CopySelection(model);
            }
            
            // Cut selection (Ctrl+X) - copy to clipboard + delete immediately
            if (keymap.IsActionTriggered("cut") && hasSelection) {
                CopySelection(model);
                DeleteSelection(model, history);
            }
            
            // Paste (Ctrl+V) - enter paste mode if clipboard has content
            if (keymap.IsActionTriggered("paste") && !clipboard.IsEmpty()) {
                EnterPasteMode();
            }
            
            // Delete selection (Delete/Backspace)
            if (hasSelection && 
                (keymap.IsActionTriggered("delete") || 
                 keymap.IsActionTriggered("deleteAlt"))) {
                DeleteSelection(model, history);
            }
            
            // Escape cancels paste mode or clears selection
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (isPasteMode) {
                    ExitPasteMode();
                } else if (hasSelection) {
                    ClearSelection();
                }
            }
            
            // Arrow keys nudge selection (moves content immediately)
            if (hasSelection && !isPasteMode) {
                int nudgeX = 0, nudgeY = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) nudgeX = -1;
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nudgeX = 1;
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) nudgeY = -1;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) nudgeY = 1;
                
                if (nudgeX != 0 || nudgeY != 0) {
                    MoveSelection(model, history, nudgeX, nudgeY);
                }
            }
            
            // Select All (Ctrl+A)
            if (keymap.IsActionTriggered("selectAll")) {
                SelectAll(model);
            }
        }
        // Marker copy/paste (when not in Select tool)
        else {
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
    
    // Update cursor based on current tool and hover state
    UpdateCursor();
    
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
    
    // Draw selection rectangle while dragging
    if (currentTool == Tool::Select && isSelecting) {
        // Calculate rectangle bounds
        float minX = std::min(selectionStartX, selectionEndX);
        float minY = std::min(selectionStartY, selectionEndY);
        float maxX = std::max(selectionStartX, selectionEndX);
        float maxY = std::max(selectionStartY, selectionEndY);
        
        // Draw semi-transparent fill (using theme colors)
        ImU32 fillColor = ImGui::GetColorU32(
            model.theme.selectionFill.ToImVec4()
        );
        drawList->AddRectFilled(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            fillColor
        );
        
        // Draw border (using theme colors)
        ImU32 borderColor = ImGui::GetColorU32(
            model.theme.selectionBorder.ToImVec4()
        );
        drawList->AddRect(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            borderColor,
            0.0f,
            0,
            2.0f
        );
    }
    
    // Draw selection highlight for completed selection
    if (hasSelection && !currentSelection.IsEmpty()) {
        // Selection colors (using theme)
        ImU32 tileFillColor = ImGui::GetColorU32(
            model.theme.selectionFill.ToImVec4()
        );
        ImU32 tileBorderColor = ImGui::GetColorU32(
            model.theme.selectionBorder.ToImVec4()
        );
        ImU32 edgeColor = ImGui::GetColorU32(
            ImVec4(model.theme.selectionBorder.r, 
                   model.theme.selectionBorder.g + 0.2f,
                   model.theme.selectionBorder.b, 0.9f)
        );
        ImU32 markerColor = ImGui::GetColorU32(
            ImVec4(1.0f, 0.8f, 0.2f, 0.9f)  // Keep distinct marker highlight
        );
        
        float tileW = model.grid.tileWidth * canvas.zoom;
        float tileH = model.grid.tileHeight * canvas.zoom;
        
        // Draw highlight overlay for selected tiles
        for (const auto& tilePair : currentSelection.tiles) {
            int tx = tilePair.first.first;
            int ty = tilePair.first.second;
            
            // Convert tile to screen coordinates
            float wx, wy;
            canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx, &wy);
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            // Draw highlight overlay
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + tileW, sy + tileH),
                tileFillColor
            );
        }
        
        // Draw bounding box around entire selection
        int boundsX = currentSelection.bounds.x;
        int boundsY = currentSelection.bounds.y;
        
        float bx, by;
        canvas.TileToWorld(
            boundsX, boundsY,
            model.grid.tileWidth, model.grid.tileHeight, &bx, &by
        );
        float bsx, bsy;
        canvas.WorldToScreen(bx, by, &bsx, &bsy);
        
        float boxW = currentSelection.bounds.w * tileW;
        float boxH = currentSelection.bounds.h * tileH;
        
        // Draw marching ants border (alternating dash pattern)
        float dashLen = 6.0f;
        float time = static_cast<float>(ImGui::GetTime());
        float offset = std::fmod(time * 20.0f, dashLen * 2.0f);
        
        // Draw dashed rectangle
        auto drawDashedLine = [&](ImVec2 p1, ImVec2 p2) {
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.1f) return;
            
            float ux = dx / len;
            float uy = dy / len;
            
            float pos = -offset;
            while (pos < len) {
                float start = std::max(0.0f, pos);
                float end = std::min(len, pos + dashLen);
                if (end > start) {
                    drawList->AddLine(
                        ImVec2(p1.x + ux * start, p1.y + uy * start),
                        ImVec2(p1.x + ux * end, p1.y + uy * end),
                        tileBorderColor,
                        2.0f
                    );
                }
                pos += dashLen * 2.0f;
            }
        };
        
        // Top, right, bottom, left edges
        drawDashedLine(ImVec2(bsx, bsy), ImVec2(bsx + boxW, bsy));
        drawDashedLine(ImVec2(bsx + boxW, bsy), ImVec2(bsx + boxW, bsy + boxH));
        drawDashedLine(ImVec2(bsx + boxW, bsy + boxH), ImVec2(bsx, bsy + boxH));
        drawDashedLine(ImVec2(bsx, bsy + boxH), ImVec2(bsx, bsy));
        
        // Draw selected edges with highlight
        for (const auto& edgePair : currentSelection.edges) {
            const EdgeId& edge = edgePair.first;
            
            // Calculate edge midpoint and draw highlight
            float wx1, wy1, wx2, wy2;
            canvas.TileToWorld(edge.x1, edge.y1, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx1, &wy1);
            canvas.TileToWorld(edge.x2, edge.y2, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx2, &wy2);
            
            float sx1, sy1, sx2, sy2;
            canvas.WorldToScreen(wx1, wy1, &sx1, &sy1);
            canvas.WorldToScreen(wx2, wy2, &sx2, &sy2);
            
            // Center of each tile
            sx1 += tileW / 2.0f;
            sy1 += tileH / 2.0f;
            sx2 += tileW / 2.0f;
            sy2 += tileH / 2.0f;
            
            // Draw edge midpoint highlight
            float midX = (sx1 + sx2) / 2.0f;
            float midY = (sy1 + sy2) / 2.0f;
            
            drawList->AddCircleFilled(
                ImVec2(midX, midY),
                5.0f,
                edgeColor,
                8
            );
        }
        
        // Draw selected markers with highlight ring
        for (const auto& markerId : currentSelection.markerIds) {
            const Marker* marker = model.FindMarker(markerId);
            if (!marker) continue;
            
            // Convert marker position to screen
            float wx, wy;
            canvas.TileToWorld(
                static_cast<int>(marker->x), 
                static_cast<int>(marker->y),
                model.grid.tileWidth, model.grid.tileHeight, &wx, &wy
            );
            
            // Add fractional offset
            wx += (marker->x - std::floor(marker->x)) * model.grid.tileWidth;
            wy += (marker->y - std::floor(marker->y)) * model.grid.tileHeight;
            
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            // Draw selection ring around marker
            float minDim = static_cast<float>(
                std::min(model.grid.tileWidth, model.grid.tileHeight)
            );
            float markerSize = minDim * canvas.zoom * marker->size;
            
            drawList->AddCircle(
                ImVec2(sx, sy),
                markerSize / 2.0f + 4.0f,
                markerColor,
                16,
                3.0f
            );
        }
    }
    
    // Draw paste preview if in paste mode
    if (currentTool == Tool::Select && isPasteMode && !clipboard.IsEmpty()) {
        // Paste preview colors (using theme)
        ImU32 previewFillColor = ImGui::GetColorU32(
            model.theme.pastePreviewFill.ToImVec4()
        );
        ImU32 previewBorderColor = ImGui::GetColorU32(
            model.theme.pastePreviewBorder.ToImVec4()
        );
        ImU32 previewTileColor = ImGui::GetColorU32(
            ImVec4(model.theme.pastePreviewBorder.r,
                   model.theme.pastePreviewBorder.g,
                   model.theme.pastePreviewBorder.b, 0.4f)
        );
        
        float tileW = model.grid.tileWidth * canvas.zoom;
        float tileH = model.grid.tileHeight * canvas.zoom;
        
        // Draw ghost tiles
        for (const auto& tilePair : clipboard.tiles) {
            int tx = pastePreviewX + tilePair.first.first;
            int ty = pastePreviewY + tilePair.first.second;
            
            float wx, wy;
            canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                              model.grid.tileHeight, &wx, &wy);
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + tileW, sy + tileH),
                previewTileColor
            );
        }
        
        // Draw bounding box outline
        float bx, by;
        canvas.TileToWorld(
            pastePreviewX, pastePreviewY,
            model.grid.tileWidth, model.grid.tileHeight, &bx, &by
        );
        float bsx, bsy;
        canvas.WorldToScreen(bx, by, &bsx, &bsy);
        
        float boxW = clipboard.width * tileW;
        float boxH = clipboard.height * tileH;
        
        // Dashed outline
        float dashLen = 8.0f;
        float time = static_cast<float>(ImGui::GetTime());
        float offset = std::fmod(time * 30.0f, dashLen * 2.0f);
        
        auto drawDashedLine = [&](ImVec2 p1, ImVec2 p2) {
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.1f) return;
            
            float ux = dx / len;
            float uy = dy / len;
            
            float pos = -offset;
            while (pos < len) {
                float start = std::max(0.0f, pos);
                float end = std::min(len, pos + dashLen);
                if (end > start) {
                    drawList->AddLine(
                        ImVec2(p1.x + ux * start, p1.y + uy * start),
                        ImVec2(p1.x + ux * end, p1.y + uy * end),
                        previewBorderColor,
                        2.0f
                    );
                }
                pos += dashLen * 2.0f;
            }
        };
        
        drawDashedLine(ImVec2(bsx, bsy), ImVec2(bsx + boxW, bsy));
        drawDashedLine(ImVec2(bsx + boxW, bsy), ImVec2(bsx + boxW, bsy + boxH));
        drawDashedLine(ImVec2(bsx + boxW, bsy + boxH), ImVec2(bsx, bsy + boxH));
        drawDashedLine(ImVec2(bsx, bsy + boxH), ImVec2(bsx, bsy));
        
        // Draw "Click to paste" hint
        const char* pasteText = "Click to paste";
        ImVec2 textSize = ImGui::CalcTextSize(pasteText);
        ImVec2 textPos(bsx + boxW / 2.0f - textSize.x / 2.0f, 
                      bsy - textSize.y - 8.0f);
        
        drawList->AddRectFilled(
            ImVec2(textPos.x - 4, textPos.y - 2),
            ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
            ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.8f))
        );
        drawList->AddText(textPos, 
            ImGui::GetColorU32(model.theme.pastePreviewBorder.ToImVec4()), 
            pasteText);
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
        // Note: Erase/paint preview colors could be theme-customizable later
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

// ============================================================================
// Selection methods
// ============================================================================

void CanvasPanel::ClearSelection() {
    currentSelection.Clear();
    hasSelection = false;
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
    int minTileX, minTileY, maxTileX, maxTileY;
    canvas.ScreenToTile(
        minScreenX, minScreenY,
        model.grid.tileWidth, model.grid.tileHeight,
        &minTileX, &minTileY
    );
    canvas.ScreenToTile(
        maxScreenX, maxScreenY,
        model.grid.tileWidth, model.grid.tileHeight,
        &maxTileX, &maxTileY
    );
    
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
