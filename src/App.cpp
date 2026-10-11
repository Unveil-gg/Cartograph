#include "App.h"
#include "render/GlRenderer.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include "platform/Time.h"
#include "IOJson.h"
#include "Package.h"
#include "ProjectFolder.h"
#include "ExportPng.h"
#include "Thumbnail.h"
#include "Preferences.h"
#include <algorithm>
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include <filesystem>

// SDL resource deleters implementation (outside namespace)
void SDL_WindowDeleter::operator()(SDL_Window* window) const {
    if (window) {
        SDL_DestroyWindow(window);
    }
}

void SDL_GLContextDeleter::operator()(SDL_GLContextState* context) const {
    if (context) {
        SDL_GL_DestroyContext(context);
    }
}

namespace Cartograph {


App::App()
    : m_running(false)
    , m_appState(AppState::Welcome)
    , m_lastEditTime(0)
    , m_lastAutosaveTime(0)
    , m_lastFrameTime(0)
    , m_hasDroppedFile(false)
    , m_isDragging(false)
    , m_hasAutosaveRecovery(false)
    , m_autosaveEnabled(true)
    , m_lastDirtyState(false)
{
}

App::~App() {
    Shutdown();
}


bool App::Init(const std::string& title, int width, int height) {
    // Initialize SDL (SDL3 returns bool, not int)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
    }
    
    // OpenGL 3.3 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 
        SDL_GL_CONTEXT_PROFILE_CORE);
    
    // HiDPI support
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // Create window
    m_window.reset(SDL_CreateWindow(
        title.c_str(),
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    ));
    
    if (!m_window) {
        return false;
    }
    
    // Lock aspect ratio to 16:9 to prevent warping during resize
    float aspectRatio = 16.0f / 9.0f;
    SDL_SetWindowAspectRatio(m_window.get(), aspectRatio, aspectRatio);
    
    // Minimum size to prevent unusably small UI (maintains 16:9)
    SDL_SetWindowMinimumSize(m_window.get(), 1152, 648);
    
    // Create OpenGL context
    m_glContext.reset(SDL_GL_CreateContext(m_window.get()));
    if (!m_glContext) {
        return false;
    }
    
    SDL_GL_MakeCurrent(m_window.get(), m_glContext.get());
    SDL_GL_SetSwapInterval(1);  // Enable VSync
    
    // Initialize GLAD (cross-platform OpenGL loader)
    int gladVersion = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (gladVersion == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize GLAD");
        return false;
    }
    
    // Initialize renderer (passes non-owning pointer)
    m_renderer = std::make_unique<GlRenderer>(m_window.get());
    
    // Initialize ImGui
    SetupImGui();
    
    // Theme will be initialized after preferences are loaded
    
    // Load icons from assets
    std::string assetsDir = Platform::GetAssetsDir();
    m_icons.LoadFromDirectory(assetsDir + "icons/", "marker", true);
    m_icons.LoadFromDirectory(assetsDir + "tools/", "tool", false);
    m_icons.BuildAtlas();
    
    // Setup UI (dockspace will be set up when entering editor)
    m_ui.SetupDockspace();
    
    // Initialize native menu (must be after SDL_Init)
    m_ui.InitializeNativeMenu();
    
    // Start background job queue
    m_jobs.Start();
    
    m_running = true;
    m_appState = AppState::Welcome;
    m_lastFrameTime = Platform::GetTime();
    
    // Ensure default directories exist
    std::string defaultProjects = Platform::GetDefaultProjectsDir();
    Platform::EnsureDirectoryExists(defaultProjects);
    
    std::string userDataDir = Platform::GetUserDataDir();
    Platform::EnsureDirectoryExists(userDataDir);
    
    // Load user preferences
    Preferences::Load();
    
    // Initialize theme from preferences
    m_model.InitDefaultTheme(Preferences::themeName);
    m_model.theme.uiScale = Preferences::uiScale;
    ApplyTheme(m_model.theme);
    
    // Load recent projects for welcome screen
    m_ui.m_welcomeScreen.LoadRecentProjects();
    
    // Check for autosave recovery
    CheckAutosaveRecovery();
    if (m_hasAutosaveRecovery) {
        m_ui.m_modals.showAutosaveRecoveryModal = true;
    }
    
    return true;
}


SDL_AppResult App::Iterate() {
    if (!m_running) {
        return SDL_APP_SUCCESS;
    }
    
    double currentTime = Platform::GetTime();
    float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
    m_lastFrameTime = currentTime;
    
    Update(deltaTime);
    Render();
    
    // Process job callbacks
    m_jobs.ProcessCallbacks();
    
    // Autosave check
    DoAutosave();
    
    return SDL_APP_CONTINUE;
}


SDL_AppResult App::HandleEvent(SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
    
    switch (event->type) {
        case SDL_EVENT_QUIT:
            RequestQuit();
            break;
            
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event->window.windowID == SDL_GetWindowID(m_window.get())) {
                RequestQuit();
            }
            break;
            
        case SDL_EVENT_DROP_BEGIN:
            m_isDragging = true;
            break;
            
        case SDL_EVENT_DROP_FILE:
            m_isDragging = false;
            if (event->drop.data) {
                m_droppedFilePath = event->drop.data;
                m_hasDroppedFile = true;
            }
            break;
            
        case SDL_EVENT_DROP_COMPLETE:
            m_isDragging = false;
            break;
    }
    
    return SDL_APP_CONTINUE;
}


void App::Shutdown() {
    if (!m_window) return;
    
    // Disable autosave to prevent race condition with cleanup
    m_autosaveEnabled = false;
    
    // Handle autosave files based on shutdown state
    if (m_model.dirty) {
        // Unclean shutdown - save metadata so recovery modal shows on restart
        SaveAutosaveMetadata();
    } else {
        // Clean shutdown - user saved, ensure autosave files are removed
        CleanupAutosave();
    }
    
    m_jobs.Stop();
    
    // Clean up IconManager before ImGui/GL shutdown to prevent memory
    // corruption from destroying GL context with live texture references
    m_icons.Clear();
    
    ShutdownImGui();
    
    // SDL resources automatically cleaned up by unique_ptr deleters
    // in proper order: context destroyed before window
    m_glContext.reset();
    m_window.reset();
    
    SDL_Quit();
}


void App::RequestQuit() {
    // Check for unsaved changes
    if (m_model.dirty) {
        // Show confirmation modal instead of quitting immediately
        m_ui.m_modals.showQuitConfirmationModal = true;
    } else {
        // No unsaved changes, quit immediately
        m_running = false;
    }
}


void App::ForceQuit() {
    // Force quit without checking for unsaved changes
    m_running = false;
}


void App::ShowWelcomeScreen() {
    m_appState = AppState::Welcome;
    
    // Load recent projects list
    m_ui.m_welcomeScreen.LoadRecentProjects();
}


void App::ShowEditor() {
    m_appState = AppState::Editor;
    
    // Clean up welcome screen resources
    m_ui.m_welcomeScreen.UnloadThumbnailTextures();
    
    // Ensure model is initialized if not already
    if (m_model.palette.empty()) {
        m_model.InitDefaults();
        m_keymap.LoadBindings(m_model.keymap);
    }
    
    // No need for world room anymore - regions are inferred from walls
    // Tiles can be painted anywhere on the grid
}



void App::Update(float deltaTime) {
    // Update canvas
    m_canvas.Update(m_model, deltaTime);
    
    // Track dirty state for autosave
    if (m_model.dirty) {
        m_lastEditTime = Platform::GetTime();
    }
    
    // Update window title if dirty state changed
    if (m_model.dirty != m_lastDirtyState) {
        m_lastDirtyState = m_model.dirty;
        UpdateWindowTitle();
    }
}


void App::Render() {
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    
    // Clear background
    m_renderer->SetRenderTarget(nullptr);
    Color bgColor = (m_appState == AppState::Welcome) 
        ? Color(0.1f, 0.1f, 0.12f, 1.0f)  // Darker for welcome
        : m_model.theme.background;
    m_renderer->Clear(bgColor);
    
    // Handle dropped files
    if (m_hasDroppedFile) {
        m_ui.HandleDroppedFile(m_droppedFilePath, *this, m_jobs, m_icons);
        m_hasDroppedFile = false;
        m_droppedFilePath.clear();
    }
    
    // Update menu state every frame (works in both Welcome and Editor states)
    m_ui.UpdateMenu(*this, m_model, m_canvas, m_history, m_icons, m_jobs);
    
    // Render UI based on state
    if (m_appState == AppState::Welcome) {
        m_ui.m_welcomeScreen.Render(*this, m_model, m_canvas, m_history, m_jobs, m_icons, m_keymap);
    } else {
        m_ui.Render(*this, *m_renderer, m_model, m_canvas, m_history, 
                    m_icons, m_jobs, m_keymap, 0.016f);
    }
    
    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Capture thumbnail AFTER ImGui has rendered to framebuffer
    // (uses previous frame's pixels - 16ms delay is imperceptible)
    // Skip capture when modals or toasts are visible to avoid UI overlays
    bool modalVisible = m_ui.m_modals.AnyModalVisible() || m_ui.HasVisibleToasts();
    
    // Capture periodically in editor mode (not just when dirty)
    // This ensures thumbnails are available after loading or saving
    if (m_appState == AppState::Editor && !modalVisible) {
        static double lastThumbnailCapture = 0.0;
        double now = Platform::GetTime();
        // Capture every 3 seconds, or immediately if dirty and never captured
        bool shouldCapture = (now - lastThumbnailCapture > 3.0) ||
                            (m_model.dirty && !m_canvas.hasCachedThumbnail);
        if (shouldCapture) {
            m_canvas.CaptureThumbnail(
                *m_renderer, m_model,
                m_canvas.GetViewportX(),
                m_canvas.GetViewportY(),
                m_canvas.GetViewportW(),
                m_canvas.GetViewportH()
            );
            lastThumbnailCapture = now;
        }
    }
    
    // Swap buffers
    SDL_GL_SwapWindow(m_window.get());
}

} // namespace Cartograph

