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

// SDL includes for callback types
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

// Forward declarations
struct SDL_Window;

// SDL_GLContext is defined by SDL3, just forward declare the struct
struct SDL_GLContextState;
typedef struct SDL_GLContextState* SDL_GLContext;

// Custom deleters for SDL resources (RAII)
struct SDL_WindowDeleter {
    void operator()(SDL_Window* window) const;
};

struct SDL_GLContextDeleter {
    void operator()(SDL_GLContextState* context) const;
};

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
     * Per-frame iteration (called by SDL3 main callbacks).
     * @return SDL_APP_CONTINUE to keep running, SDL_APP_SUCCESS to quit
     */
    SDL_AppResult Iterate();
    
    /**
     * Handle a single SDL event (called by SDL3 main callbacks).
     * @param event The SDL event to process
     * @return SDL_APP_CONTINUE to keep running, SDL_APP_SUCCESS to quit
     */
    SDL_AppResult HandleEvent(SDL_Event* event);
    
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
     * Get current file path (empty if untitled).
     */
    const std::string& GetCurrentFilePath() const { 
        return m_currentFilePath; 
    }
    
    /**
     * Set current file path (used during autosave recovery).
     */
    void SetCurrentFilePath(const std::string& path) {
        m_currentFilePath = path;
        UpdateWindowTitle();
    }
    
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
    
    // File dialogs
    void ShowNewProjectDialog();
    void ShowOpenProjectDialog();
    
    // Window management
    void UpdateWindowTitle();
    
    /**
     * Apply theme to ImGui style.
     * Call after changing model.theme to update UI appearance.
     */
    void ApplyTheme(const Theme& theme);
    
private:
    void Update(float deltaTime);
    void Render();
    
    void SetupImGui();
    void ShutdownImGui();
    
    void StartAutosave();
    void StopAutosave();
    void DoAutosave();
    
    // Autosave recovery
    void CheckAutosaveRecovery();
    void SaveAutosaveMetadata();
    void CleanupAutosave();
    
    // SDL window and OpenGL context (owned via RAII smart pointers)
    std::unique_ptr<SDL_Window, SDL_WindowDeleter> m_window;
    std::unique_ptr<SDL_GLContextState, SDL_GLContextDeleter> m_glContext;
    
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
    bool m_autosaveEnabled;                // Disabled during shutdown
    
    // Frame timing
    double m_lastFrameTime;
    
    // File drop handling
    std::string m_droppedFilePath;
    bool m_hasDroppedFile;
    bool m_isDragging;  // True when file is being dragged over window
};

} // namespace Cartograph

