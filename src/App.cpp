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

void App::SetupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable docking (but we'll lock the layout)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Setup platform/renderer backends
    ImGui_ImplSDL3_InitForOpenGL(m_window.get(), m_glContext.get());
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Apply theme
    ApplyTheme(m_model.theme);
}

void App::ShutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void App::ApplyTheme(const Theme& theme) {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Apply base ImGui theme based on brightness
    if (theme.name == "Print-Light") {
        ImGui::StyleColorsLight();
    } else {
        // Dark, Loud-Yellow, and Unveil all use dark base
        ImGui::StyleColorsDark();
    }
    
    // Apply custom accent colors for themed variants
    if (theme.name == "Loud-Yellow") {
        // Yellow accent colors for UI elements
        style.Colors[ImGuiCol_Header] = ImVec4(0.75f, 0.60f, 0.15f, 0.6f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.85f, 0.70f, 0.20f, 0.8f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.80f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.55f, 0.45f, 0.12f, 0.6f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.60f, 0.15f, 0.8f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.80f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.16f, 0.08f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.24f, 0.10f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.38f, 0.32f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.10f, 0.05f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.24f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.35f, 0.28f, 0.10f, 0.8f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.75f, 0.60f, 0.15f, 0.9f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.55f, 0.45f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.92f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.70f, 0.15f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.85f, 0.20f, 1.0f);
    } else if (theme.name == "Unveil") {
        // Purple accent colors for UI elements
        style.Colors[ImGuiCol_Header] = ImVec4(0.42f, 0.32f, 0.52f, 0.6f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.55f, 0.40f, 0.70f, 0.8f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.68f, 0.50f, 0.88f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.32f, 0.22f, 0.42f, 0.6f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.32f, 0.58f, 0.8f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.40f, 0.78f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.08f, 0.16f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.12f, 0.24f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.16f, 0.32f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.05f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.10f, 0.22f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.22f, 0.15f, 0.30f, 0.8f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.50f, 0.35f, 0.65f, 0.9f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.38f, 0.25f, 0.50f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.80f, 0.90f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.55f, 0.40f, 0.72f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.50f, 0.90f, 1.0f);
    } else if (theme.name == "Aeterna") {
        // Violet/gold accent colors (Aeterna Noctis inspired)
        style.Colors[ImGuiCol_Header] = ImVec4(0.35f, 0.18f, 0.55f, 0.6f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.48f, 0.25f, 0.75f, 0.8f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.61f, 0.30f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.28f, 0.14f, 0.45f, 0.6f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.22f, 0.65f, 0.8f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.55f, 0.28f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.05f, 0.12f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.08f, 0.20f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.12f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.03f, 0.08f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.07f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.10f, 0.32f, 0.8f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.45f, 0.22f, 0.70f, 0.9f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.32f, 0.16f, 0.50f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.70f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);
    } else if (theme.name == "Hornet") {
        // Crimson/bone accent colors (Hollow Knight: Silksong inspired)
        style.Colors[ImGuiCol_Header] = ImVec4(0.55f, 0.12f, 0.12f, 0.6f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.72f, 0.16f, 0.16f, 0.8f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.91f, 0.19f, 0.19f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.45f, 0.10f, 0.10f, 0.6f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.62f, 0.14f, 0.14f, 0.8f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.18f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.10f, 0.10f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.14f, 0.14f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.18f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.06f, 0.06f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.10f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.32f, 0.10f, 0.10f, 0.8f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.65f, 0.15f, 0.15f, 0.9f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.48f, 0.12f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.93f, 0.91f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.78f, 0.13f, 0.13f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.91f, 0.19f, 0.19f, 1.0f);
    } else if (theme.name == "Soma") {
        // Steel blue/silver accent colors (Castlevania: Aria of Sorrow inspired)
        style.Colors[ImGuiCol_Header] = ImVec4(0.28f, 0.35f, 0.48f, 0.6f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.45f, 0.60f, 0.8f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.52f, 0.72f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.28f, 0.38f, 0.6f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.38f, 0.52f, 0.8f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.48f, 0.65f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.16f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.20f, 0.25f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.27f, 0.34f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.24f, 0.32f, 0.8f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.32f, 0.42f, 0.58f, 0.9f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.26f, 0.34f, 0.46f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.60f, 0.85f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.48f, 0.70f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.60f, 0.85f, 1.0f);
    }
    
    // Apply UI scale
    style.ScaleAllSizes(theme.uiScale);
}

void App::DoAutosave() {
    if (!m_autosaveEnabled || !m_model.dirty) return;
    
    double now = Platform::GetTime();
    double timeSinceEdit = now - m_lastEditTime;
    double timeSinceAutosave = now - m_lastAutosaveTime;
    
    bool shouldAutosave = 
        (timeSinceEdit >= AUTOSAVE_DEBOUNCE) ||
        (timeSinceAutosave >= AUTOSAVE_INTERVAL);
    
    if (shouldAutosave) {
        std::string autosaveDir = Platform::GetAutosaveDir();
        Platform::EnsureDirectoryExists(autosaveDir);
        
        std::string path = autosaveDir + "autosave.json";
        if (IOJson::SaveToFile(m_model, path)) {
            m_lastAutosaveTime = now;
            SaveAutosaveMetadata();
            // Silent autosave - no UI feedback
        }
    }
}

void App::NewProject(
    const std::string& savePath,
    const std::string& projectName,
    GridPreset gridPreset,
    int mapWidth,
    int mapHeight
) {
    m_model = Model();
    m_model.InitDefaults();
    m_history.Clear();
    
    // Apply optional configuration (overrides defaults)
    if (!projectName.empty()) {
        m_model.meta.title = projectName;
    }
    
    // Apply grid preset and dimensions if specified
    m_model.ApplyGridPreset(gridPreset);
    if (mapWidth > 0) {
        m_model.grid.cols = mapWidth;
    }
    if (mapHeight > 0) {
        m_model.grid.rows = mapHeight;
    }
    
    // Load keymap bindings into keymap manager
    m_keymap.LoadBindings(m_model.keymap);
    
    // If save path provided, set it and save immediately
    if (!savePath.empty()) {
        // Ensure directory exists
        Platform::EnsureDirectoryExists(savePath);
        
        // Save project to the specified path
        m_currentFilePath = savePath;
        
        // Don't generate thumbnail on initial create - project is empty
        // Thumbnail will be generated on first manual save (Cmd+S)
        bool success = ProjectFolder::Save(m_model, savePath, &m_icons);
        
        if (success) {
            m_model.ClearDirty();
            UpdateWindowTitle();
            m_ui.m_welcomeScreen.AddRecentProject(savePath);
        } else {
            m_currentFilePath.clear();
        }
    } else {
        // No path provided - old behavior (untitled project)
        m_currentFilePath.clear();
        UpdateWindowTitle();
    }
}

void App::OpenProject(const std::string& path) {
    Model newModel;
    bool success = false;
    
    // Detect format and load accordingly
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cart") {
        // Load as .cart package (ZIP with embedded icons)
        success = Package::Load(path, newModel, &m_icons);
    } else if (ProjectFolder::IsProjectFolder(path)) {
        // Load as project folder (git-friendly format)
        success = ProjectFolder::Load(path, newModel, &m_icons);
    } else {
        m_ui.ShowToast("Unsupported format. Use .cart or project folder.", 
                      Toast::Type::Error);
        return;
    }
    
    if (success) {
        m_model = newModel;
        m_currentFilePath = path;
        m_history.Clear();
        UpdateWindowTitle();
        m_ui.m_welcomeScreen.AddRecentProject(path);
        
        // Apply global theme preferences (theme is global, not per-project)
        m_model.InitDefaultTheme(Preferences::themeName);
        m_model.theme.uiScale = Preferences::uiScale;
        ApplyTheme(m_model.theme);
        
        // Rebuild icon atlas (must be on main thread for OpenGL)
        m_icons.BuildAtlas();
        
        // Load keymap bindings into keymap manager
        m_keymap.LoadBindings(m_model.keymap);
        
        // Focus canvas on content bounds
        ContentBounds bounds = m_model.CalculateContentBounds();
        if (!bounds.isEmpty) {
            m_canvas.FocusOnRect(
                bounds.minX, bounds.minY, bounds.maxX, bounds.maxY,
                m_model.grid.tileWidth, m_model.grid.tileHeight
            );
        }
        
        // Extract project name from path for console message
        std::string projectName = std::filesystem::path(path)
            .filename().string();
        if (projectName.empty()) {
            projectName = std::filesystem::path(path)
                .parent_path().filename().string();
        }
        m_ui.ShowToast("Opened: " + projectName, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to open: " + path, Toast::Type::Error);
    }
}

void App::ShowNewProjectDialog() {
    // Callback struct
    struct CallbackData {
        App* app;
    };
    
    // Allocate callback data (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->app = this;
    
    // Show native folder picker dialog for save location
    SDL_ShowOpenFolderDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                return;
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;
            }
            
            // User selected a folder - create new project there
            std::string folderPath = filelist[0];
            data->app->NewProject(folderPath);
            data->app->ShowEditor();
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Default location
        nullptr,
        // Allow multiple folders
        false
    );
}

void App::ShowOpenProjectDialog() {
    // Use platform-native dialog that supports both files and folders
    auto result = Platform::ShowOpenDialogForImport(
        "Open Cartograph Project",
        true,   // Allow .cart files
        true,   // Allow .cartproj folders
        {"cart"},  // File extensions filter
        Platform::GetDefaultProjectsDir()  // Default directory
    );
    
    if (result) {
        OpenProject(*result);
        ShowEditor();
    }
    // If nullopt, user cancelled - no action needed
}

void App::SaveProject() {
    if (m_currentFilePath.empty()) {
        // No file path yet - show save dialog for new/untitled projects
        m_ui.ShowSaveProjectDialog(*this);
        return;
    }
    
    SaveProjectAs(m_currentFilePath);
}

void App::SaveProjectAs(const std::string& path) {
    bool success = false;
    
    // Use cached thumbnail from last render (simpler and more reliable)
    std::vector<uint8_t> thumbnailPixels;
    int thumbWidth = 0, thumbHeight = 0;
    
    if (m_canvas.hasCachedThumbnail) {
        thumbnailPixels = m_canvas.cachedThumbnail;
        thumbWidth = m_canvas.cachedThumbnailWidth;
        thumbHeight = m_canvas.cachedThumbnailHeight;
    }
    
    // Detect save format and save accordingly
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cart") {
        // Save as .cart package (ZIP with embedded icons)
        success = Package::Save(m_model, path, &m_icons,
                               thumbnailPixels.empty() ? nullptr : 
                                   thumbnailPixels.data(),
                               thumbWidth, thumbHeight);
    } else {
        // Save as project folder (git-friendly format)
        success = ProjectFolder::Save(m_model, path, &m_icons,
                                     thumbnailPixels.empty() ? nullptr : 
                                         thumbnailPixels.data(),
                                     thumbWidth, thumbHeight);
    }
    
    if (success) {
        m_currentFilePath = path;
        m_model.ClearDirty();
        CleanupAutosave();  // Remove autosave after successful manual save
        UpdateWindowTitle();
        m_ui.m_welcomeScreen.AddRecentProject(path);
        
        // Extract project name from path for console message
        std::string projectName = std::filesystem::path(path)
            .filename().string();
        if (projectName.empty()) {
            projectName = std::filesystem::path(path)
                .parent_path().filename().string();
        }
        m_ui.ShowToast("Saved: " + projectName, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to save: " + path, Toast::Type::Error);
    }
}

void App::SaveProjectFolder(const std::string& folderPath) {
    // Use cached thumbnail from last render (simpler and more reliable)
    std::vector<uint8_t> thumbnailPixels;
    int thumbWidth = 0, thumbHeight = 0;
    
    if (m_canvas.hasCachedThumbnail) {
        thumbnailPixels = m_canvas.cachedThumbnail;
        thumbWidth = m_canvas.cachedThumbnailWidth;
        thumbHeight = m_canvas.cachedThumbnailHeight;
    }
    
    bool success = ProjectFolder::Save(m_model, folderPath, &m_icons,
                                      thumbnailPixels.empty() ? nullptr : 
                                          thumbnailPixels.data(),
                                      thumbWidth, thumbHeight);
    
    if (success) {
        m_currentFilePath = folderPath;
        m_model.ClearDirty();
        CleanupAutosave();
        UpdateWindowTitle();
        m_ui.m_welcomeScreen.AddRecentProject(folderPath);
        
        // Extract project name from path
        std::string projectName = std::filesystem::path(folderPath)
            .filename().string();
        if (projectName.empty()) {
            projectName = std::filesystem::path(folderPath)
                .parent_path().filename().string();
        }
        m_ui.ShowToast("Saved: " + projectName, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to save project folder: " + folderPath, 
                      Toast::Type::Error);
    }
}

bool App::RenameProjectFolder(const std::string& newTitle) {
    namespace fs = std::filesystem;
    
    // Must have a current file path (project folder)
    if (m_currentFilePath.empty()) {
        m_ui.ShowToast("No project folder to rename", Toast::Type::Error);
        return false;
    }
    
    // Only works for project folders, not .cart files
    if (m_currentFilePath.size() >= 5 && 
        m_currentFilePath.substr(m_currentFilePath.size() - 5) == ".cart") {
        m_ui.ShowToast("Cannot rename .cart files this way", 
                      Toast::Type::Error);
        return false;
    }
    
    // Sanitize the new title for filesystem use
    std::string sanitizedName = ProjectFolder::SanitizeProjectName(newTitle);
    
    // Preserve .cartproj extension if current path has it
    bool hasCartprojExt = ProjectFolder::HasCartprojExtension(m_currentFilePath);
    
    if (sanitizedName.empty()) {
        m_ui.ShowToast("Project name cannot be empty", Toast::Type::Error);
        return false;
    }
    
    // Explicitly strip trailing slashes from path string
    // (lexically_normal() doesn't reliably remove them on all platforms)
    std::string pathStr = m_currentFilePath;
    while (!pathStr.empty() && (pathStr.back() == '/' || pathStr.back() == '\\')) {
        pathStr.pop_back();
    }
    
    if (pathStr.empty()) {
        m_ui.ShowToast("Invalid project path", Toast::Type::Error);
        return false;
    }
    
    // Now construct paths - parent_path() will work correctly
    fs::path currentPath(pathStr);
    fs::path parentDir = currentPath.parent_path();
    
    // Preserve .cartproj extension
    std::string newFolderName = sanitizedName;
    if (hasCartprojExt) {
        newFolderName += CARTPROJ_EXTENSION;
    }
    fs::path newPath = parentDir / newFolderName;
    
    // Get strings for comparison
    std::string currentNorm = currentPath.string();
    std::string newNorm = newPath.string();
    
    // Check if the name is actually different
    // Use case-insensitive comparison on macOS/Windows (case-preserving filesystems)
#if defined(__APPLE__) || defined(_WIN32)
    std::string currentLower = currentNorm;
    std::string newLower = newNorm;
    std::transform(currentLower.begin(), currentLower.end(), 
                   currentLower.begin(), ::tolower);
    std::transform(newLower.begin(), newLower.end(), 
                   newLower.begin(), ::tolower);
    if (currentLower == newLower) {
        // Same path (case-insensitive) - nothing to do
        return true;
    }
#else
    if (currentNorm == newNorm) {
        // Same path - nothing to do
        return true;
    }
#endif
    
    // Check if target folder already exists (and is different from current)
    if (fs::exists(newPath)) {
        // On case-insensitive filesystems, check if it's actually the same folder
        std::error_code ec;
        if (fs::equivalent(currentPath, newPath, ec)) {
            // Same folder, just different case - proceed with rename for case change
        } else {
            m_ui.ShowToast("A folder named \"" + sanitizedName + 
                          "\" already exists", Toast::Type::Error);
            return false;
        }
    }
    
    // Store old path for recent projects update
    std::string oldPath = m_currentFilePath;
    
    // Perform the rename
    std::error_code ec;
    fs::rename(currentPath, newPath, ec);
    
    if (ec) {
        m_ui.ShowToast("Failed to rename folder: " + ec.message(), 
                      Toast::Type::Error);
        return false;
    }
    
    // Update current file path (ensure trailing slash for folders)
    std::string newPathStr = newPath.string();
    if (newPathStr.back() != '/' && newPathStr.back() != '\\') {
#ifdef _WIN32
        newPathStr += '\\';
#else
        newPathStr += '/';
#endif
    }
    m_currentFilePath = newPathStr;
    
    // Update recent projects list
    RecentProjects::Remove(oldPath);
    RecentProjects::Add(m_currentFilePath);
    
    // Update window title
    UpdateWindowTitle();
    
    m_ui.ShowToast("Project renamed to \"" + sanitizedName + "\"", 
                  Toast::Type::Success);
    
    return true;
}

void App::ExportPackage(const std::string& cartPath) {
    // Use cached thumbnail from last render (simpler and more reliable)
    std::vector<uint8_t> thumbnailPixels;
    int thumbWidth = 0, thumbHeight = 0;
    
    if (m_canvas.hasCachedThumbnail) {
        thumbnailPixels = m_canvas.cachedThumbnail;
        thumbWidth = m_canvas.cachedThumbnailWidth;
        thumbHeight = m_canvas.cachedThumbnailHeight;
    }
    
    bool success = Package::Save(m_model, cartPath, &m_icons,
                                thumbnailPixels.empty() ? nullptr : 
                                    thumbnailPixels.data(),
                                thumbWidth, thumbHeight);
    
    if (success) {
        // Extract filename from path
        std::string filename = std::filesystem::path(cartPath)
            .filename().string();
        m_ui.ShowToast("Exported: " + filename, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to export package: " + cartPath, 
                      Toast::Type::Error);
    }
}

void App::ExportPng(const std::string& path) {
    bool success = Cartograph::ExportPng::Export(
        m_model, 
        m_canvas, 
        *m_renderer, 
        &m_icons,
        path, 
        m_ui.m_modals.exportOptions
    );
    
    if (success) {
        // Extract filename from path
        std::string filename = std::filesystem::path(path).filename().string();
        m_ui.ShowToast("Exported: " + filename, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to export: " + path, Toast::Type::Error);
    }
}

void App::CheckAutosaveRecovery() {
    std::string autosaveDir = Platform::GetAutosaveDir();
    std::string autosavePath = autosaveDir + "autosave.json";
    std::string metadataPath = autosaveDir + "metadata.json";
    
    // Check if autosave file exists
    auto autosaveContent = Platform::ReadFile(autosavePath);
    if (!autosaveContent) {
        return;  // No autosave to recover
    }
    
    // Check metadata for clean shutdown flag
    auto metadataContent = Platform::ReadFile(metadataPath);
    bool wasCleanShutdown = true;  // Default to clean if no metadata
    
    if (metadataContent) {
        try {
            nlohmann::json meta = nlohmann::json::parse(*metadataContent);
            wasCleanShutdown = meta.value("cleanShutdown", true);
        } catch (...) {
            // Metadata parse error, assume unclean
            wasCleanShutdown = false;
        }
    }
    
    // If shutdown wasn't clean, mark for recovery
    if (!wasCleanShutdown) {
        m_hasAutosaveRecovery = true;
    }
}

void App::SaveAutosaveMetadata() {
    std::string autosaveDir = Platform::GetAutosaveDir();
    Platform::EnsureDirectoryExists(autosaveDir);
    
    std::string metadataPath = autosaveDir + "metadata.json";
    
    nlohmann::json meta;
    meta["projectPath"] = m_currentFilePath;
    meta["timestamp"] = static_cast<int64_t>(Platform::GetTime());
    meta["cleanShutdown"] = !m_model.dirty;  // Clean if no unsaved changes
    
    std::string content = meta.dump(2);
    Platform::WriteTextFile(metadataPath, content);
}

void App::CleanupAutosave() {
    std::string autosaveDir = Platform::GetAutosaveDir();
    std::string autosavePath = autosaveDir + "autosave.json";
    std::string metadataPath = autosaveDir + "metadata.json";
    
    // Delete autosave files
    // Note: Platform::DeleteFile doesn't exist yet, so we'll use filesystem
    try {
        std::filesystem::remove(autosavePath);
        std::filesystem::remove(metadataPath);
    } catch (...) {
        // Ignore errors - file might not exist
    }
}

void App::UpdateWindowTitle() {
    if (!m_window) return;
    
    std::string title;
    
    // Get project name: prefer meta.title, fall back to filename, then "Untitled"
    std::string projectName;
    if (!m_model.meta.title.empty()) {
        projectName = m_model.meta.title;
    } else if (!m_currentFilePath.empty()) {
        // Extract filename from path as fallback
        size_t lastSlash = m_currentFilePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            projectName = m_currentFilePath.substr(lastSlash + 1);
        } else {
            projectName = m_currentFilePath;
        }
    } else {
        projectName = "Untitled";
    }
    
    // Add modified indicator using platform convention
    if (m_model.dirty) {
#ifdef __APPLE__
        // macOS uses bullet before project name
        title = "• " + projectName + " - Cartograph";
#else
        // Windows/Linux use asterisk after project name
        title = "*" + projectName + " - Cartograph";
#endif
    } else {
        title = projectName + " - Cartograph";
    }
    
    SDL_SetWindowTitle(m_window.get(), title.c_str());
}

} // namespace Cartograph
