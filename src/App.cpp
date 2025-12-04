#include "App.h"
#include "render/SdlGpuRenderer.h"
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
#include <imgui_impl_sdlgpu3.h>
#include <nlohmann/json.hpp>
#include <filesystem>

// SDL resource deleters implementation (outside namespace)
void SDL_WindowDeleter::operator()(SDL_Window* window) const {
    if (window) {
        SDL_DestroyWindow(window);
    }
}

void SDL_GPUDeviceDeleter::operator()(SDL_GPUDevice* device) const {
    if (device) {
        SDL_DestroyGPUDevice(device);
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
    
    // Create GPU device (auto-selects best backend: Metal/Vulkan/D3D12)
    // Note: Debug mode disabled as it can cause issues on older hardware
    m_gpuDevice.reset(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | 
        SDL_GPU_SHADERFORMAT_DXBC,
        false,  // Debug mode off for compatibility
        nullptr  // No specific device preference
    ));
    
    if (!m_gpuDevice) {
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        return false;
    }
    
    // Create window (no OpenGL flag needed)
    m_window.reset(SDL_CreateWindow(
        title.c_str(),
        width, height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    ));
    
    if (!m_window) {
        return false;
    }
    
    // Claim window for GPU rendering
    if (!SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), m_window.get())) {
        SDL_Log("Failed to claim window for GPU: %s", SDL_GetError());
        return false;
    }
    
    // Lock aspect ratio to 16:9 to prevent warping during resize
    float aspectRatio = 16.0f / 9.0f;
    SDL_SetWindowAspectRatio(m_window.get(), aspectRatio, aspectRatio);
    
    // Minimum size to prevent unusably small UI (maintains 16:9)
    SDL_SetWindowMinimumSize(m_window.get(), 1152, 648);
    
    // Initialize renderer
    m_renderer = std::make_unique<SdlGpuRenderer>(
        m_window.get(), m_gpuDevice.get()
    );
    
    // Initialize ImGui
    SetupImGui();
    
    // Initialize minimal theme for welcome screen
    m_model.InitDefaultTheme("Dark");
    
    // Load icons from assets (now uses SDL_GPU for textures)
    std::string assetsDir = Platform::GetAssetsDir();
    m_icons.SetGPUDevice(m_gpuDevice.get());
    m_icons.LoadFromDirectory(assetsDir + "icons/", "marker", true);
    m_icons.LoadFromDirectory(assetsDir + "tools/", "tool", false);
    m_icons.BuildAtlas();
    
    // Set GPU device for UI components that create textures
    m_ui.m_welcomeScreen.SetGPUDevice(m_gpuDevice.get());
    m_ui.m_modals.SetGPUDevice(m_gpuDevice.get());
    
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
    
    // Clean up IconManager before ImGui/GPU shutdown to prevent
    // issues from destroying GPU device with live texture references
    m_icons.Clear();
    
    // Clean up welcome screen textures
    m_ui.m_welcomeScreen.UnloadThumbnailTextures();
    m_ui.m_modals.CleanupTextures(m_gpuDevice.get());
    
    ShutdownImGui();
    
    // Renderer must be destroyed before GPU device
    m_renderer.reset();
    
    // Release window from GPU device before destroying either
    if (m_gpuDevice && m_window) {
        SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), m_window.get());
    }
    
    // SDL resources automatically cleaned up by unique_ptr deleters
    m_gpuDevice.reset();
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
    // Begin GPU frame
    m_renderer->BeginFrame();
    
    // Start ImGui frame
    ImGui_ImplSDLGPU3_NewFrame();
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
        m_ui.m_welcomeScreen.Render(
            *this, m_model, m_canvas, m_history, m_jobs, m_icons, m_keymap
        );
    } else {
        m_ui.Render(*this, *m_renderer, m_model, m_canvas, m_history, 
                    m_icons, m_jobs, m_keymap, 0.016f);
    }
    
    // Finalize ImGui
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    
    // Get command buffer and render ImGui
    SDL_GPUCommandBuffer* cmdBuf = m_renderer->GetCommandBuffer();
    if (cmdBuf && drawData->TotalVtxCount > 0) {
        // Prepare ImGui draw data (uploads vertex/index buffers)
        ImGui_ImplSDLGPU3_PrepareDrawData(drawData, cmdBuf);
        
        // Begin render pass to swapchain
        SDL_GPUTexture* swapchainTex = nullptr;
        if (SDL_WaitAndAcquireGPUSwapchainTexture(
                cmdBuf, m_window.get(), &swapchainTex, nullptr, nullptr)) {
            
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = swapchainTex;
            colorTarget.clear_color.r = bgColor.r;
            colorTarget.clear_color.g = bgColor.g;
            colorTarget.clear_color.b = bgColor.b;
            colorTarget.clear_color.a = bgColor.a;
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            
            SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
                cmdBuf, &colorTarget, 1, nullptr
            );
            
            if (renderPass) {
                ImGui_ImplSDLGPU3_RenderDrawData(drawData, cmdBuf, renderPass);
                SDL_EndGPURenderPass(renderPass);
            }
        }
    }
    
    // End GPU frame (submits command buffer)
    m_renderer->EndFrame();
    
    // Capture thumbnail (uses previous frame's pixels - 16ms delay imperceptible)
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
}

void App::SetupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable docking (but we'll lock the layout)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Setup platform backend
    ImGui_ImplSDL3_InitForOther(m_window.get());
    
    // Setup SDL_GPU renderer backend
    ImGui_ImplSDLGPU3_InitInfo gpuInfo = {};
    gpuInfo.Device = m_gpuDevice.get();
    gpuInfo.ColorTargetFormat = m_renderer->GetSwapchainFormat();
    gpuInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&gpuInfo);
    
    // Apply theme
    ApplyTheme(m_model.theme);
}

void App::ShutdownImGui() {
    ImGui_ImplSDLGPU3_Shutdown();
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
        
        // Rebuild icon atlas (must be on main thread for GPU)
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
