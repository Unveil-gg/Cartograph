#pragma once

#include "Model.h"
#include "Canvas.h"
#include "UI.h"
#include "History.h"
#include "Icons.h"
#include "Jobs.h"
#include "Keymap.h"
#include <memory>
#include <string>

// Forward declarations
struct SDL_Window;

// SDL_GLContext is defined by SDL3, just forward declare the struct
struct SDL_GLContextState;
typedef struct SDL_GLContextState* SDL_GLContext;

namespace Cartograph {

class IRenderer;
class GlRenderer;

/**
 * Application state.
 */
enum class AppState {
    Welcome,  // Welcome screen with project creation/import
    Editor    // Main map editor
};

/**
 * Main application class.
 * Manages the application lifecycle, window, and main systems.
 */
class App {
public:
    App();
    ~App();
    
    /**
     * Initialize the application.
     * @param title Window title
     * @param width Window width
     * @param height Window height
     * @return true on success
     */
    bool Init(const std::string& title, int width, int height);
    
    /**
     * Run the main loop.
     * @return Exit code
     */
    int Run();
    
    /**
     * Shutdown and cleanup.
     */
    void Shutdown();
    
    /**
     * Request application exit.
     */
    void RequestQuit();
    
    /**
     * Force application exit (bypassing unsaved changes check).
     */
    void ForceQuit();
    
    /**
     * Transition to welcome screen.
     */
    void ShowWelcomeScreen();
    
    /**
     * Transition to editor (creating project if needed).
     */
    void ShowEditor();
    
    /**
     * Get current application state.
     */
    AppState GetState() const { return m_appState; }
    
    /**
     * Get current model (read-only).
     */
    const Model& GetModel() const { return m_model; }
    
    /**
     * Check if a file is currently being dragged over the window.
     */
    bool IsDragging() const { return m_isDragging; }
    
    // File operations
    void NewProject(const std::string& savePath = "");
    void OpenProject(const std::string& path);
    void SaveProject();
    void SaveProjectAs(const std::string& path);
    void SaveProjectFolder(const std::string& folderPath);
    void ExportPackage(const std::string& cartPath);
    void ExportPng(const std::string& path);
    
    // Window management
    void UpdateWindowTitle();
    
private:
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();
    
    void SetupImGui();
    void ShutdownImGui();
    void ApplyTheme(const Theme& theme);
    
    void StartAutosave();
    void StopAutosave();
    void DoAutosave();
    
    // Autosave recovery
    void CheckAutosaveRecovery();
    void SaveAutosaveMetadata();
    void CleanupAutosave();
    
    // SDL window and OpenGL context
    SDL_Window* m_window;
    SDL_GLContext m_glContext;
    
    // Core systems
    std::unique_ptr<GlRenderer> m_renderer;
    Model m_model;
    Canvas m_canvas;
    UI m_ui;
    History m_history;
    IconManager m_icons;
    JobQueue m_jobs;
    KeymapManager m_keymap;
    
    // Application state
    bool m_running;
    AppState m_appState;
    std::string m_currentFilePath;
    bool m_lastDirtyState;  // Track dirty state for window title updates
    
    // Autosave
    double m_lastEditTime;
    double m_lastAutosaveTime;
    const double AUTOSAVE_DEBOUNCE = 5.0;   // 5 seconds after edit
    const double AUTOSAVE_INTERVAL = 30.0;  // Or every 30 seconds
    bool m_hasAutosaveRecovery;            // Autosave available for recovery
    
    // Frame timing
    double m_lastFrameTime;
    
    // File drop handling
    std::string m_droppedFilePath;
    bool m_hasDroppedFile;
    bool m_isDragging;  // True when file is being dragged over window
};

} // namespace Cartograph

