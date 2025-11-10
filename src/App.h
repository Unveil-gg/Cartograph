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
    
    // File operations
    void NewProject();
    void OpenProject(const std::string& path);
    void SaveProject();
    void SaveProjectAs(const std::string& path);
    void ExportPng(const std::string& path);
    
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
    std::string m_currentFilePath;
    
    // Autosave
    double m_lastEditTime;
    double m_lastAutosaveTime;
    const double AUTOSAVE_DEBOUNCE = 5.0;   // 5 seconds after edit
    const double AUTOSAVE_INTERVAL = 30.0;  // Or every 30 seconds
    
    // Frame timing
    double m_lastFrameTime;
};

} // namespace Cartograph

