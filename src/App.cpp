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
    
    // Initialize renderer (passes non-owning pointer)
    m_renderer = std::make_unique<GlRenderer>(m_window.get());
    
    // Initialize ImGui
    SetupImGui();
    
    // Initialize minimal theme for welcome screen
    m_model.InitDefaultTheme("Dark");
    
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
    
    // Only generate auto-walls if there are existing walls/edges
    // (Skip on empty new projects to avoid performance issues)
    if (!m_model.edges.empty()) {
        m_model.UpdateAllAutoWalls();
    }
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
    // Skip capture when modals are visible to avoid capturing UI overlays
    bool modalVisible = m_ui.m_modals.showQuitConfirmationModal ||
                        m_ui.m_modals.showSaveBeforeActionModal ||
                        m_ui.m_modals.showAutosaveRecoveryModal;
    
    if (m_appState == AppState::Editor && m_model.dirty && !modalVisible) {
        static double lastThumbnailCapture = 0.0;
        double now = Platform::GetTime();
        if (now - lastThumbnailCapture > 3.0) {
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
    
    if (theme.name == "Dark") {
        ImGui::StyleColorsDark();
    } else if (theme.name == "Print-Light") {
        ImGui::StyleColorsLight();
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

void App::NewProject(const std::string& savePath) {
    // TODO: Check for unsaved changes
    m_model = Model();
    m_model.InitDefaults();
    m_history.Clear();
    
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
        
        // Rebuild icon atlas (must be on main thread for OpenGL)
        m_icons.BuildAtlas();
        
        // Load keymap bindings into keymap manager
        m_keymap.LoadBindings(m_model.keymap);
        
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
    // Callback struct
    struct CallbackData {
        App* app;
    };
    
    // Allocate callback data (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->app = this;
    
    // Setup filters for both .cart files and folders
    static SDL_DialogFileFilter filters[] = {
        { "Cartograph Project", "cart" },
        { "All Files", "*" }
    };
    
    // Show native file dialog for opening
    SDL_ShowOpenFileDialog(
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
            
            // User selected a file or folder - open it
            std::string path = filelist[0];
            data->app->OpenProject(path);
            data->app->ShowEditor();
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Filters
        filters,
        // Number of filters
        2,
        // Default location
        nullptr,
        // Allow multiple files
        false
    );
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
    
    // Get filename from path, or "Untitled" if no file
    std::string filename;
    if (m_currentFilePath.empty()) {
        filename = "Untitled";
    } else {
        // Extract filename from path
        size_t lastSlash = m_currentFilePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = m_currentFilePath.substr(lastSlash + 1);
        } else {
            filename = m_currentFilePath;
        }
    }
    
    // Add modified indicator using platform convention
    if (m_model.dirty) {
#ifdef __APPLE__
        // macOS uses bullet before filename
        title = "• " + filename + " - Cartograph";
#else
        // Windows/Linux use asterisk after filename
        title = "*" + filename + " - Cartograph";
#endif
    } else {
        title = filename + " - Cartograph";
    }
    
    SDL_SetWindowTitle(m_window.get(), title.c_str());
}

} // namespace Cartograph
