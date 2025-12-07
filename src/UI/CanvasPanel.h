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

class Modals;  // Forward declaration for fill confirmation

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
        RoomSelect,    // Click cell to select its room
        RoomPaint,     // Paint cells to assign to active room
        RoomFill,      // Flood-fill cells into active room
        RoomErase      // Remove cells from rooms
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
    bool isSelecting = false;           // Currently dragging selection rect
    float selectionStartX = 0.0f;       // Screen coords of selection start
    float selectionStartY = 0.0f;
    float selectionEndX = 0.0f;         // Screen coords of selection end
    float selectionEndY = 0.0f;
    
    // Selection layer filters (what to include in selection)
    bool selectTiles = true;            // Include tiles in selection
    bool selectEdges = true;            // Include walls/doors in selection
    bool selectMarkers = true;          // Include markers in selection
    
    // Current selection content (populated after selection completes)
    SelectionData currentSelection;
    bool hasSelection = false;          // True if currentSelection is valid
    
    // Clipboard for copy/paste
    ClipboardData clipboard;
    
    // Paste preview state
    bool isPasteMode = false;           // Currently in paste preview mode
    int pastePreviewX = 0;              // Tile position for paste preview
    int pastePreviewY = 0;
    
    // Selection drag state (for move operation)
    bool isDraggingSelection = false;
    int dragSelectionStartX = 0;        // Original selection position
    int dragSelectionStartY = 0;
    int dragOffsetX = 0;                // Current drag offset in tiles
    int dragOffsetY = 0;
    
    // Floating selection state (content lifted from canvas for preview)
    bool isFloatingSelection = false;   // True when selection is "floating"
    int floatingOriginX = 0;            // Original tile X before floating
    int floatingOriginY = 0;            // Original tile Y before floating
    SelectionData floatingContent;      // Content being floated (for rendering)
    
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
    std::string selectedMarkerId = "";  // ID of selected marker (safe across vector realloc)
    std::string hoveredMarkerId = "";   // ID of hovered marker (safe across vector realloc)
    
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
    std::string hoveredRoomId;         // Room hovered in UI (for canvas highlight)
    
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
    
    // Pending fill state (for confirmation dialog when exceeding soft limit)
    bool hasPendingTileFill = false;
    std::vector<PaintTilesCommand::TileChange> pendingTileFillChanges;
    bool hasPendingRoomFill = false;
    std::vector<ModifyRoomAssignmentsCommand::CellAssignment> 
        pendingRoomFillAssignments;
    std::string pendingRoomFillActiveRoomId;  // Room ID for pending room fill
    
    // Hovered tile coordinates (for status bar)
    int hoveredTileX = -1;
    int hoveredTileY = -1;
    bool isHoveringCanvas = false;
    
    // Selected palette tile
    int selectedTileId = 1;
    
    // Hierarchy panel visibility (affects layout)
    bool* showPropertiesPanel = nullptr;
    bool* layoutInitialized = nullptr;
    
    // Modals reference for fill confirmation dialog
    Modals* modals = nullptr;
    
    /**
     * Initialize custom cursors for tools.
     * Call once after SDL is initialized.
     */
    void InitCursors();
    
    /**
     * Update cursor based on current tool and hover state.
     * Called each frame during Render.
     */
    void UpdateCursor();
    
    /**
     * Clear current selection.
     */
    void ClearSelection();
    
    /**
     * Select all content on canvas based on layer filters.
     * @param model Model to query for content
     */
    void SelectAll(const Model& model);
    
    /**
     * Populate selection from screen rectangle.
     * Converts screen coords to tile coords and collects selected content.
     * @param model Model to query for content
     * @param canvas Canvas for coordinate conversion
     */
    void PopulateSelectionFromRect(
        const Model& model,
        const Canvas& canvas
    );
    
    /**
     * Delete selected content (tiles, edges, markers).
     * Creates a compound undo command.
     * @param model Model to modify
     * @param history History for undo
     */
    void DeleteSelection(Model& model, History& history);
    
    /**
     * Copy current selection to clipboard.
     * Stores content with relative positions.
     * @param model Model to read from
     */
    void CopySelection(const Model& model);
    
    /**
     * Paste clipboard content at specified position.
     * @param model Model to modify
     * @param history History for undo
     * @param targetX Target tile X position
     * @param targetY Target tile Y position
     */
    void PasteClipboard(
        Model& model, 
        History& history,
        int targetX, 
        int targetY
    );
    
    /**
     * Enter paste preview mode.
     */
    void EnterPasteMode();
    
    /**
     * Exit paste preview mode without pasting.
     */
    void ExitPasteMode();
    
    /**
     * Enter floating selection mode (lifts content for preview).
     * Content is visually removed from canvas but model unchanged until commit.
     */
    void EnterFloatingMode();
    
    /**
     * Commit floating selection at current offset position.
     * Actually modifies the model: deletes from origin, places at new position.
     * @param model Model to modify
     * @param history History for undo
     */
    void CommitFloatingSelection(Model& model, History& history);
    
    /**
     * Cancel floating selection and return to normal selection state.
     * No model changes occur.
     */
    void CancelFloatingSelection();
    
    /**
     * Move selection by offset (for nudge/drag).
     * @param model Model to modify
     * @param history History for undo
     * @param dx X offset in tiles
     * @param dy Y offset in tiles
     */
    void MoveSelection(Model& model, History& history, int dx, int dy);
    
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
    
    // Custom cursors for tools
    SDL_Cursor* m_eyedropperCursor = nullptr;
    SDL_Cursor* m_zoomCursor = nullptr;
    SDL_Cursor* m_defaultCursor = nullptr;
    bool m_cursorsInitialized = false;
    Tool m_lastCursorTool = Tool::Move;  // Track tool for cursor changes
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

