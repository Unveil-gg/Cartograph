#pragma once

#include "Model.h"
#include "ExportPng.h"
#include "History.h"
#include "UI/CanvasPanel.h"
#include "UI/Modals.h"
#include <string>
#include <vector>
#include <atomic>
#include <memory>

// Forward declarations
typedef unsigned int ImGuiID;
struct SDL_Cursor;

// Custom deleter for SDL_Cursor (RAII)
struct SDL_CursorDeleter {
    void operator()(SDL_Cursor* cursor) const;
};

namespace Cartograph {

class Canvas;
class IconManager;
class App;
class JobQueue;
class KeymapManager;

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
    ~UI();
    
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
     * @param keymap Keymap manager for input handling
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
        KeymapManager& keymap,
        float deltaTime
    );
    
    /**
     * Render welcome screen.
     * @param app Application instance
     * @param model Current model
     * @param jobs Job queue for async operations
     * @param icons Icon manager
     */
    void RenderWelcomeScreen(App& app, Model& model, Canvas& canvas,
                            History& history, JobQueue& jobs,
                            IconManager& icons, KeymapManager& keymap);
    
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
     * @param app Application instance for state and project operations
     * @param jobs Job queue for async loading
     * @param icons Icon manager for loading project icons
     */
    void HandleDroppedFile(const std::string& filePath, App& app,
                          JobQueue& jobs, IconManager& icons);
    
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
    
    /**
     * Load thumbnail texture for a project (called by Modals).
     */
    void LoadThumbnailTexture(RecentProject& project);
    
    // Icon import state
    bool isImportingIcon = false;
    std::string importingIconName;
    std::string droppedFilePath;
    bool hasDroppedFile = false;
    
    // Welcome screen state (non-modal components)
    std::vector<RecentProject> recentProjects;
    unsigned int placeholderTexture = 0;  // Placeholder texture for missing thumbnails
    
    // Panel visibility
    bool showPropertiesPanel = false;  // Toggleable via View menu
    
    // Canvas panel (contains all canvas-related state and rendering)
    CanvasPanel m_canvasPanel;
    
    // Modals manager (contains all modal dialog state and rendering)
    Modals m_modals;
    
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
    void RenderToolsPanel(Model& model, History& history, IconManager& icons, 
                         JobQueue& jobs);
    void RenderPropertiesPanel(Model& model, IconManager& icons, JobQueue& jobs);
    void RenderStatusBar(Model& model, Canvas& canvas);
    void RenderToasts(float deltaTime);
    
    // Welcome screen components (non-modal)
    void RenderRecentProjectsList(App& app);
    unsigned int GeneratePlaceholderTexture();
    
    /**
     * Build the fixed docking layout (called once at startup).
     * @param dockspaceId ImGui dockspace ID
     */
    void BuildFixedLayout(ImGuiID dockspaceId);
    
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

