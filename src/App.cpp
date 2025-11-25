#include "App.h"
#include "render/GlRenderer.h"
#include "platform/Paths.h"
#include "platform/Time.h"
#include "IOJson.h"
#include "Package.h"
#include "ExportPng.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace Cartograph {

App::App()
    : m_window(nullptr)
    , m_glContext(nullptr)
    , m_running(false)
    , m_appState(AppState::Welcome)
    , m_lastEditTime(0)
    , m_lastAutosaveTime(0)
    , m_lastFrameTime(0)
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
    m_window = SDL_CreateWindow(
        title.c_str(),
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    
    if (!m_window) {
        return false;
    }
    
    // Create OpenGL context
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        return false;
    }
    
    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(1);  // Enable VSync
    
    // Initialize renderer
    m_renderer = std::make_unique<GlRenderer>(m_window);
    
    // Initialize ImGui
    SetupImGui();
    
    // Initialize minimal theme for welcome screen
    m_model.InitDefaultTheme("Dark");
    
    // Load icons from assets
    std::string assetsDir = Platform::GetAssetsDir();
    m_icons.LoadFromDirectory(assetsDir + "icons/", true);
    m_icons.BuildAtlas();
    
    // Setup UI (dockspace will be set up when entering editor)
    m_ui.SetupDockspace();
    
    // Start background job queue
    m_jobs.Start();
    
    m_running = true;
    m_appState = AppState::Welcome;
    m_lastFrameTime = Platform::GetTime();
    
    return true;
}

int App::Run() {
    while (m_running) {
        double currentTime = Platform::GetTime();
        float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;
        
        ProcessEvents();
        Update(deltaTime);
        Render();
        
        // Process job callbacks
        m_jobs.ProcessCallbacks();
        
        // Autosave check
        DoAutosave();
    }
    
    return 0;
}

void App::Shutdown() {
    if (!m_window) return;
    
    m_jobs.Stop();
    
    // Clean up IconManager before ImGui/GL shutdown to prevent memory
    // corruption from destroying GL context with live texture references
    m_icons.Clear();
    
    ShutdownImGui();
    
    if (m_glContext) {
        // SDL3 renamed DeleteContext to DestroyContext
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    
    SDL_Quit();
}

void App::RequestQuit() {
    // TODO: Check for unsaved changes
    m_running = false;
}

void App::ShowWelcomeScreen() {
    m_appState = AppState::Welcome;
}

void App::ShowEditor() {
    m_appState = AppState::Editor;
    
    // Ensure model is initialized if not already
    if (m_model.palette.empty()) {
        m_model.InitDefaults();
        m_keymap.LoadBindings(m_model.keymap);
    }
    
    // No need for world room anymore - regions are inferred from walls
    // Tiles can be painted anywhere on the grid
    
    // Only generate auto-walls if there are existing walls/edges
    // (Skip on empty new projects to avoid performance issues)
    if (!m_model.edges.empty()) {
        m_model.UpdateAllAutoWalls();
    }
}

void App::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        
        switch (event.type) {
            case SDL_EVENT_QUIT:
                RequestQuit();
                break;
                
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == SDL_GetWindowID(m_window)) {
                    RequestQuit();
                }
                break;
        }
    }
}

void App::Update(float deltaTime) {
    // Update canvas
    m_canvas.Update(m_model, deltaTime);
    
    // Track dirty state for autosave
    if (m_model.dirty) {
        m_lastEditTime = Platform::GetTime();
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
    
    // Render UI based on state
    if (m_appState == AppState::Welcome) {
        m_ui.RenderWelcomeScreen(*this, m_model);
    } else {
        m_ui.Render(*m_renderer, m_model, m_canvas, m_history, m_icons,
                    m_jobs, 0.016f);
    }
    
    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Swap buffers
    SDL_GL_SwapWindow(m_window);
}

void App::SetupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable docking (but we'll lock the layout)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Setup platform/renderer backends
    ImGui_ImplSDL3_InitForOpenGL(m_window, m_glContext);
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
    
    if (theme.name == "Dark") {
        ImGui::StyleColorsDark();
    } else if (theme.name == "Print-Light") {
        ImGui::StyleColorsLight();
    }
    
    // Apply UI scale
    style.ScaleAllSizes(theme.uiScale);
}

void App::DoAutosave() {
    if (!m_model.dirty) return;
    
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
            m_ui.ShowToast("Autosaved", Toast::Type::Success, 2.0f);
        }
    }
}

void App::NewProject() {
    // TODO: Check for unsaved changes
    m_model = Model();
    m_model.InitDefaults();
    m_currentFilePath.clear();
    m_history.Clear();
}

void App::OpenProject(const std::string& path) {
    Model newModel;
    bool success = false;
    
    // Try to load as .cart package (C++17 compatible check)
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cart") {
        success = Package::Load(path, newModel);
    } else {
        // Load as raw JSON
        success = IOJson::LoadFromFile(path, newModel);
    }
    
    if (success) {
        m_model = newModel;
        m_currentFilePath = path;
        m_history.Clear();
        m_ui.ShowToast("Opened: " + path, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to open: " + path, Toast::Type::Error);
    }
}

void App::SaveProject() {
    if (m_currentFilePath.empty()) {
        // TODO: Show save dialog
        return;
    }
    
    SaveProjectAs(m_currentFilePath);
}

void App::SaveProjectAs(const std::string& path) {
    bool success = false;
    
    // C++17 compatible check
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".cart") {
        success = Package::Save(m_model, path);
    } else {
        success = IOJson::SaveToFile(m_model, path);
    }
    
    if (success) {
        m_currentFilePath = path;
        m_model.ClearDirty();
        m_ui.ShowToast("Saved: " + path, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to save: " + path, Toast::Type::Error);
    }
}

void App::ExportPng(const std::string& path) {
    bool success = Cartograph::ExportPng::Export(
        m_model, 
        m_canvas, 
        *m_renderer, 
        &m_icons,
        path, 
        m_ui.exportOptions
    );
    
    if (success) {
        m_ui.ShowToast("Exported: " + path, Toast::Type::Success);
    } else {
        m_ui.ShowToast("Failed to export: " + path, Toast::Type::Error);
    }
}

} // namespace Cartograph

