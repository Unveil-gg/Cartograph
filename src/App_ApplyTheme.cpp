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


namespace Cartograph {


void App::ApplyTheme(const Theme& theme) {
    ImGuiStyle& style = ImGui::GetStyle();

    // Reset sizes to defaults first: ScaleAllSizes() is cumulative, so
    // reapplying it on an already-scaled style compounds the scale.
    style = ImGuiStyle();
    
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
    style.FontScaleMain = theme.uiScale;
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
    if (Package::HasCartExtension(path)) {
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
        m_model = std::move(newModel);
        FinishOpen(path);
    } else {
        m_ui.ShowToast("Failed to open: " + path, Toast::Type::Error);
    }
}


void App::InstallLoadedProject(
    const std::string& path,
    Model loadedModel,
    IconManager& loadedIcons
) {
    m_model = std::move(loadedModel);

    // Custom icons were decoded on the worker. Copy them here.
    auto customIcons = loadedIcons.GetCustomIconData();
    for (const auto& iconPair : customIcons) {
        const std::vector<uint8_t>* pixels = iconPair.second;
        if (!pixels || pixels->empty()) {
            continue;
        }
        int width = 0;
        int height = 0;
        if (!loadedIcons.GetIconDimensions(iconPair.first, width, height)) {
            continue;
        }
        m_icons.AddIconFromMemory(
            iconPair.first, pixels->data(), width, height, "marker");
    }

    FinishOpen(path);
}


void App::FinishOpen(const std::string& path) {
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

    std::string projectName = std::filesystem::path(path)
        .filename().string();
    if (projectName.empty()) {
        projectName = std::filesystem::path(path)
            .parent_path().filename().string();
    }
    m_ui.ShowToast("Opened: " + projectName, Toast::Type::Success);
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


} // namespace Cartograph
