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
 * Console message type.
 */
enum class MessageType {
    Info,
    Success,
    Warning,
    Error
};

/**
 * Console message entry.
 */
struct ConsoleMessage {
    std::string message;
    MessageType type;
    double timestamp;  // When message was created
    
    ConsoleMessage(const std::string& msg, MessageType t, double ts)
        : message(msg), type(t), timestamp(ts) {}
};

/**
 * Legacy toast notification (deprecated, keeping for compatibility).
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
    std::string saveDirectory = "";  // Directory where project will be saved
    std::string fullSavePath = "";   // Full path: {saveDirectory}/{projectName}/
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
    std::string thumbnailPath;      // Path to thumbnail image file
    unsigned int thumbnailTextureId; // OpenGL texture ID (0 if not loaded)
    bool thumbnailLoaded;            // Whether texture has been loaded
    
    RecentProject() 
        : thumbnailTextureId(0), thumbnailLoaded(false) {}
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
     * @param app Application instance
     * @param renderer Renderer for drawing canvas
     * @param model Current model
     * @param canvas Canvas
     * @param history History
     * @param icons Icon manager
     * @param jobs Job queue for background tasks
     * @param deltaTime Frame delta time
     */
    void Render(
        App& app,
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
     * Show a message in the console.
     * @param message Message text
     * @param type Message type
     */
    void ShowToast(
        const std::string& message, 
        Toast::Type type = Toast::Type::Info,
        float duration = 3.0f
    );
    
    /**
     * Add a message to the console.
     * @param message Message text
     * @param type Message type
     */
    void AddConsoleMessage(const std::string& message, MessageType type);
    
    /**
     * Import a custom icon.
     * @param iconManager Icon manager to add icon to
     * @param jobs Job queue for background processing
     */
    void ImportIcon(IconManager& iconManager, JobQueue& jobs);
    
    /**
     * Show save dialog for project folder format.
     * @param app App instance to save through
     */
    void ShowSaveProjectDialog(App& app);
    
    /**
     * Show save dialog for package export (.cart).
     * @param app App instance to export through
     */
    void ShowExportPackageDialog(App& app);
    
    /**
     * Show save dialog for PNG export.
     * @param app App instance to export through
     */
    void ShowExportPngDialog(App& app);
    
    /**
     * Handle dropped file (from OS drag-drop).
     * @param filePath Path to dropped file
     */
    void HandleDroppedFile(const std::string& filePath);
    
    /**
     * Load recent projects from persistent storage.
     */
    void LoadRecentProjects();
    
    /**
     * Add a project to the recent projects list.
     * @param path Project file or folder path
     */
    void AddRecentProject(const std::string& path);
    
    /**
     * Unload all thumbnail textures (cleanup when leaving welcome screen).
     */
    void UnloadThumbnailTextures();
    
    // UI state
    bool showExportModal = false;
    bool shouldShowExportPngDialog = false;
    ExportOptions exportOptions;
    bool showSettingsModal = false;
    int settingsModalSelectedTab = 1;  // 0=Project, 1=Grid&Canvas, 2=Keybindings
    
    // Icon import state
    bool isImportingIcon = false;
    std::string importingIconName;
    std::string droppedFilePath;
    bool hasDroppedFile = false;
    
    // Icon rename state
    bool showRenameIconModal = false;
    std::string renameIconOldName;
    char renameIconNewName[64] = "";
    
    // Welcome screen state
    bool showNewProjectModal = false;
    NewProjectConfig newProjectConfig;
    ProjectTemplate selectedTemplate = ProjectTemplate::Medium;
    std::vector<RecentProject> recentProjects;
    unsigned int placeholderTexture = 0;  // Placeholder texture for missing thumbnails
    bool showProjectBrowserModal = false;  // "View more" projects modal
    bool showWhatsNew = false;
    bool showAutosaveRecoveryModal = false;
    
    // Quit confirmation state
    bool showQuitConfirmationModal = false;
    
    // Panel visibility
    bool showPropertiesPanel = false;  // Toggleable via View menu
    
    // Selected palette tile
    int selectedTileId = 1;
    
    // Hovered tile coordinates (for status bar)
    int hoveredTileX = -1;
    int hoveredTileY = -1;
    bool isHoveringCanvas = false;
    
    // Current tool
    enum class Tool {
        Move, Select, Paint, Fill, Erase, Marker, Eyedropper
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
        App& app,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        JobQueue& jobs
    );
    void RenderPalettePanel(Model& model);
    void RenderToolsPanel(Model& model, IconManager& icons, JobQueue& jobs);
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
    void RenderRenameIconModal(Model& model, IconManager& icons);
    
    // Welcome screen components
    void RenderNewProjectModal(App& app, Model& model);
    void RenderRecentProjectsList(App& app);
    void RenderProjectBrowserModal(App& app);
    void RenderProjectTemplates();
    void RenderWhatsNewPanel();
    void RenderAutosaveRecoveryModal(App& app, Model& model);
    void RenderQuitConfirmationModal(App& app, Model& model);
    void ApplyTemplate(ProjectTemplate tmpl);
    void LoadThumbnailTexture(RecentProject& project);
    unsigned int GeneratePlaceholderTexture();
    void ShowNewProjectFolderPicker();
    void UpdateNewProjectPath();
    
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
    
    std::vector<Toast> m_toasts;  // Legacy, will be removed
    std::vector<ConsoleMessage> m_consoleMessages;
    static constexpr size_t MAX_CONSOLE_MESSAGES = 100;
    float m_statusBarHeight = 24.0f;
    bool m_layoutInitialized = false;
    
    // Status bar error message
    std::string m_statusError;
    float m_statusErrorTime = 0.0f;
};

} // namespace Cartograph

