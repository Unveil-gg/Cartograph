#pragma once

#include "../Model.h"
#include "../Canvas.h"
#include "../History.h"
#include "../Icons.h"
#include "../Keymap.h"
#include "../render/Renderer.h"
#include <memory>

// Forward declarations
struct SDL_Cursor;

namespace Cartograph {

/**
 * Canvas panel renderer and input handler.
 * Handles the main editing canvas with tools, pan/zoom, and interactions.
 */
class CanvasPanel {
public:
    CanvasPanel();
    ~CanvasPanel();
    
    /**
     * Tool types for canvas editing.
     */
    enum class Tool {
        Move, 
        Select, 
        Paint, 
        Fill, 
        Erase, 
        Marker, 
        Eyedropper,
        Zoom,          // Zoom in/out centered on click point
        RoomPaint,     // Paint cells to assign to active room
        RoomErase,     // Remove cells from rooms
        RoomFill       // Flood-fill cells into active room
    };
    
    /**
     * Render the canvas panel.
     * @param renderer Renderer for drawing
     * @param model Current model
     * @param canvas Canvas for view transforms
     * @param history History for undo/redo
     * @param icons Icon manager
     * @param keymap Keymap for input bindings
     */
    void Render(
        IRenderer& renderer,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        KeymapManager& keymap
    );
    
    // Tool state
    Tool currentTool = Tool::Move;
    
    // Selection state (for Select tool)
    bool isSelecting = false;
    float selectionStartX = 0.0f;
    float selectionStartY = 0.0f;
    float selectionEndX = 0.0f;
    float selectionEndY = 0.0f;
    
    // Paint state (for Paint/Erase tools)
    bool isPainting = false;
    int lastPaintedTileX = -1;
    int lastPaintedTileY = -1;
    std::vector<PaintTilesCommand::TileChange> currentPaintChanges;
    bool twoFingerEraseActive = false;
    
    // Edge modification state (for Paint tool)
    bool isModifyingEdges = false;
    std::vector<ModifyEdgesCommand::EdgeChange> currentEdgeChanges;
    
    // Marker tool state
    std::string selectedIconName = "dot";  // Default icon
    std::string markerLabel = "";
    Color markerColor = Color(0.3f, 0.8f, 0.3f, 1.0f);  // Green
    char markerColorHex[16] = "#4dcc4d";  // Hex representation
    Marker* selectedMarker = nullptr;  // For editing existing markers
    Marker* hoveredMarker = nullptr;   // For hover feedback
    
    // Marker drag state
    bool isDraggingMarker = false;
    float dragStartX = 0.0f;
    float dragStartY = 0.0f;
    
    // Marker clipboard
    std::vector<Marker> copiedMarkers;
    EdgeId hoveredEdge;
    bool isHoveringEdge = false;
    
    // Eyedropper tool state
    bool eyedropperAutoSwitchToPaint = false;
    Tool lastTool = Tool::Move;  // Track last tool for cursor restore
    
    // Room management state
    std::string selectedRoomId;        // Currently selected room
    std::string selectedRegionGroupId; // Currently selected region
    bool roomPaintMode = false;        // Room painting mode active
    bool showRoomOverlays = true;      // Show room visual overlays
    
    // Room assignment state (for room paint mode)
    bool isPaintingRoomCells = false;
    int lastRoomPaintX = -1;
    int lastRoomPaintY = -1;
    std::vector<ModifyRoomAssignmentsCommand::CellAssignment> 
        currentRoomAssignments;
    
    // Active room for room tools (RoomPaint, RoomErase, RoomFill)
    std::string activeRoomId;  // Room being painted/edited with room tools
    
    // Eraser tool state
    int eraserBrushSize = 1;  // 1-5, eraser brush size in tiles
    
    // Hovered tile coordinates (for status bar)
    int hoveredTileX = -1;
    int hoveredTileY = -1;
    bool isHoveringCanvas = false;
    
    // Selected palette tile
    int selectedTileId = 1;
    
    // Hierarchy panel visibility (affects layout)
    bool* showPropertiesPanel = nullptr;
    bool* layoutInitialized = nullptr;
    
private:
    /**
     * Detect if mouse position is near a cell edge.
     * @param mouseX Screen mouse X
     * @param mouseY Screen mouse Y
     * @param canvas Canvas for coordinate conversion
     * @param grid Grid configuration
     * @param outEdgeId Output: detected edge ID
     * @param outEdgeSide Output: which side of the cell
     * @return true if near an edge
     */
    bool DetectEdgeHover(
        float mouseX, float mouseY,
        const Canvas& canvas,
        const GridConfig& grid,
        EdgeId* outEdgeId,
        EdgeSide* outEdgeSide
    );
};

/**
 * Helper function: Get all tiles along a line from (x0, y0) to (x1, y1).
 * Uses Bresenham's line algorithm to ensure continuous painting.
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @return Vector of (x, y) tile coordinates along the line
 */
std::vector<std::pair<int, int>> GetTilesAlongLine(
    int x0, int y0, 
    int x1, int y1
);

} // namespace Cartograph

