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
    int cellWidth = 16;   // cell width in pixels
    int cellHeight = 16;  // cell height in pixels
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
     * @param deltaTime Frame delta time
     */
    void Render(
        IRenderer& renderer,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
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
    
    // UI state
    bool showExportModal = false;
    ExportOptions exportOptions;
    bool showSettingsModal = false;
    
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
        Move, Select, Paint, Erase, Fill, Rectangle, Door, Marker, Eyedropper
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
    
private:
    void RenderMenuBar(Model& model, Canvas& canvas, History& history);
    void RenderPalettePanel(Model& model);
    void RenderToolsPanel(Model& model);
    void RenderPropertiesPanel(Model& model);
    void RenderCanvasPanel(
        IRenderer& renderer,
        Model& model, 
        Canvas& canvas, 
        History& history
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
    
    std::vector<Toast> m_toasts;
    float m_statusBarHeight = 24.0f;
    bool m_layoutInitialized = false;
    
    // Status bar error message
    std::string m_statusError;
    float m_statusErrorTime = 0.0f;
};

} // namespace Cartograph

