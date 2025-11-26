#pragma once

#include "Model.h"
#include "ExportPng.h"
#include "History.h"
#include <string>
#include <vector>

// Forward declarations
typedef unsigned int ImGuiID;

namespace Cartograph {

class Canvas;
class IconManager;
class App;
class JobQueue;

/**
 * Toast notification.
 */
struct Toast {
    std::string message;
    float remainingTime;
    enum class Type {
        Info, Success, Warning, Error
    } type;
};

/**
 * New project configuration.
 */
struct NewProjectConfig {
    char projectName[256] = "New Map";
    GridPreset gridPreset = GridPreset::Square;  // Cell type (Square or Rectangle)
    int mapWidth = 256;   // in cells
    int mapHeight = 256;  // in cells
};

/**
 * Project template presets.
 */
enum class ProjectTemplate {
    Custom,      // User-defined settings
    Small,       // 128x128, 16px cells
    Medium,      // 256x256, 16px cells
    Large,       // 512x512, 16px cells
    Metroidvania // 256x256, 8px cells (detailed)
};

/**
 * Recent project entry.
 */
struct RecentProject {
    std::string path;
    std::string name;
    std::string lastModified;
    // TODO: Add thumbnail support
};

/**
 * UI manager.
 * Handles all ImGui panels and windows.
 */
class UI {
public:
    UI();
    
    /**
     * Setup ImGui docking layout (call once at startup).
     */
    void SetupDockspace();
    
    /**
     * Render all UI panels (editor mode).
     * @param renderer Renderer for drawing canvas
     * @param model Current model
     * @param canvas Canvas
     * @param history History
     * @param icons Icon manager
     * @param jobs Job queue for background tasks
     * @param deltaTime Frame delta time
     */
    void Render(
        IRenderer& renderer,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        JobQueue& jobs,
        float deltaTime
    );
    
    /**
     * Render welcome screen.
     * @param app Application instance
     * @param model Current model
     */
    void RenderWelcomeScreen(App& app, Model& model);
    
    /**
     * Show a toast notification.
     * @param message Message text
     * @param type Toast type
     * @param duration Duration in seconds
     */
    void ShowToast(
        const std::string& message, 
        Toast::Type type = Toast::Type::Info,
        float duration = 3.0f
    );
    
    /**
     * Import a custom icon.
     * @param iconManager Icon manager to add icon to
     * @param jobs Job queue for background processing
     */
    void ImportIcon(IconManager& iconManager, JobQueue& jobs);
    
    // UI state
    bool showExportModal = false;
    ExportOptions exportOptions;
    bool showSettingsModal = false;
    
    // Icon import state
    bool isImportingIcon = false;
    std::string importingIconName;
    
    // Welcome screen state
    bool showNewProjectModal = false;
    NewProjectConfig newProjectConfig;
    ProjectTemplate selectedTemplate = ProjectTemplate::Medium;
    std::vector<RecentProject> recentProjects;
    bool showWhatsNew = false;
    
    // Selected palette tile
    int selectedTileId = 1;
    
    // Current tool
    enum class Tool {
        Move, Select, Paint, Erase, Fill, Marker, Eyedropper
    } currentTool = Tool::Move;
    
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
    
    // Room management state
    std::string selectedRoomId;   // Currently selected room
    bool roomPaintMode = false;   // Room painting mode active
    bool showRoomOverlays = true; // Show room visual overlays
    bool showNewRoomDialog = false;
    char newRoomName[64] = "New Room";
    float newRoomColor[3] = {1.0f, 0.5f, 0.5f};
    
    // Room assignment state (for room paint mode)
    bool isPaintingRoomCells = false;
    int lastRoomPaintX = -1;
    int lastRoomPaintY = -1;
    std::vector<ModifyRoomAssignmentsCommand::CellAssignment> 
        currentRoomAssignments;
    
private:
    void RenderMenuBar(
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        JobQueue& jobs
    );
    void RenderPalettePanel(Model& model);
    void RenderToolsPanel(Model& model);
    void RenderPropertiesPanel(Model& model, IconManager& icons, JobQueue& jobs);
    void RenderCanvasPanel(
        IRenderer& renderer,
        Model& model, 
        Canvas& canvas, 
        History& history,
        IconManager& icons
    );
    void RenderStatusBar(Model& model, Canvas& canvas);
    void RenderToasts(float deltaTime);
    void RenderExportModal(Model& model, Canvas& canvas);
    void RenderSettingsModal(Model& model);
    
    // Welcome screen components
    void RenderNewProjectModal(App& app, Model& model);
    void RenderRecentProjectsList(App& app);
    void RenderProjectTemplates();
    void RenderWhatsNewPanel();
    void ApplyTemplate(ProjectTemplate tmpl);
    void LoadRecentProjects();
    void AddRecentProject(const std::string& path);
    
    /**
     * Build the fixed docking layout (called once at startup).
     * @param dockspaceId ImGui dockspace ID
     */
    void BuildFixedLayout(ImGuiID dockspaceId);
    
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
    
    std::vector<Toast> m_toasts;
    float m_statusBarHeight = 24.0f;
    bool m_layoutInitialized = false;
    
    // Status bar error message
    std::string m_statusError;
    float m_statusErrorTime = 0.0f;
};

} // namespace Cartograph

