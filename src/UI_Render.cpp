#include "UI.h"
#include "Config.h"
#include "App.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "Jobs.h"
#include "render/Renderer.h"
#include "IOJson.h"
#include "Package.h"
#include "ProjectFolder.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include "platform/Time.h"
#include "platform/System.h"
#include "platform/NativeMenu.h"
#include "platform/NativeMenu_imgui.h"
#include "UI/ImGuiHelpers.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_surface.h>
#include <nlohmann/json.hpp>
#include <stb/stb_image.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <cstring>
#include <set>
#include <map>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// SDL_CursorDeleter implementation (outside namespace)

namespace Cartograph {


void UI::Render(
    App& app,
    IRenderer& renderer,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs,
    KeymapManager& keymap,
    float deltaTime
) {
    // Note: Menu update/render moved to UpdateMenu() 
    // (called from App::Render before state check)
    
    // Global keyboard shortcuts (work even when menus are closed)
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        // Save
        if (keymap.IsActionTriggered("save")) {
            app.SaveProject();
        }
        
        // Save As
        if (keymap.IsActionTriggered("saveAs")) {
            ShowSaveProjectDialog(app);
        }
        
        // Export PNG
        if (keymap.IsActionTriggered("export")) {
            m_modals.showExportModal = true;
        }
        
        // Export Package
        if (keymap.IsActionTriggered("exportPackage")) {
            ShowExportPackageDialog(app);
        }
    }
    
    // Create fullscreen dockspace window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // WorkPos/WorkSize already account for main menu bar
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("CartographDockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);
    
    // Create dockspace
    ImGuiID dockspaceId = ImGui::GetID("CartographDockSpaceID");
    ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
    
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
    
    // Build fixed layout on first run
    if (!m_layoutInitialized) {
        BuildFixedLayout(dockspaceId);
        m_layoutInitialized = true;
    }
    
    ImGui::End();
    
    // Render all panels (they will dock into the dockspace)
    // Canvas first (background layer), then side panels (on top for tooltips)
    RenderValidationWindow(model, canvas);
    m_canvasPanel.Render(renderer, model, canvas, history, icons, keymap);
    RenderStatusBar(model, canvas);
    RenderToolsPanel(model, history, icons, jobs, canvas);
    if (m_canvasPanel.requestEditInterior) {
        m_canvasPanel.requestEditInterior = false;
        if (!m_canvasPanel.selectedRoomId.empty()) {
            m_interiorEditor.Open(m_canvasPanel.selectedRoomId);
        }
    }
    m_interiorEditor.Render(model, history);
    
    if (showPropertiesPanel) {
        RenderPropertiesPanel(model, icons, jobs, history, canvas);
    }
    
    // Render toasts
    RenderToasts(deltaTime);
    
    // Render modal dialogs
    m_modals.RenderAll(
        app, model, canvas, history, icons, jobs, keymap,
        m_canvasPanel.selectedIconName,
        m_canvasPanel.selectedMarkerId,
        m_canvasPanel.selectedTileId
    );
    
    // Handle export PNG dialog (triggered after modal closes)
    if (m_modals.shouldShowExportPngDialog) {
        m_modals.shouldShowExportPngDialog = false;
        ShowExportPngDialog(app);
    }
    
    // Note: Cursor updates are now handled by CanvasPanel
}

void UI::ShowToast(
    const std::string& message, 
    Toast::Type type,
    float duration
) {
    // Convert Toast::Type to MessageType and add to console
    MessageType msgType;
    switch (type) {
        case Toast::Type::Info:    msgType = MessageType::Info; break;
        case Toast::Type::Success: msgType = MessageType::Success; break;
        case Toast::Type::Warning: msgType = MessageType::Warning; break;
        case Toast::Type::Error:   msgType = MessageType::Error; break;
        default:                   msgType = MessageType::Info; break;
    }
    AddConsoleMessage(message, msgType);
    
    // Legacy toast system (deprecated, will be removed)
    Toast toast;
    toast.message = message;
    toast.type = type;
    toast.remainingTime = duration;
    m_toasts.push_back(toast);
}


void UI::AddConsoleMessage(const std::string& message, MessageType type) {
    double timestamp = Platform::GetTime();
    m_consoleMessages.emplace_back(message, type, timestamp);
    
    // Keep only last MAX_CONSOLE_MESSAGES
    if (m_consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
        m_consoleMessages.erase(m_consoleMessages.begin());
    }
}


void UI::InitializeMenuCallbacks(App& app) {
    // File menu callbacks
    m_nativeMenu->SetCallback("file.new", [this, &app]() {
        // Check for unsaved changes before creating new project
        if (app.GetModel().dirty) {
            m_modals.pendingAction = Modals::PendingAction::NewProject;
            m_modals.showSaveBeforeActionModal = true;
        } else {
            app.ShowNewProjectDialog();
        }
    });
    
    m_nativeMenu->SetCallback("file.open", [this, &app]() {
        // Check for unsaved changes before opening project
        if (app.GetModel().dirty) {
            m_modals.pendingAction = Modals::PendingAction::OpenProject;
            m_modals.showSaveBeforeActionModal = true;
        } else {
            app.ShowOpenProjectDialog();
        }
    });
    
    m_nativeMenu->SetCallback("file.save", [&app]() {
        app.SaveProject();
    });
    
    m_nativeMenu->SetCallback("file.save_as", [this, &app]() {
        ShowSaveProjectDialog(app);
    });
    
    m_nativeMenu->SetCallback("file.export_package", [this, &app]() {
        ShowExportPackageDialog(app);
    });
    
    m_nativeMenu->SetCallback("file.export_png", [this]() {
        m_modals.showExportModal = true;
    });
    
    m_nativeMenu->SetCallback("file.quit", [&app]() {
        app.RequestQuit();
    });
    
    // Edit menu callbacks
    // Note: undo/redo/zoom callbacks are set dynamically in Update()
    // because they need access to model/history/canvas
    
    m_nativeMenu->SetCallback("edit.settings", [this]() {
        m_modals.showSettingsModal = true;
    });
    
    // View menu callbacks
    m_nativeMenu->SetCallback("view.properties", [this]() {
        showPropertiesPanel = !showPropertiesPanel;
        m_layoutInitialized = false;
    });
    
    // Assets menu callback - set dynamically in Render since it needs 
    // icons/jobs refs
    
    // Help menu callbacks
    m_nativeMenu->SetCallback("help.about", [this]() {
        m_modals.showAboutModal = true;
    });
    
    m_nativeMenu->SetCallback("help.license", []() {
        Platform::OpenURL(
            "https://github.com/Unveil-gg/Cartograph/blob/main/LICENSE"
        );
    });
    
    m_nativeMenu->SetCallback("help.report_bug", []() {
        Platform::OpenURL(
            "https://github.com/Unveil-gg/Cartograph/issues/new"
        );
    });
}




} // namespace Cartograph
