#include "UI.h"
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
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
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
void SDL_CursorDeleter::operator()(SDL_Cursor* cursor) const {
    if (cursor) {
        SDL_DestroyCursor(cursor);
    }
}

namespace Cartograph {

namespace {

/**
 * Sanitize a string for use as a filename.
 * Replaces spaces and special characters with dashes, collapses multiple
 * dashes, and trims leading/trailing dashes.
 * @param input The string to sanitize
 * @return Sanitized filename (without extension)
 */
std::string SanitizeFilename(const std::string& input) {
    if (input.empty()) {
        return "Untitled";
    }
    
    std::string result;
    result.reserve(input.size());
    
    bool lastWasDash = true;  // Start true to trim leading dashes
    
    for (char c : input) {
        // Replace spaces and invalid filename characters with dash
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == '\t' || c == '\n' || c == '\r') {
            if (!lastWasDash) {
                result += '-';
                lastWasDash = true;
            }
        } else {
            result += c;
            lastWasDash = false;
        }
    }
    
    // Trim trailing dashes
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    
    // Fallback if result is empty after sanitization
    if (result.empty()) {
        return "Untitled";
    }
    
    return result;
}

}  // anonymous namespace

UI::UI() : m_modals(*this), m_welcomeScreen(*this) {
    // Connect canvas panel to UI state for shared members
    m_canvasPanel.showPropertiesPanel = &showPropertiesPanel;
    m_canvasPanel.layoutInitialized = &m_layoutInitialized;
    m_canvasPanel.modals = &m_modals;
    
    // Create platform-specific native menu (but don't initialize yet)
    m_nativeMenu = CreateNativeMenu();
}

UI::~UI() {
    m_welcomeScreen.UnloadThumbnailTextures();
}

void UI::InitializeNativeMenu() {
    if (m_nativeMenu) {
        m_nativeMenu->Initialize();
    }
}

void UI::SetupDockspace() {
    // Docking setup happens in first frame of Render()
}

void UI::UpdateMenu(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    // Initialize menu callbacks once
    static bool menuCallbacksInitialized = false;
    if (!menuCallbacksInitialized) {
        InitializeMenuCallbacks(app);
        menuCallbacksInitialized = true;
    }
    
    // Set callbacks that need icons/jobs reference
    m_nativeMenu->SetCallback("assets.import_icon", 
        [this, &icons, &jobs]() {
            ImportIcon(icons, jobs);
        }
    );
    
    // Set selection operation callbacks (need model/history/canvasPanel)
    m_nativeMenu->SetCallback("edit.cut", 
        [this, &model, &history]() {
            if (m_canvasPanel.hasSelection && 
                !m_canvasPanel.currentSelection.IsEmpty()) {
                m_canvasPanel.CopySelection(model);
                m_canvasPanel.DeleteSelection(model, history);
            }
        }
    );
    m_nativeMenu->SetCallback("edit.copy", 
        [this, &model]() {
            if (m_canvasPanel.hasSelection && 
                !m_canvasPanel.currentSelection.IsEmpty()) {
                m_canvasPanel.CopySelection(model);
            }
        }
    );
    m_nativeMenu->SetCallback("edit.paste", 
        [this]() {
            if (!m_canvasPanel.clipboard.IsEmpty()) {
                m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
                m_canvasPanel.EnterPasteMode();
            }
        }
    );
    m_nativeMenu->SetCallback("edit.delete", 
        [this, &model, &history]() {
            if (m_canvasPanel.hasSelection && 
                !m_canvasPanel.currentSelection.IsEmpty()) {
                m_canvasPanel.DeleteSelection(model, history);
            }
        }
    );
    m_nativeMenu->SetCallback("edit.selectAll", 
        [this, &model]() {
            m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
            m_canvasPanel.SelectAll(model);
        }
    );
    
    // Update menu state (native on macOS, ImGui on Windows/Linux)
    m_nativeMenu->Update(app, model, canvas, history, icons, jobs);
    
    // For ImGui menus, we need to pass the hierarchy panel visibility pointer
    // This is a bit of a hack but necessary for the ImGui implementation
    if (!m_nativeMenu->IsNative()) {
        NativeMenuImGui* imguiMenu = 
            dynamic_cast<NativeMenuImGui*>(m_nativeMenu.get());
        if (imguiMenu) {
            imguiMenu->SetShowPropertiesPanel(&showPropertiesPanel);
        }
    }
    
    // Render menu (for ImGui implementations)
    m_nativeMenu->Render();
}

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
    m_canvasPanel.Render(renderer, model, canvas, history, icons, keymap);
    RenderStatusBar(model, canvas);
    RenderToolsPanel(model, history, icons, jobs, canvas);
    
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

void UI::RenderMenuBar(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    if (ImGui::BeginMainMenuBar()) {
        // File Menu
        if (ImGui::BeginMenu("File")) {
            // New project
            std::string newShortcut = Platform::FormatShortcut("N");
            if (ImGui::MenuItem("New Project...", newShortcut.c_str())) {
                app.ShowNewProjectDialog();
            }
            
            // Open project
            std::string openShortcut = Platform::FormatShortcut("O");
            if (ImGui::MenuItem("Open Project...", openShortcut.c_str())) {
                app.ShowOpenProjectDialog();
            }
            
            ImGui::Separator();
            
            // Save
            std::string saveShortcut = Platform::FormatShortcut("S");
            if (ImGui::MenuItem("Save", saveShortcut.c_str())) {
                app.SaveProject();
            }
            
            // Save As
            std::string saveAsShortcut = 
                Platform::FormatShortcut("Shift+S");
            if (ImGui::MenuItem("Save As...", saveAsShortcut.c_str())) {
                ShowSaveProjectDialog(app);
            }
            
            ImGui::Separator();
            
            // Export Package
            std::string exportPkgShortcut = 
                Platform::FormatShortcut("Shift+E");
            if (ImGui::MenuItem("Export Package (.cart)...", 
                               exportPkgShortcut.c_str())) {
                ShowExportPackageDialog(app);
            }
            
            // Export PNG
            std::string exportPngShortcut = Platform::FormatShortcut("E");
            if (ImGui::MenuItem("Export PNG...", 
                               exportPngShortcut.c_str())) {
                m_modals.showExportModal = true;
            }
            
            ImGui::Separator();
            
            // Quit (platform-specific label)
#ifdef _WIN32
            std::string quitLabel = "Exit";
#else
            std::string quitLabel = "Quit";
#endif
            std::string quitShortcut = Platform::FormatShortcut("Q");
            if (ImGui::MenuItem(quitLabel.c_str(), quitShortcut.c_str())) {
                app.RequestQuit();
            }
            
            ImGui::EndMenu();
        }
        
        // Edit Menu
        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = history.CanUndo();
            bool canRedo = history.CanRedo();
            
            std::string undoShortcut = Platform::FormatShortcut("Z");
            if (ImGui::MenuItem("Undo", undoShortcut.c_str(), 
                               false, canUndo)) {
                history.Undo(model);
            }
            
            std::string redoShortcut = Platform::FormatShortcut("Y");
            if (ImGui::MenuItem("Redo", redoShortcut.c_str(), 
                               false, canRedo)) {
                history.Redo(model);
            }
            
            ImGui::Separator();
            
            // Selection operations
            bool hasSelection = m_canvasPanel.hasSelection && 
                               !m_canvasPanel.currentSelection.IsEmpty();
            bool hasClipboard = !m_canvasPanel.clipboard.IsEmpty();
            
            std::string cutShortcut = Platform::FormatShortcut("X");
            if (ImGui::MenuItem("Cut", cutShortcut.c_str(), 
                               false, hasSelection)) {
                m_canvasPanel.CopySelection(model);
                m_canvasPanel.DeleteSelection(model, history);
            }
            
            std::string copyShortcut = Platform::FormatShortcut("C");
            if (ImGui::MenuItem("Copy", copyShortcut.c_str(), 
                               false, hasSelection)) {
                m_canvasPanel.CopySelection(model);
            }
            
            std::string pasteShortcut = Platform::FormatShortcut("V");
            if (ImGui::MenuItem("Paste", pasteShortcut.c_str(), 
                               false, hasClipboard)) {
                m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
                m_canvasPanel.EnterPasteMode();
            }
            
            std::string deleteShortcut = Platform::FormatShortcut("Delete");
            if (ImGui::MenuItem("Delete", deleteShortcut.c_str(), 
                               false, hasSelection)) {
                m_canvasPanel.DeleteSelection(model, history);
            }
            
            ImGui::Separator();
            
            std::string selectAllShortcut = Platform::FormatShortcut("A");
            if (ImGui::MenuItem("Select All", selectAllShortcut.c_str())) {
                m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
                m_canvasPanel.SelectAll(model);
            }
            
            ImGui::Separator();
            
            std::string settingsShortcut = Platform::FormatShortcut(",");
            if (ImGui::MenuItem("Settings...", settingsShortcut.c_str())) {
                m_modals.showSettingsModal = true;
            }
            
            ImGui::EndMenu();
        }
        
        // View Menu
        if (ImGui::BeginMenu("View")) {
            std::string propPanelShortcut = Platform::FormatShortcut("P");
            if (ImGui::MenuItem("Hierarchy Panel", propPanelShortcut.c_str(),
                               showPropertiesPanel)) {
                showPropertiesPanel = !showPropertiesPanel;
                m_layoutInitialized = false;  // Trigger layout rebuild
            }
            
            ImGui::Separator();
            
            ImGui::MenuItem("Show Grid", "G", &canvas.showGrid);
            
            // Marker Labels submenu
            if (ImGui::BeginMenu("Marker Labels")) {
                bool isAlways = (canvas.markerLabelMode == MarkerLabelMode::Always);
                bool isNever = (canvas.markerLabelMode == MarkerLabelMode::Never);
                bool isHover = (canvas.markerLabelMode == MarkerLabelMode::HoverOnly);
                
                if (ImGui::MenuItem("Always Show", nullptr, isAlways)) {
                    canvas.markerLabelMode = MarkerLabelMode::Always;
                }
                if (ImGui::MenuItem("Never Show", nullptr, isNever)) {
                    canvas.markerLabelMode = MarkerLabelMode::Never;
                }
                if (ImGui::MenuItem("Hover Only", nullptr, isHover)) {
                    canvas.markerLabelMode = MarkerLabelMode::HoverOnly;
                }
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Zoom In", "=")) {
                canvas.SetZoom(canvas.zoom * 1.2f);
            }
            if (ImGui::MenuItem("Zoom Out", "-")) {
                canvas.SetZoom(canvas.zoom / 1.2f);
            }
            if (ImGui::MenuItem("Reset Zoom", "0")) {
                canvas.SetZoom(2.5f);
            }
            
            ImGui::EndMenu();
        }
        
        // Assets Menu
        if (ImGui::BeginMenu("Assets")) {
            if (ImGui::MenuItem("Import Icon...")) {
                ImportIcon(icons, jobs);
            }
            
            // Show loading indicator if importing
            if (isImportingIcon) {
                ImGui::Separator();
                ImGui::TextDisabled("Importing: %s...", 
                                   importingIconName.c_str());
            }
            
            ImGui::EndMenu();
        }
        
        // Help Menu
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Cartograph")) {
                m_modals.showAboutModal = true;
            }
            
            if (ImGui::MenuItem("View License")) {
                // Open LICENSE file
                std::string licensePath = Platform::GetAssetsDir() + 
                                         "../LICENSE";
                Platform::OpenURL("file://" + licensePath);
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Report Bug...")) {
                Platform::OpenURL(
                    "https://github.com/Unveil-gg/Cartograph/issues/new"
                );
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void UI::RenderPalettePanel(Model& model) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;
    
    ImGui::Begin("Cartograph/Palette", nullptr, flags);
    
    ImGui::Text("Tile Types");
    ImGui::Separator();
    
    for (const auto& tile : model.palette) {
        ImGui::PushID(tile.id);
        
        bool selected = (m_canvasPanel.selectedTileId == tile.id);
        ImVec4 color = tile.color.ToImVec4();
        
        // Color button
        if (ImGui::ColorButton("##color", color, 0, ImVec2(24, 24))) {
            m_canvasPanel.selectedTileId = tile.id;
        }
        
        ImGui::SameLine();
        
        // Selectable name
        if (ImGui::Selectable(tile.name.c_str(), selected)) {
            m_canvasPanel.selectedTileId = tile.id;
        }
        
        ImGui::PopID();
    }
    
    ImGui::End();
}

void UI::RenderToolsPanel(Model& model, History& history, IconManager& icons, 
                          JobQueue& jobs, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Tools", nullptr, flags);
    
    ImGui::Text("Tools");
    ImGui::Separator();
    
    const char* toolNames[] = {
        "Move", "Select", "Paint", "Erase", "Fill", 
        "Marker", "Eyedropper", "Zoom"
    };
    
    const char* toolIconNames[] = {
        "move", "square-dashed", "paintbrush", "paint-bucket", "eraser",
        "map-pinned", "pipette", "zoom-in"
    };
    
    const char* toolShortcuts[] = {
        "V", "S", "B", "E", "F", "M", "I", "Z"
    };
    
    // Icon button size and spacing
    const ImVec2 iconButtonSize(36.0f, 36.0f);
    const float iconSpacing = 6.0f;
    const float panelPadding = 8.0f;
    const int TOOLS_PER_ROW = 4;  // Fixed 4-column grid
    
    // Add padding
    ImGui::Dummy(ImVec2(0, panelPadding * 0.5f));
    
    for (int i = 0; i < 8; ++i) {
        bool selected = (static_cast<int>(m_canvasPanel.currentTool) == i);
        
        ImGui::PushID(i);
        
        // Get icon for this tool
        const Icon* icon = icons.GetIcon(toolIconNames[i]);
        
        // Prominent selection highlight
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.36f, 0.69f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.16f, 0.49f, 0.88f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        bool clicked = false;
        
        if (icon) {
            // Draw icon button with UV coordinates
            ImTextureID texId = icons.GetAtlasTexture();
            ImVec2 uv0(icon->u0, icon->v0);
            ImVec2 uv1(icon->u1, icon->v1);
            
            clicked = ImGui::ImageButton(toolIconNames[i], texId, 
                                         iconButtonSize, uv0, uv1);
        } else {
            // Fallback to text button if icon not found
            clicked = ImGui::Button(toolNames[i], iconButtonSize);
        }
        
        // Tooltip on button hover (force to main viewport to avoid clipping)
        if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::SetNextWindowPos(ImVec2(mousePos.x + 15, mousePos.y + 10));
            ImGui::BeginTooltip();
            switch (i) {
                case 0: // Move
                    ImGui::Text("Move Tool [V]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Drag to pan the canvas\nWheel to zoom in/out");
                    break;
                case 1: // Select
                    ImGui::Text("Select Tool [S]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Drag to create selection rectangle");
                    break;
                case 2: // Paint
                    ImGui::Text("Paint Tool [B]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Left-click: Paint with selected color\n"
                        "Right-click: Erase\n"
                        "E+Click: Erase (alternative)");
                    break;
                case 3: // Fill
                    ImGui::Text("Fill Tool [F]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to fill area");
                    break;
                case 4: // Erase
                    ImGui::Text("Erase Tool [E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to erase tiles");
                    break;
                case 5: // Marker
                    ImGui::Text("Marker Tool [M]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to place marker");
                    break;
                case 6: // Eyedropper
                    ImGui::Text("Eyedropper Tool [I]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to pick tile color");
                    break;
                case 7: // Zoom
                    ImGui::Text("Zoom Tool [Z]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Left-click: Zoom in at point\n"
                        "Right-click: Zoom out at point");
                    break;
                default:
                    ImGui::Text("%s", toolNames[i]);
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Tool not yet implemented");
                    break;
            }
            ImGui::EndTooltip();
        }
        
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        
        if (clicked) {
            // Clear selection when switching away from Select tool
            if (m_canvasPanel.currentTool == CanvasPanel::Tool::Select && 
                static_cast<CanvasPanel::Tool>(i) != CanvasPanel::Tool::Select) {
                m_canvasPanel.ClearSelection();
            }
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(i);
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 7) {
            ImGui::SameLine(0, iconSpacing);
        }
        
        ImGui::PopID();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Room Tools (with maroon background)
    ImGui::Text("Room Tools");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, panelPadding * 0.5f));
    
    const char* roomToolNames[] = {
        "Room Select", "Room Paint", "Room Fill", "Room Erase"
    };
    
    const char* roomToolIconNames[] = {
        "mouse-pointer-2", "paintbrush", "paint-bucket", "eraser"
    };
    
    const char* roomToolShortcuts[] = {
        "Shift+S", "Shift+R", "Shift+F", "Shift+E"
    };
    
    const ImVec4 maroonBg(0.55f, 0.1f, 0.1f, 1.0f);
    const ImVec4 maroonBgHover(0.65f, 0.2f, 0.2f, 1.0f);
    const ImVec4 maroonBgActive(0.45f, 0.05f, 0.05f, 1.0f);
    
    for (int i = 0; i < 4; ++i) {
        int toolIdx = 8 + i;  // RoomSelect=8, RoomPaint=9, RoomFill=10, RoomErase=11
        bool selected = (static_cast<int>(m_canvasPanel.currentTool) == toolIdx);
        
        ImGui::PushID(toolIdx);
        
        // Get icon for this tool
        const Icon* icon = icons.GetIcon(roomToolIconNames[i]);
        
        // Maroon background for room tools
        if (selected) {
            // Selected: brighter blue border
            ImGui::PushStyleColor(ImGuiCol_Button, maroonBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, maroonBgHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, maroonBgActive);
            ImGui::PushStyleColor(ImGuiCol_Border, 
                ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        } else {
            // Unselected: maroon background
            ImGui::PushStyleColor(ImGuiCol_Button, maroonBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, maroonBgHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, maroonBgActive);
        }
        
        bool clicked = false;
        
        if (icon) {
            // Draw icon button with UV coordinates
            ImTextureID texId = icons.GetAtlasTexture();
            ImVec2 uv0(icon->u0, icon->v0);
            ImVec2 uv1(icon->u1, icon->v1);
            
            clicked = ImGui::ImageButton(roomToolIconNames[i], texId, 
                                         iconButtonSize, uv0, uv1);
        } else {
            // Fallback to text button if icon not found
            clicked = ImGui::Button(roomToolNames[i], iconButtonSize);
        }
        
        // Tooltip on button hover
        if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::SetNextWindowPos(ImVec2(mousePos.x + 15, mousePos.y + 10));
            ImGui::BeginTooltip();
            switch (i) {
                case 0: // Room Select
                    ImGui::Text("Room Select Tool [Shift+S]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click cell to select its room");
                    break;
                case 1: // Room Paint
                    ImGui::Text("Room Paint Tool [Shift+R]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click/drag to assign cells to active room");
                    break;
                case 2: // Room Fill
                    ImGui::Text("Room Fill Tool [Shift+F]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to flood-fill area into active room");
                    break;
                case 3: // Room Erase
                    ImGui::Text("Room Erase Tool [Shift+E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click/drag to remove cells from rooms");
                    break;
            }
            ImGui::EndTooltip();
        }
        
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        } else {
            ImGui::PopStyleColor(3);
        }
        
        if (clicked) {
            // Clear selection when switching away from Select tool
            if (m_canvasPanel.currentTool == CanvasPanel::Tool::Select) {
                m_canvasPanel.ClearSelection();
            }
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(toolIdx);
            
            // Auto-open hierarchy panel when selecting room tools
            if (!showPropertiesPanel) {
                showPropertiesPanel = true;
                m_layoutInitialized = false;  // Trigger layout rebuild
            }
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 3) {
            ImGui::SameLine(0, iconSpacing);
        }
        
        ImGui::PopID();
    }
    
    // Show tool options for Select tool
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Select) {
        ImGui::Spacing();
        ImGui::Text("Select Layers");
        ImGui::Separator();
        
        ImGui::Checkbox("Tiles", &m_canvasPanel.selectTiles);
        ImGui::Checkbox("Walls/Doors", &m_canvasPanel.selectEdges);
        ImGui::Checkbox("Markers", &m_canvasPanel.selectMarkers);
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Selection info
        if (m_canvasPanel.hasSelection && 
            !m_canvasPanel.currentSelection.IsEmpty()) {
            ImGui::TextColored(
                ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
                "Selection:"
            );
            if (m_canvasPanel.currentSelection.TileCount() > 0) {
                ImGui::Text("  %d tiles", 
                    m_canvasPanel.currentSelection.TileCount());
            }
            if (m_canvasPanel.currentSelection.EdgeCount() > 0) {
                ImGui::Text("  %d edges", 
                    m_canvasPanel.currentSelection.EdgeCount());
            }
            if (m_canvasPanel.currentSelection.MarkerCount() > 0) {
                ImGui::Text("  %d markers", 
                    m_canvasPanel.currentSelection.MarkerCount());
            }
            
            ImGui::Spacing();
            
            // Deselect button
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                m_canvasPanel.ClearSelection();
            }
        } else {
            ImGui::TextDisabled("Drag to select content");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Select All button (always available)
        std::string selectAllLabel = "Select All (" 
                                     + Platform::FormatShortcut("A") + ")";
        if (ImGui::Button(selectAllLabel.c_str(), ImVec2(-1, 0))) {
            m_canvasPanel.SelectAll(model);
        }
        
        // Hint about Edit menu / right-click
        ImGui::Spacing();
        ImGui::TextDisabled("Right-click for Cut/Copy/Paste");
    }
    
    // Show tool options for Zoom tool
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Zoom) {
        ImGui::Spacing();
        RenderZoomOptions(canvas, history, model);
    }
    
    // Show tool options for room tools (match regular tool UX)
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::RoomPaint ||
        m_canvasPanel.currentTool == CanvasPanel::Tool::RoomFill) {
        ImGui::Spacing();
        ImGui::Text("Target Room:");
        ImGui::Separator();
        
        // Calculate height for scrollable area (max 10 rooms visible)
        const float itemHeight = 28.0f;  // Height per room row
        const float maxVisibleRooms = 10.0f;
        const float addButtonHeight = 28.0f;
        float listHeight = std::min(
            static_cast<float>(model.rooms.size()) * itemHeight + addButtonHeight,
            maxVisibleRooms * itemHeight + addButtonHeight
        );
        // Minimum height when empty
        if (model.rooms.empty()) {
            listHeight = addButtonHeight + 8.0f;
        }
        
        // Scrollable room list
        if (ImGui::BeginChild("##roomList", ImVec2(-1, listHeight), true)) {
            // Display rooms as list (similar to paint palette)
            for (const auto& room : model.rooms) {
                ImGui::PushID(room.id.c_str());
                
                bool isSelected = (m_canvasPanel.activeRoomId == room.id);
                ImVec4 roomColor = room.color.ToImVec4();
                
                // Highlight selected room row
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, 
                        ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                        ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
                }
                
                // Color swatch button
                if (ImGui::ColorButton("##roomColor", roomColor,
                                      ImGuiColorEditFlags_NoTooltip |
                                      ImGuiColorEditFlags_NoPicker,
                                      ImVec2(20, 20))) {
                    m_canvasPanel.activeRoomId = room.id;
                }
                
                ImGui::SameLine();
                
                // Room name button (full width)
                float availWidth = ImGui::GetContentRegionAvail().x;
                if (ImGui::Button(room.name.c_str(), ImVec2(availWidth, 20))) {
                    m_canvasPanel.activeRoomId = room.id;
                }
                
                if (isSelected) {
                    ImGui::PopStyleColor(2);
                }
                
                // Tooltip with room info
                if (ImGui::IsItemHovered()) {
                    auto cells = model.GetRoomCells(room.id);
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", room.name.c_str());
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "%zu cells", cells.size());
                    ImGui::EndTooltip();
                }
                
                // Right-click context menu
                if (ImGui::BeginPopupContextItem("room_context")) {
                    ImGui::TextDisabled("%s", room.name.c_str());
                    ImGui::Separator();
                    
                    if (ImGui::MenuItem("Rename...")) {
                        m_modals.showRenameRoomDialog = true;
                        m_modals.editingRoomId = room.id;
                        strncpy(m_modals.renameBuffer, room.name.c_str(),
                               sizeof(m_modals.renameBuffer) - 1);
                        m_modals.renameBuffer[
                            sizeof(m_modals.renameBuffer) - 1] = '\0';
                    }
                    
                    if (ImGui::MenuItem("Add Perimeter Walls")) {
                        auto changes = model.ComputeRoomPerimeterWallChanges(
                            room.id);
                        if (!changes.empty()) {
                            std::vector<ModifyEdgesCommand::EdgeChange> edgeChanges;
                            for (const auto& [edgeId, oldState, newState] : changes) {
                                ModifyEdgesCommand::EdgeChange ec;
                                ec.edgeId = edgeId;
                                ec.oldState = oldState;
                                ec.newState = newState;
                                edgeChanges.push_back(ec);
                            }
                            auto cmd = std::make_unique<ModifyEdgesCommand>(
                                edgeChanges);
                            history.AddCommand(std::move(cmd), model, true);
                            AddConsoleMessage("Added " + 
                                std::to_string(changes.size()) + 
                                " wall segment(s) to " + room.name,
                                MessageType::Success);
                        } else {
                            AddConsoleMessage("No walls to add - " + room.name +
                                " already has perimeter walls",
                                MessageType::Info);
                        }
                    }
                    
                    ImGui::Separator();
                    
                    if (ImGui::MenuItem("Delete Room...")) {
                        m_modals.showDeleteRoomDialog = true;
                        m_modals.editingRoomId = room.id;
                    }
                    
                    ImGui::EndPopup();
                }
                
                ImGui::PopID();
            }
            
            // "+ Add Room" button at the end
            ImGui::Spacing();
            if (ImGui::Button("+ Add Room", ImVec2(-1, 0))) {
                Room newRoom;
                newRoom.id = model.GenerateRoomId();
                newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
                newRoom.regionId = -1;
                newRoom.color = model.GenerateDistinctRoomColor();
                newRoom.cellsCacheDirty = true;
                newRoom.connectionsDirty = true;
                
                auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
                history.AddCommand(std::move(cmd), model);
                
                m_canvasPanel.activeRoomId = newRoom.id;
                AddConsoleMessage("Created " + newRoom.name, 
                    MessageType::Success);
            }
        }
        ImGui::EndChild();  // Must be called regardless of BeginChild return
    }
    else if (m_canvasPanel.currentTool == CanvasPanel::Tool::RoomErase) {
        ImGui::Spacing();
        ImGui::Text("Room Eraser Options");
        ImGui::Separator();
        
        // Label with tooltip (matching regular Erase tool)
        ImGui::Text("Eraser Size");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of room eraser brush\n"
                            "1 = single cell (precise)\n"
                            "5 = 5x5 area (fast erase)");
        }
        
        // Slider below label (matching regular Erase tool)
        ImGui::SetNextItemWidth(-1);  // Full width
        ImGui::SliderInt("##roomEraserSize", &m_canvasPanel.eraserBrushSize, 1, 5);
        
        // Visual preview of eraser size (matching regular Erase tool)
        ImGui::Spacing();
        ImGui::Text("Preview:");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        float previewSize = 80.0f;
        float cellSize = previewSize / 5.0f;  // 5x5 grid
        
        // Draw grid background
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                ImVec2 p0(cursorPos.x + x * cellSize, 
                         cursorPos.y + y * cellSize);
                ImVec2 p1(p0.x + cellSize, p0.y + cellSize);
                
                // Calculate if this cell is within brush
                int centerOffset = 2;  // Center of 5x5 grid
                int halfBrush = m_canvasPanel.eraserBrushSize / 2;
                bool inBrush = (x >= centerOffset - halfBrush && 
                               x <= centerOffset + halfBrush &&
                               y >= centerOffset - halfBrush && 
                               y <= centerOffset + halfBrush);
                
                // Draw cell (maroon tint for room eraser)
                ImU32 fillColor = inBrush ? 
                    ImGui::GetColorU32(ImVec4(0.8f, 0.2f, 0.2f, 0.4f)) :
                    ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
                drawList->AddRectFilled(p0, p1, fillColor);
                
                // Draw grid lines
                ImU32 lineColor = ImGui::GetColorU32(
                    ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                drawList->AddRect(p0, p1, lineColor);
            }
        }
        
        ImGui::Dummy(ImVec2(previewSize, previewSize));
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Show eyedropper preview when Eyedropper tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Eyedropper) {
        ImGui::Text("Eyedropper Tool");
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Hover to preview");
        
        ImGui::Spacing();
        
        // Get hover preview color (what WILL be picked)
        Color hoverColor(0.5f, 0.5f, 0.5f, 1.0f);
        std::string hoverName = "No tile";
        bool isHoveringTile = false;
        
        if (m_canvasPanel.isHoveringCanvas) {
            const std::string globalRoomId = "";
            int hoveredTileId = model.GetTileAt(
                globalRoomId, m_canvasPanel.hoveredTileX, m_canvasPanel.hoveredTileY
            );
            
            if (hoveredTileId != 0) {
                for (const auto& tile : model.palette) {
                    if (tile.id == hoveredTileId) {
                        hoverColor = tile.color;
                        hoverName = tile.name;
                        isHoveringTile = true;
                        break;
                    }
                }
            }
        }
        
        // If not hovering, show selected color as fallback
        if (!isHoveringTile) {
            for (const auto& tile : model.palette) {
                if (tile.id == m_canvasPanel.selectedTileId) {
                    hoverColor = tile.color;
                    hoverName = tile.name;
                    break;
                }
            }
        }
        
        // Large hover preview
        float previewWidth = ImGui::GetContentRegionAvail().x;
        float hoverHeight = 60.0f;
        
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 hoverMin = cursorPos;
        ImVec2 hoverMax = ImVec2(
            cursorPos.x + previewWidth, 
            cursorPos.y + hoverHeight
        );
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw hover preview
        drawList->AddRectFilled(hoverMin, hoverMax, hoverColor.ToU32());
        
        // Draw border (highlight if hovering tile)
        ImU32 hoverBorderColor = isHoveringTile ? 
            ImGui::GetColorU32(ImVec4(0.0f, 0.8f, 1.0f, 1.0f)) :  // Cyan
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));   // Dark
        float hoverBorderThickness = isHoveringTile ? 2.5f : 1.5f;
        drawList->AddRect(
            hoverMin, hoverMax, hoverBorderColor, 0.0f, 0, 
            hoverBorderThickness
        );
        
        ImGui::Dummy(ImVec2(previewWidth, hoverHeight));
        
        // Display hover color name (centered)
        ImGui::Spacing();
        float hoverTextWidth = ImGui::CalcTextSize(hoverName.c_str()).x;
        float hoverCenterX = 
            (ImGui::GetContentRegionAvail().x - hoverTextWidth) * 0.5f;
        if (hoverCenterX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + hoverCenterX);
        }
        ImGui::Text("%s", hoverName.c_str());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Current selection (smaller preview)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Current Selection:");
        ImGui::Spacing();
        
        Color selectedColor(0.8f, 0.8f, 0.8f, 1.0f);
        std::string selectedName = "Empty";
        for (const auto& tile : model.palette) {
            if (tile.id == m_canvasPanel.selectedTileId) {
                selectedColor = tile.color;
                selectedName = tile.name;
                break;
            }
        }
        
        // Smaller selected preview
        float selectedHeight = 40.0f;
        cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 selectedMin = cursorPos;
        ImVec2 selectedMax = ImVec2(
            cursorPos.x + previewWidth, 
            cursorPos.y + selectedHeight
        );
        
        drawList->AddRectFilled(
            selectedMin, selectedMax, selectedColor.ToU32()
        );
        drawList->AddRect(
            selectedMin, selectedMax,
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)),
            0.0f, 0, 1.5f
        );
        
        ImGui::Dummy(ImVec2(previewWidth, selectedHeight));
        
        // Display selected name (centered)
        float selectedTextWidth = ImGui::CalcTextSize(selectedName.c_str()).x;
        float selectedCenterX = 
            (ImGui::GetContentRegionAvail().x - selectedTextWidth) * 0.5f;
        if (selectedCenterX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + selectedCenterX);
        }
        ImGui::TextColored(
            ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", selectedName.c_str()
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Auto-switch to Paint toggle
        ImGui::Checkbox("Auto-switch to Paint", &m_canvasPanel.eyedropperAutoSwitchToPaint);
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Automatically switch to Paint tool\n"
                "after picking a color"
            );
        }
    }
    
    // Show palette when Paint tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Paint || m_canvasPanel.currentTool == CanvasPanel::Tool::Fill) {
        ImGui::Text("Paint Color");
        ImGui::Separator();
        
        // Display palette tiles inline
        for (const auto& tile : model.palette) {
            if (tile.id == 0) continue;  // Skip empty tile
            
            ImGui::PushID(tile.id);
            
            bool selected = (m_canvasPanel.selectedTileId == tile.id);
            ImVec4 color = tile.color.ToImVec4();
            bool inUse = model.IsPaletteColorInUse(tile.id);
            
            // Color button with visual indicator if in use
            ImGuiColorEditFlags colorBtnFlags = 0;
            if (ImGui::ColorButton("##color", color, colorBtnFlags, 
                                  ImVec2(24, 24))) {
                m_canvasPanel.selectedTileId = tile.id;
            }
            
            // Double-click to edit
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                // Open edit modal
                m_modals.colorPickerEditingTileId = tile.id;
                strncpy(m_modals.colorPickerName, tile.name.c_str(), 
                       sizeof(m_modals.colorPickerName) - 1);
                m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
                
                m_modals.colorPickerColor[0] = tile.color.r;
                m_modals.colorPickerColor[1] = tile.color.g;
                m_modals.colorPickerColor[2] = tile.color.b;
                m_modals.colorPickerColor[3] = tile.color.a;
                
                // Save original for preview
                m_modals.colorPickerOriginalColor[0] = tile.color.r;
                m_modals.colorPickerOriginalColor[1] = tile.color.g;
                m_modals.colorPickerOriginalColor[2] = tile.color.b;
                m_modals.colorPickerOriginalColor[3] = tile.color.a;
                
                m_modals.showColorPickerModal = true;
            }
            
            // Tooltip showing hex value and usage
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", tile.color.ToHex(false).c_str());
                if (inUse) {
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), 
                                      "In use");
                }
                ImGui::Text("Double-click to edit");
                ImGui::Text("Right-click for options");
                ImGui::EndTooltip();
            }
            
            // Right-click context menu
            if (ImGui::BeginPopupContextItem("color_context")) {
                ImGui::TextDisabled("%s", tile.name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Edit...")) {
                    // Open edit modal
                    m_modals.colorPickerEditingTileId = tile.id;
                    strncpy(m_modals.colorPickerName, tile.name.c_str(), 
                           sizeof(m_modals.colorPickerName) - 1);
                    m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
                    
                    m_modals.colorPickerColor[0] = tile.color.r;
                    m_modals.colorPickerColor[1] = tile.color.g;
                    m_modals.colorPickerColor[2] = tile.color.b;
                    m_modals.colorPickerColor[3] = tile.color.a;
                    
                    m_modals.colorPickerOriginalColor[0] = tile.color.r;
                    m_modals.colorPickerOriginalColor[1] = tile.color.g;
                    m_modals.colorPickerOriginalColor[2] = tile.color.b;
                    m_modals.colorPickerOriginalColor[3] = tile.color.a;
                    
                    m_modals.showColorPickerModal = true;
                }
                
                if (ImGui::MenuItem("Duplicate")) {
                    // Create a copy with "(Copy)" suffix
                    std::string newName = tile.name + " (Copy)";
                    auto cmd = std::make_unique<AddPaletteColorCommand>(
                        newName, tile.color);
                    history.AddCommand(std::move(cmd), model, true);
                    
                    ShowToast("Color duplicated: " + newName, 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                
                // Delete option with warning if in use
                if (inUse) {
                    ImGui::PushStyleColor(ImGuiCol_Text, 
                        ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                    if (ImGui::MenuItem("Delete (in use)...")) {
                        // Open delete confirmation
                        m_modals.colorPickerEditingTileId = tile.id;
                        m_modals.colorPickerDeleteRequested = true;
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::MenuItem("Delete")) {
                        // Direct delete without confirmation
                        auto cmd = std::make_unique<RemovePaletteColorCommand>(
                            tile.id, 0);
                        history.AddCommand(std::move(cmd), model, true);
                        
                        // If this was selected, switch to default
                        if (m_canvasPanel.selectedTileId == tile.id) {
                            m_canvasPanel.selectedTileId = 1;  // Default to "Solid"
                        }
                        
                        ShowToast("Color deleted: " + tile.name, 
                                 Toast::Type::Info);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::SameLine();
            
            // Selectable name
            if (ImGui::Selectable(tile.name.c_str(), selected, 0, 
                                  ImVec2(0, 24))) {
                m_canvasPanel.selectedTileId = tile.id;
            }
            
            // Same context menu on name
            if (ImGui::BeginPopupContextItem("name_context")) {
                ImGui::TextDisabled("%s", tile.name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Edit...")) {
                    m_modals.colorPickerEditingTileId = tile.id;
                    strncpy(m_modals.colorPickerName, tile.name.c_str(), 
                           sizeof(m_modals.colorPickerName) - 1);
                    m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
                    
                    m_modals.colorPickerColor[0] = tile.color.r;
                    m_modals.colorPickerColor[1] = tile.color.g;
                    m_modals.colorPickerColor[2] = tile.color.b;
                    m_modals.colorPickerColor[3] = tile.color.a;
                    
                    m_modals.colorPickerOriginalColor[0] = tile.color.r;
                    m_modals.colorPickerOriginalColor[1] = tile.color.g;
                    m_modals.colorPickerOriginalColor[2] = tile.color.b;
                    m_modals.colorPickerOriginalColor[3] = tile.color.a;
                    
                    m_modals.showColorPickerModal = true;
                }
                
                if (ImGui::MenuItem("Duplicate")) {
                    std::string newName = tile.name + " (Copy)";
                    auto cmd = std::make_unique<AddPaletteColorCommand>(
                        newName, tile.color);
                    history.AddCommand(std::move(cmd), model, true);
                    
                    ShowToast("Color duplicated: " + newName, 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                
                if (inUse) {
                    ImGui::PushStyleColor(ImGuiCol_Text, 
                        ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                    if (ImGui::MenuItem("Delete (in use)...")) {
                        m_modals.colorPickerEditingTileId = tile.id;
                        m_modals.colorPickerDeleteRequested = true;
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::MenuItem("Delete")) {
                        auto cmd = std::make_unique<RemovePaletteColorCommand>(
                            tile.id, 0);
                        history.AddCommand(std::move(cmd), model, true);
                        
                        if (m_canvasPanel.selectedTileId == tile.id) {
                            m_canvasPanel.selectedTileId = 1;
                        }
                        
                        ShowToast("Color deleted: " + tile.name, 
                                 Toast::Type::Info);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::PopID();
        }
        
        ImGui::Spacing();
        
        // Add Color button
        bool canAddMore = model.palette.size() < 32;
        
        if (!canAddMore) {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("+ Add Color", ImVec2(-1, 0))) {
            // Open modal in "new color" mode
            m_modals.colorPickerEditingTileId = -1;
            
            // Generate a smart default name
            int colorNum = static_cast<int>(model.palette.size());
            std::string defaultName = "Color " + std::to_string(colorNum);
            strncpy(m_modals.colorPickerName, defaultName.c_str(), 
                   sizeof(m_modals.colorPickerName) - 1);
            m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
            
            // Start with white color
            m_modals.colorPickerColor[0] = 1.0f;
            m_modals.colorPickerColor[1] = 1.0f;
            m_modals.colorPickerColor[2] = 1.0f;
            m_modals.colorPickerColor[3] = 1.0f;
            
            m_modals.showColorPickerModal = true;
        }
        
        if (!canAddMore) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Palette is full (max 32 colors)");
            }
        }
    }
    
    // Show eraser options when Erase tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Erase) {
        ImGui::Text("Eraser Options");
        ImGui::Separator();
        
        // Label on its own line
        ImGui::Text("Eraser Size");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of eraser brush in tiles\n"
                            "1 = single tile (precise)\n"
                            "5 = 5x5 area (fast erase)");
        }
        
        // Slider below label
        ImGui::SetNextItemWidth(-1);  // Full width
        ImGui::SliderInt("##eraserSize", &m_canvasPanel.eraserBrushSize, 1, 5);
        
        // Visual preview of eraser size
        ImGui::Spacing();
        ImGui::Text("Preview:");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        float previewSize = 80.0f;
        float cellSize = previewSize / 5.0f;  // 5x5 grid
        
        // Draw grid background
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                ImVec2 p0(cursorPos.x + x * cellSize, 
                         cursorPos.y + y * cellSize);
                ImVec2 p1(p0.x + cellSize, p0.y + cellSize);
                
                // Calculate if this cell is within brush
                int centerOffset = 2;  // Center of 5x5 grid
                int halfBrush = m_canvasPanel.eraserBrushSize / 2;
                bool inBrush = (x >= centerOffset - halfBrush && 
                               x <= centerOffset + halfBrush &&
                               y >= centerOffset - halfBrush && 
                               y <= centerOffset + halfBrush);
                
                // Draw cell
                ImU32 fillColor = inBrush ? 
                    ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 0.4f)) :
                    ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
                drawList->AddRectFilled(p0, p1, fillColor);
                
                // Draw grid lines
                ImU32 lineColor = ImGui::GetColorU32(
                    ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                drawList->AddRect(p0, p1, lineColor);
            }
        }
        
        ImGui::Dummy(ImVec2(previewSize, previewSize));
    }
    
    // Show marker options when Marker tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Marker) {
        ImGui::Text("Marker Settings");
        ImGui::Separator();
        
        // Helper lambda to capture current marker properties
        auto captureMarkerProps = [](const Marker* m) -> MarkerPropertiesSnapshot {
            MarkerPropertiesSnapshot snap;
            if (m) {
                snap.label = m->label;
                snap.icon = m->icon;
                snap.color = m->color;
                snap.showLabel = m->showLabel;
            }
            return snap;
        };
        
        // Label input
        char labelBuf[128];
        std::strncpy(labelBuf, m_canvasPanel.markerLabel.c_str(), 
                    sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
        
        // Capture state when label input becomes active
        if (ImGui::IsItemActive() == false) {
            // Will check activation after InputText
        }
        
        // Look up selected marker by ID (safe across vector reallocation)
        Marker* selMarker = model.FindMarker(m_canvasPanel.selectedMarkerId);
        
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
            m_canvasPanel.markerLabel = labelBuf;
            
            // Update selected marker if editing
            if (selMarker) {
                selMarker->label = m_canvasPanel.markerLabel;
                selMarker->showLabel = !m_canvasPanel.markerLabel.empty();
                model.MarkDirty();
            }
        }
        
        // Capture state when starting to edit label
        if (ImGui::IsItemActivated()) {
            if (selMarker) {
                // Editing a specific marker - clear palette style selection
                m_selectedPaletteStyleKey.clear();
                m_paletteStyleMarkerCount = 0;
                m_markerEditStartState = captureMarkerProps(selMarker);
                m_editingMarkerId = selMarker->id;
            }
        }
        
        // Handle label edit completion
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            // Check if editing from a palette style with multiple markers
            if (!m_selectedPaletteStyleKey.empty() && 
                m_paletteStyleMarkerCount > 1) {
                // Show rename dialog - ask "Rename All" or "Create New"
                m_modals.showMarkerLabelRenameModal = true;
                m_modals.markerLabelRenameStyleKey = m_selectedPaletteStyleKey;
                m_modals.markerLabelRenameNewLabel = m_canvasPanel.markerLabel;
                m_modals.markerLabelRenameCount = m_paletteStyleMarkerCount;
            } else if (selMarker && m_editingMarkerId == selMarker->id) {
                // Single marker selected - create undo command
                auto newProps = captureMarkerProps(selMarker);
                if (newProps != m_markerEditStartState) {
                    auto cmd = std::make_unique<ModifyMarkerPropertiesCommand>(
                        selMarker->id,
                        m_markerEditStartState,
                        newProps
                    );
                    history.AddCommand(std::move(cmd), model, false);
                }
            }
            // Else: no marker selected, just updated template - nothing to save
        }
        
        // Color picker (hex input)
        ImGui::Text("Color");
        ImGui::SameLine();
        
        // Color preview button (clickable to open picker)
        ImVec4 colorPreview = m_canvasPanel.markerColor.ToImVec4();
        if (ImGui::ColorButton("##colorpreview", colorPreview, 
                              ImGuiColorEditFlags_NoAlpha, 
                              ImVec2(40, 20))) {
            // Open color picker popup
            ImGui::OpenPopup("ColorPicker");
            // Capture state when popup opens
            if (selMarker) {
                m_markerEditStartState = captureMarkerProps(selMarker);
                m_editingMarkerId = selMarker->id;
            }
        }
        
        // Color picker popup
        bool colorPickerIsOpen = ImGui::BeginPopup("ColorPicker");
        if (colorPickerIsOpen) {
            float colorArray[4] = {
                m_canvasPanel.markerColor.r, m_canvasPanel.markerColor.g, 
                m_canvasPanel.markerColor.b, m_canvasPanel.markerColor.a
            };
            if (ImGui::ColorPicker4("##picker", colorArray, 
                                   ImGuiColorEditFlags_NoAlpha)) {
                m_canvasPanel.markerColor = Color(
                    colorArray[0], colorArray[1], 
                    colorArray[2], colorArray[3]
                );
                
                // Update hex input
                std::string hexStr = m_canvasPanel.markerColor.ToHex(false);
                std::strncpy(m_canvasPanel.markerColorHex, hexStr.c_str(), 
                            sizeof(m_canvasPanel.markerColorHex) - 1);
                m_canvasPanel.markerColorHex[
                    sizeof(m_canvasPanel.markerColorHex) - 1] = '\0';
                
                // Update selected marker if editing
                if (selMarker) {
                    selMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
            ImGui::EndPopup();
        }
        
        // Create command when color picker closes
        if (m_colorPickerWasOpen && !colorPickerIsOpen && 
            selMarker && m_editingMarkerId == selMarker->id) {
            auto newProps = captureMarkerProps(selMarker);
            if (newProps != m_markerEditStartState) {
                auto cmd = std::make_unique<ModifyMarkerPropertiesCommand>(
                    selMarker->id,
                    m_markerEditStartState,
                    newProps
                );
                history.AddCommand(std::move(cmd), model, false);
            }
        }
        m_colorPickerWasOpen = colorPickerIsOpen;
        
        // Hex input field
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputText("##colorhex", m_canvasPanel.markerColorHex, 
                            sizeof(m_canvasPanel.markerColorHex))) {
            // Parse hex input
            std::string hexStr(m_canvasPanel.markerColorHex);
            Color newColor = Color::FromHex(hexStr);
            
            // Only update if valid (not black unless intentional)
            if (!hexStr.empty() && hexStr[0] == '#') {
                m_canvasPanel.markerColor = newColor;
                
                // Update selected marker if editing
                if (selMarker) {
                    selMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
        }
        
        // Capture state when starting to edit hex color
        if (ImGui::IsItemActivated() && selMarker) {
            m_markerEditStartState = captureMarkerProps(selMarker);
            m_editingMarkerId = selMarker->id;
        }
        
        // Create command when hex edit finishes
        if (ImGui::IsItemDeactivatedAfterEdit() && selMarker &&
            m_editingMarkerId == selMarker->id) {
            auto newProps = captureMarkerProps(selMarker);
            if (newProps != m_markerEditStartState) {
                auto cmd = std::make_unique<ModifyMarkerPropertiesCommand>(
                    selMarker->id,
                    m_markerEditStartState,
                    newProps
                );
                history.AddCommand(std::move(cmd), model, false);
            }
        }
        
        // Show hint text
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Hex color (e.g., #4dcc4d)");
        }
        
        ImGui::Separator();
        
        // Import Icon button at the top
        if (ImGui::Button("Import Icon...", ImVec2(-1, 0))) {
            ImportIcon(icons, jobs);
        }
        
        // Show loading indicator if importing
        if (isImportingIcon) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "Loading...");
        }
        
        ImGui::Spacing();
        
        // Handle dropped file (OS-level file drop)
        if (hasDroppedFile) {
            // Check if it's an image file
            std::string ext = droppedFilePath.substr(
                droppedFilePath.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || 
                ext == "bmp" || ext == "gif" || ext == "tga" || ext == "webp") {
                // Extract filename without extension for icon name
                size_t lastSlash = droppedFilePath.find_last_of("/\\");
                size_t lastDot = droppedFilePath.find_last_of(".");
                std::string baseName = droppedFilePath.substr(
                    lastSlash + 1, 
                    lastDot - lastSlash - 1
                );
                
                // Generate unique name
                std::string iconName = icons.GenerateUniqueName(baseName);
                
                // Start async import
                importingIconName = iconName;
                isImportingIcon = true;
                
                // Capture variables for async processing
                std::string capturedIconName = iconName;
                std::string capturedFilePath = droppedFilePath;
                
                jobs.Enqueue(
                    JobType::ProcessIcon,
                    [capturedIconName, capturedFilePath, &icons]() {
                        std::vector<uint8_t> pixels;
                        int width, height;
                        std::string errorMsg;
                        
                        if (!IconManager::ProcessIconFromFile(
                                capturedFilePath, pixels, 
                                width, height, errorMsg)) {
                            throw std::runtime_error(errorMsg);
                        }
                        
                        if (!icons.AddIconFromMemory(capturedIconName, 
                                                     pixels.data(), 
                                                     width, height, "marker")) {
                            throw std::runtime_error(
                                "Failed to add icon to memory");
                        }
                    },
                    [this, &icons, capturedIconName](
                        bool success, const std::string& error) {
                        isImportingIcon = false;
                        if (success) {
                            // Build atlas on main thread (OpenGL required)
                            icons.BuildAtlas();
                            
                            ShowToast("Icon imported: " + capturedIconName, 
                                     Toast::Type::Success, 2.0f);
                            // Auto-select the imported icon
                            m_canvasPanel.selectedIconName = capturedIconName;
                        } else {
                            ShowToast("Failed to import: " + error, 
                                     Toast::Type::Error, 3.0f);
                        }
                    }
                );
            } else {
                ShowToast("Unsupported format. Use PNG, JPEG, BMP, GIF, "
                         "TGA, or WebP", 
                         Toast::Type::Warning, 3.0f);
            }
            
            hasDroppedFile = false;
            droppedFilePath.clear();
        }
        
        // Icon picker grid
        ImGui::Text("Select Icon");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Drag & drop image files here to import");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
            " | Right-click or Del key to delete");
        if (ImGui::BeginChild("IconPicker", ImVec2(0, 280), true)) {
            // Show loading overlay if importing
            if (isImportingIcon) {
                ImVec2 childSize = ImGui::GetWindowSize();
                ImVec2 textSize = ImGui::CalcTextSize(
                    ("Importing: " + importingIconName + "...").c_str());
                ImGui::SetCursorPos(ImVec2(
                    (childSize.x - textSize.x) * 0.5f,
                    (childSize.y - textSize.y) * 0.5f
                ));
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), 
                    "Importing: %s...", importingIconName.c_str());
            } else if (icons.GetIconCount() == 0) {
                ImGui::TextDisabled("No icons loaded");
                ImGui::TextDisabled("Click 'Import Icon...' to add icons");
            } else {
                // Get marker icon names (exclude tool icons)
                auto iconNames = icons.GetIconNamesByCategory("marker");
                
                // Responsive grid layout (no centering to avoid clipping)
                float buttonSize = 80.0f;  // Size of each icon button
                float spacing = 8.0f;
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                // Calculate responsive columns (min 2, max 4)
                int columns = std::max(2, 
                    static_cast<int>((availWidth + spacing) / 
                                    (buttonSize + spacing)));
                columns = std::min(columns, 4);
                
                for (size_t i = 0; i < iconNames.size(); ++i) {
                    const std::string& iconName = iconNames[i];
                    const Icon* icon = icons.GetIcon(iconName);
                    
                    if (!icon) continue;
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    // Grid layout without centering (prevents clipping)
                    if (i % columns != 0) {
                        ImGui::SameLine(0, spacing);
                    }
                    
                    ImGui::BeginGroup();
                    
                    // Draw icon button
                    bool isSelected = (m_canvasPanel.selectedIconName == iconName);
                    
                    // Highlight selected icon
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, 
                            ImVec4(0.2f, 0.4f, 0.8f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                            ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                            ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
                    }
                    
                    // Icon image button
                    if (icons.GetAtlasTexture()) {
                        ImVec2 uvMin(icon->u0, icon->v0);
                        ImVec2 uvMax(icon->u1, icon->v1);
                        
                        if (ImGui::ImageButton(
                            iconName.c_str(),
                            icons.GetAtlasTexture(),
                            ImVec2(buttonSize, buttonSize),
                            uvMin, uvMax)) {
                            // Update selected marker if editing (with undo)
                            // Look up marker fresh (may have changed)
                            Marker* iconSelMarker = 
                                model.FindMarker(m_canvasPanel.selectedMarkerId);
                            if (iconSelMarker) {
                                // Capture old state before change
                                MarkerPropertiesSnapshot oldProps;
                                oldProps.label = iconSelMarker->label;
                                oldProps.icon = iconSelMarker->icon;
                                oldProps.color = iconSelMarker->color;
                                oldProps.showLabel = iconSelMarker->showLabel;
                                
                                // Apply change
                                m_canvasPanel.selectedIconName = iconName;
                                iconSelMarker->icon = iconName;
                                model.MarkDirty();
                                
                                // Capture new state and create command
                                MarkerPropertiesSnapshot newProps = oldProps;
                                newProps.icon = iconName;
                                
                                auto cmd = 
                                    std::make_unique<ModifyMarkerPropertiesCommand>(
                                        iconSelMarker->id,
                                        oldProps,
                                        newProps
                                    );
                                history.AddCommand(std::move(cmd), model, false);
                            } else {
                                m_canvasPanel.selectedIconName = iconName;
                            }
                        }
                    } else {
                        // Fallback if no texture
                        if (ImGui::Button("##icon", 
                                         ImVec2(buttonSize, buttonSize))) {
                            // Update selected marker if editing (with undo)
                            // Look up marker fresh (may have changed)
                            Marker* iconSelMarker = 
                                model.FindMarker(m_canvasPanel.selectedMarkerId);
                            if (iconSelMarker) {
                                // Capture old state before change
                                MarkerPropertiesSnapshot oldProps;
                                oldProps.label = iconSelMarker->label;
                                oldProps.icon = iconSelMarker->icon;
                                oldProps.color = iconSelMarker->color;
                                oldProps.showLabel = iconSelMarker->showLabel;
                                
                                // Apply change
                                m_canvasPanel.selectedIconName = iconName;
                                iconSelMarker->icon = iconName;
                                model.MarkDirty();
                                
                                // Capture new state and create command
                                MarkerPropertiesSnapshot newProps = oldProps;
                                newProps.icon = iconName;
                                
                                auto cmd = 
                                    std::make_unique<ModifyMarkerPropertiesCommand>(
                                        iconSelMarker->id,
                                        oldProps,
                                        newProps
                                    );
                                history.AddCommand(std::move(cmd), model, false);
                            } else {
                                m_canvasPanel.selectedIconName = iconName;
                            }
                        }
                    }
                    
                    if (isSelected) {
                        ImGui::PopStyleColor(3);
                    }
                    
                    // Enable drag-drop from icon button
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        // Set payload with icon name
                        ImGui::SetDragDropPayload("MARKER_ICON", 
                            iconName.c_str(), 
                            iconName.length() + 1);
                        
                        // Show preview while dragging
                        if (icons.GetAtlasTexture()) {
                            ImVec2 uvMin(icon->u0, icon->v0);
                            ImVec2 uvMax(icon->u1, icon->v1);
                            ImGui::Image(icons.GetAtlasTexture(), 
                                ImVec2(32, 32), uvMin, uvMax);
                        }
                        ImGui::TextUnformatted(iconName.c_str());
                        
                        ImGui::EndDragDropSource();
                    }
                    
                    // Right-click context menu for custom icons
                    if (icon->category == "marker" && 
                        ImGui::BeginPopupContextItem(
                            ("icon_ctx_" + iconName).c_str())) {
                        ImGui::TextDisabled("Icon: %s", iconName.c_str());
                        ImGui::Separator();
                        
                        if (ImGui::MenuItem("Rename...")) {
                            m_modals.showRenameIconModal = true;
                            m_modals.renameIconOldName = iconName;
                            std::strncpy(m_modals.renameIconNewName, 
                                        iconName.c_str(), 
                                        sizeof(m_modals.renameIconNewName) - 1);
                            m_modals.renameIconNewName[sizeof(m_modals.renameIconNewName) - 1] 
                                = '\0';
                        }
                        
                        if (ImGui::MenuItem("Delete...")) {
                            m_modals.showDeleteIconModal = true;
                            m_modals.deleteIconName = iconName;
                            // Count markers using this icon
                            m_modals.deleteIconMarkerCount = 
                                model.CountMarkersUsingIcon(iconName);
                            // Get affected marker IDs (for display)
                            m_modals.deleteIconAffectedMarkers = 
                                model.GetMarkersUsingIcon(iconName);
                        }
                        
                        ImGui::EndPopup();
                    }
                    
                    // Icon name label below button (centered)
                    float textWidth = ImGui::CalcTextSize(iconName.c_str()).x;
                    float offset = (buttonSize - textWidth) * 0.5f;
                    if (offset > 0) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
                    }
                    ImGui::TextWrapped("%s", iconName.c_str());
                    
                    // Handle Delete key for selected icon
                    if (isSelected && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                        m_modals.showDeleteIconModal = true;
                        m_modals.deleteIconName = iconName;
                        // Count markers using this icon
                        m_modals.deleteIconMarkerCount = 
                            model.CountMarkersUsingIcon(iconName);
                        // Get affected marker IDs (for display)
                        m_modals.deleteIconAffectedMarkers = 
                            model.GetMarkersUsingIcon(iconName);
                    }
                    
                    ImGui::EndGroup();
                    
                    ImGui::PopID();
                }
            }
            
            // Show import progress
            if (isImportingIcon) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f),
                    "Importing: %s...", importingIconName.c_str());
            }
            
            ImGui::EndChild();
        }
        
        // ====================================================================
        // Placed Markers Palette
        // ====================================================================
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Placed Markers", 
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            // Group markers by (icon + color) to create "styles"
            struct MarkerStyle {
                std::string icon;
                Color color;
                std::string label;      // First label found (for display)
                int count = 0;          // How many markers use this style
            };
            
            // Use map with string key "icon:colorHex" for grouping
            std::map<std::string, MarkerStyle> styles;
            
            for (const auto& marker : model.markers) {
                // Create key from icon + color (round color to avoid near-dupes)
                std::string colorHex = marker.color.ToHex(false);
                std::string key = marker.icon + ":" + colorHex;
                
                auto& style = styles[key];
                if (style.count == 0) {
                    style.icon = marker.icon;
                    style.color = marker.color;
                    style.label = marker.label;
                }
                style.count++;
                // Prefer non-empty labels
                if (style.label.empty() && !marker.label.empty()) {
                    style.label = marker.label;
                }
            }
            
            if (styles.empty()) {
                ImGui::TextDisabled("No markers placed yet");
                ImGui::TextDisabled("Click on the map to place markers");
            } else {
                // Display each style as a selectable row
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                for (auto& [key, style] : styles) {
                    ImGui::PushID(key.c_str());
                    
                    // Check if this style matches current settings
                    bool isCurrentStyle = 
                        (m_canvasPanel.selectedIconName == style.icon &&
                         m_canvasPanel.markerColor.ToHex(false) == 
                             style.color.ToHex(false));
                    
                    // Highlight current style
                    if (isCurrentStyle) {
                        ImGui::PushStyleColor(ImGuiCol_Header, 
                            ImVec4(0.2f, 0.4f, 0.7f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 
                            ImVec4(0.3f, 0.5f, 0.8f, 0.6f));
                    }
                    
                    // Selectable row - clicking applies this style
                    bool clicked = ImGui::Selectable("##style", isCurrentStyle,
                        ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(availWidth, 28));
                    
                    if (isCurrentStyle) {
                        ImGui::PopStyleColor(2);
                    }
                    
                    // Draw content on top of selectable
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - availWidth + 8);
                    
                    // Icon preview (tinted)
                    const Icon* icon = icons.GetIcon(style.icon);
                    if (icon && icons.GetAtlasTexture()) {
                        ImVec2 uvMin(icon->u0, icon->v0);
                        ImVec2 uvMax(icon->u1, icon->v1);
                        ImVec4 tintCol = style.color.ToImVec4();
                        ImVec4 borderCol(0, 0, 0, 0);  // No border
                        
                        ImGui::Image(icons.GetAtlasTexture(),
                            ImVec2(24, 24), uvMin, uvMax, 
                            tintCol, borderCol);
                    } else {
                        // Fallback colored square
                        ImGui::ColorButton("##preview", 
                            style.color.ToImVec4(),
                            ImGuiColorEditFlags_NoTooltip | 
                            ImGuiColorEditFlags_NoBorder,
                            ImVec2(24, 24));
                    }
                    
                    ImGui::SameLine();
                    
                    // Label or icon name + count
                    std::string displayName = style.label.empty() ? 
                        style.icon : style.label;
                    if (style.count > 1) {
                        ImGui::Text("%s (x%d)", displayName.c_str(), 
                                   style.count);
                    } else {
                        ImGui::TextUnformatted(displayName.c_str());
                    }
                    
                    // Handle click - apply this style
                    if (clicked) {
                        m_canvasPanel.selectedIconName = style.icon;
                        m_canvasPanel.markerColor = style.color;
                        m_canvasPanel.markerLabel = style.label;
                        
                        // Track selected palette style for label rename feature
                        m_selectedPaletteStyleKey = key;
                        m_paletteStyleMarkerCount = style.count;
                        
                        // Update hex input
                        std::string hexStr = style.color.ToHex(false);
                        std::strncpy(m_canvasPanel.markerColorHex, 
                                    hexStr.c_str(),
                                    sizeof(m_canvasPanel.markerColorHex) - 1);
                        m_canvasPanel.markerColorHex[
                            sizeof(m_canvasPanel.markerColorHex) - 1] = '\0';
                        
                        // If a marker is selected, update it too (with undo)
                        Marker* styleMarker = 
                            model.FindMarker(m_canvasPanel.selectedMarkerId);
                        if (styleMarker) {
                            MarkerPropertiesSnapshot oldProps;
                            oldProps.label = styleMarker->label;
                            oldProps.icon = styleMarker->icon;
                            oldProps.color = styleMarker->color;
                            oldProps.showLabel = styleMarker->showLabel;
                            
                            styleMarker->icon = style.icon;
                            styleMarker->color = style.color;
                            model.MarkDirty();
                            
                            MarkerPropertiesSnapshot newProps = oldProps;
                            newProps.icon = style.icon;
                            newProps.color = style.color;
                            
                            auto cmd = std::make_unique<
                                ModifyMarkerPropertiesCommand>(
                                    styleMarker->id, oldProps, newProps);
                            history.AddCommand(std::move(cmd), model, false);
                        }
                    }
                    
                    // Tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Click to use this marker style\n"
                                         "Icon: %s\nColor: %s\nCount: %d",
                                         style.icon.c_str(),
                                         style.color.ToHex(false).c_str(),
                                         style.count);
                    }
                    
                    ImGui::PopID();
                }
            }
        }
        
        // Actions
        ImGui::Separator();
        
        if (selMarker) {
            ImGui::Text("Editing marker:");
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               selMarker->x, selMarker->y);
            
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                m_canvasPanel.selectedMarkerId.clear();
            }
        }
    }
    
    // Future: Add layers visibility toggles (grid, tiles, edges, markers)
    
    ImGui::End();
}

// Zoom preset levels (display percentages)
static const int ZOOM_PRESETS[] = {
    10, 25, 50, 75, 100, 150, 200, 400, 800, 1000
};
static const int ZOOM_PRESET_COUNT = 
    sizeof(ZOOM_PRESETS) / sizeof(ZOOM_PRESETS[0]);

// Convert display percentage to internal zoom value
static float DisplayToInternalZoom(int displayPercent) {
    return (displayPercent / 100.0f) * Canvas::DEFAULT_ZOOM;
}

// Convert internal zoom to display percentage
static int InternalToDisplayZoom(float internalZoom) {
    return static_cast<int>((internalZoom / Canvas::DEFAULT_ZOOM) * 100.0f);
}

// Find next zoom preset (higher or lower)
static int GetNextZoomPreset(int currentPercent, bool zoomIn) {
    if (zoomIn) {
        // Find first preset > current
        for (int i = 0; i < ZOOM_PRESET_COUNT; ++i) {
            if (ZOOM_PRESETS[i] > currentPercent) {
                return ZOOM_PRESETS[i];
            }
        }
        return ZOOM_PRESETS[ZOOM_PRESET_COUNT - 1];  // Max
    } else {
        // Find last preset < current
        for (int i = ZOOM_PRESET_COUNT - 1; i >= 0; --i) {
            if (ZOOM_PRESETS[i] < currentPercent) {
                return ZOOM_PRESETS[i];
            }
        }
        return ZOOM_PRESETS[0];  // Min
    }
}

void UI::RenderZoomOptions(Canvas& canvas, History& history, Model& model) {
    ImGui::Text("Zoom");
    ImGui::Separator();
    
    // Get current zoom as display percentage
    int currentPercent = InternalToDisplayZoom(canvas.zoom);
    float oldZoom = canvas.zoom;
    
    // Compact layout: [ - ] [ 100% ] [ + ]
    ImGui::Spacing();
    
    float buttonWidth = 28.0f;
    float inputWidth = 60.0f;
    float spacing = 4.0f;
    
    // Minus button
    if (ImGui::Button("-##zoomOut", ImVec2(buttonWidth, 0))) {
        int newPercent = GetNextZoomPreset(currentPercent, false);
        if (newPercent != currentPercent) {
            float newZoom = DisplayToInternalZoom(newPercent);
            auto cmd = std::make_unique<SetZoomCommand>(
                canvas, oldZoom, newZoom, newPercent
            );
            history.AddCommand(std::move(cmd), model);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom out to previous preset");
    }
    
    ImGui::SameLine(0, spacing);
    
    // Input field for direct percentage entry
    ImGui::SetNextItemWidth(inputWidth);
    static char zoomBuf[16];
    snprintf(zoomBuf, sizeof(zoomBuf), "%d%%", currentPercent);
    
    if (ImGui::InputText("##zoomInput", zoomBuf, sizeof(zoomBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
        // Parse input (strip % if present)
        int inputPercent = 0;
        if (sscanf(zoomBuf, "%d", &inputPercent) == 1) {
            // Clamp to valid range (10% - 1000%)
            inputPercent = std::clamp(inputPercent, 10, 1000);
            if (inputPercent != currentPercent) {
                float newZoom = DisplayToInternalZoom(inputPercent);
                auto cmd = std::make_unique<SetZoomCommand>(
                    canvas, oldZoom, newZoom, inputPercent
                );
                history.AddCommand(std::move(cmd), model);
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enter zoom percentage (10%% - 1000%%)");
    }
    
    ImGui::SameLine(0, spacing);
    
    // Plus button
    if (ImGui::Button("+##zoomIn", ImVec2(buttonWidth, 0))) {
        int newPercent = GetNextZoomPreset(currentPercent, true);
        if (newPercent != currentPercent) {
            float newZoom = DisplayToInternalZoom(newPercent);
            auto cmd = std::make_unique<SetZoomCommand>(
                canvas, oldZoom, newZoom, newPercent
            );
            history.AddCommand(std::move(cmd), model);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom in to next preset");
    }
}

void UI::RenderPropertiesPanel(Model& model, IconManager& icons, JobQueue& jobs,
                               History& history, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize;
    
    ImGui::Begin("Cartograph/Hierarchy", nullptr, flags);
    
    ImGui::Text("Hierarchy");
    ImGui::Separator();
    
    // ========================================================================
    // HIERARCHY SECTION
    // ========================================================================
    
    // Toolbar buttons
    if (ImGui::Button("+ Room")) {
        // Create new room via command (undoable)
        Room newRoom;
        newRoom.id = model.GenerateRoomId();
        newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
        newRoom.regionId = -1;
        newRoom.color = model.GenerateDistinctRoomColor();
        newRoom.cellsCacheDirty = true;
        newRoom.connectionsDirty = true;
        
        auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
        history.AddCommand(std::move(cmd), model);
        
        m_canvasPanel.activeRoomId = newRoom.id;
        AddConsoleMessage("Created " + newRoom.name, MessageType::Success);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("+ Region")) {
        // Create new region group via command (undoable)
        RegionGroup newRegion;
        newRegion.id = model.GenerateRegionGroupId();
        newRegion.name = "Region " + std::to_string(model.regionGroups.size() + 1);
        
        auto cmd = std::make_unique<CreateRegionCommand>(newRegion);
        history.AddCommand(std::move(cmd), model);
        
        AddConsoleMessage("Created " + newRegion.name, MessageType::Success);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Detect Rooms")) {
        // Create undoable command for room detection
        auto cmd = std::make_unique<DetectRoomsCommand>();
        cmd->Execute(model);
        
        int created = cmd->GetCreatedCount();
        int splitCount = cmd->GetSplitCount();
        
        // Build result message
        std::string message;
        if (splitCount > 0 && created > 0) {
            message = "Split " + std::to_string(splitCount) + 
                " disconnected region(s), created " + 
                std::to_string(created) + " new room(s)";
            AddConsoleMessage(message, MessageType::Success);
        } else if (splitCount > 0) {
            message = "Split " + std::to_string(splitCount) + 
                " disconnected region(s)";
            AddConsoleMessage(message, MessageType::Success);
        } else if (created > 0) {
            message = "Detected and created " + 
                std::to_string(created) + " room(s)";
            AddConsoleMessage(message, MessageType::Success);
        } else {
            AddConsoleMessage("No rooms to detect or split", 
                MessageType::Info);
        }
        
        // Only add to history if something changed
        if (created > 0 || splitCount > 0) {
            history.AddCommand(std::move(cmd), model, false);
        }
    }
    
    ImGui::SameLine();
    
    // Add Perimeter Walls button (greyed out if no rooms need walls)
    // Check if any room actually has edges that need walls added
    bool hasRooms = !model.rooms.empty();
    bool anyRoomNeedsWalls = false;
    if (hasRooms) {
        for (const auto& room : model.rooms) {
            auto changes = model.ComputeRoomPerimeterWallChanges(room.id);
            if (!changes.empty()) {
                anyRoomNeedsWalls = true;
                break;  // Found at least one room needing walls
            }
        }
    }
    
    bool canAddWalls = hasRooms && anyRoomNeedsWalls;
    
    if (!canAddWalls) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("Add Walls")) {
        // Collect all edge changes across all rooms for undo support
        std::vector<ModifyEdgesCommand::EdgeChange> allChanges;
        std::unordered_set<EdgeId, EdgeIdHash> seenEdges;  // Avoid duplicates
        
        for (const auto& room : model.rooms) {
            auto roomChanges = model.ComputeRoomPerimeterWallChanges(room.id);
            for (const auto& [edgeId, oldState, newState] : roomChanges) {
                if (seenEdges.insert(edgeId).second) {
                    ModifyEdgesCommand::EdgeChange change;
                    change.edgeId = edgeId;
                    change.oldState = oldState;
                    change.newState = newState;
                    allChanges.push_back(change);
                }
            }
        }
        
        if (!allChanges.empty()) {
            auto cmd = std::make_unique<ModifyEdgesCommand>(allChanges);
            history.AddCommand(std::move(cmd), model, true);
            AddConsoleMessage("Added " + std::to_string(allChanges.size()) + 
                " wall segment(s)", MessageType::Success);
        } else {
            AddConsoleMessage("No walls to add - rooms already have walls", 
                MessageType::Info);
        }
    }
    
    if (!canAddWalls) {
        ImGui::EndDisabled();
    }
    
    // Tooltip for the button
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!hasRooms) {
            ImGui::SetTooltip("No rooms detected yet.\n"
                            "Click 'Detect Rooms' first.");
        } else if (!anyRoomNeedsWalls) {
            ImGui::SetTooltip("All rooms already have walls.");
        } else {
            ImGui::SetTooltip("Add walls around all room boundaries\n"
                            "where there are no existing walls or doors.");
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Toggle room overlays (at top for easy access)
    ImGui::Checkbox("Show Room Overlays", &m_canvasPanel.showRoomOverlays);
    ImGui::Spacing();
    
    // Search/filter box
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search...", searchBuffer, 
                            sizeof(searchBuffer));
    std::string searchTerm(searchBuffer);
    
    // ========================================================================
    // UNITY-STYLE FLAT HIERARCHY LIST
    // ========================================================================
    
    // Track drag-and-drop state
    static std::string draggedRoomId = "";
    static std::string dragTargetRegionId = "";
    
    // Calculate dynamic height: reserve space for properties section
    // -50 for overlays toggle, -350 for properties section (dynamic based on selection)
    float hierarchyHeight = -400.0f;
    if (!m_canvasPanel.selectedRoomId.empty() || !m_canvasPanel.selectedRegionGroupId.empty()) {
        hierarchyHeight = -450.0f;  // More space when properties visible
    }
    
    // Begin child region for scrollable hierarchy
    ImGui::BeginChild("HierarchyList", ImVec2(0, hierarchyHeight), true);
    
    // Context menu on empty space for creating new items
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu")) {
        if (ImGui::MenuItem("Create Room")) {
            Room newRoom;
            newRoom.id = model.GenerateRoomId();
            newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
            newRoom.regionId = -1;
            newRoom.color = model.GenerateDistinctRoomColor();
            newRoom.cellsCacheDirty = true;
            newRoom.connectionsDirty = true;
            
            auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
            history.AddCommand(std::move(cmd), model);
            
            m_canvasPanel.selectedRoomId = newRoom.id;
            AddConsoleMessage("Created " + newRoom.name, MessageType::Success);
        }
        if (ImGui::MenuItem("Create Region")) {
            RegionGroup newRegion;
            newRegion.id = model.GenerateRegionGroupId();
            newRegion.name = "Region " + std::to_string(model.regionGroups.size() + 1);
            
            auto cmd = std::make_unique<CreateRegionCommand>(newRegion);
            history.AddCommand(std::move(cmd), model);
            
            m_canvasPanel.selectedRegionGroupId = newRegion.id;
            AddConsoleMessage("Created " + newRegion.name, MessageType::Success);
        }
        ImGui::EndPopup();
    }
    
    // Render all regions and unassigned rooms in a flat list
    bool hasAnyItems = false;
    
    // Helper for case-insensitive search
    auto containsIgnoreCase = [](const std::string& haystack, 
                                  const std::string& needle) -> bool {
        if (needle.empty()) return true;
        std::string lowerHaystack = haystack;
        std::string lowerNeedle = needle;
        std::transform(lowerHaystack.begin(), lowerHaystack.end(),
                      lowerHaystack.begin(), ::tolower);
        std::transform(lowerNeedle.begin(), lowerNeedle.end(),
                      lowerNeedle.begin(), ::tolower);
        return lowerHaystack.find(lowerNeedle) != std::string::npos;
    };
    
    // First, render all regions
    for (auto& region : model.regionGroups) {
        // Filter by search (case-insensitive)
        if (!searchTerm.empty() && !containsIgnoreCase(region.name, searchTerm)) {
            continue;
        }
        
        hasAnyItems = true;
        
        ImGui::PushID(region.id.c_str());
        
        // Region folder node
        ImGuiTreeNodeFlags nodeFlags = 
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        
        bool isRegionSelected = (m_canvasPanel.selectedRegionGroupId == region.id);
        if (isRegionSelected) {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }
        
        bool regionOpen = ImGui::TreeNodeEx(
            ("📁 " + region.name).c_str(), nodeFlags
        );
        
        // Click to select region
        if (ImGui::IsItemClicked()) {
            m_canvasPanel.selectedRegionGroupId = region.id;
            m_canvasPanel.selectedRoomId = "";  // Clear room selection
            
            // Navigate to region: calculate bounding box of all rooms in region
            int minX = INT_MAX, minY = INT_MAX;
            int maxX = INT_MIN, maxY = INT_MIN;
            bool hasRoomsInRegion = false;
            
            for (const auto& room : model.rooms) {
                if (room.parentRegionGroupId == region.id) {
                    auto cells = model.GetRoomCells(room.id);
                    for (const auto& cell : cells) {
                        minX = std::min(minX, cell.first);
                        maxX = std::max(maxX, cell.first);
                        minY = std::min(minY, cell.second);
                        maxY = std::max(maxY, cell.second);
                        hasRoomsInRegion = true;
                    }
                }
            }
            
            // Focus canvas on region if it has rooms
            if (hasRoomsInRegion) {
                canvas.FocusOnRect(minX, minY, maxX, maxY,
                                  model.grid.tileWidth, model.grid.tileHeight);
            }
        }
        
        // Drag-and-drop target: drop rooms onto region
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROOM_DRAG")) {
                std::string droppedRoomId = std::string((const char*)payload->Data, payload->DataSize);
                
                // Find the room and assign it to this region
                for (auto& room : model.rooms) {
                    if (room.id == droppedRoomId) {
                        std::string oldParent = room.parentRegionGroupId;
                        room.parentRegionGroupId = region.id;
                        model.MarkDirty();
                        AddConsoleMessage("Moved " + room.name + " to " + region.name, 
                                        MessageType::Success);
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        // Right-click context menu for region
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_modals.showRenameRegionDialog = true;
                m_modals.editingRegionId = region.id;
                strncpy(m_modals.renameBuffer, region.name.c_str(), 
                       sizeof(m_modals.renameBuffer) - 1);
                m_modals.renameBuffer[sizeof(m_modals.renameBuffer) - 1] = '\0';
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Region")) {
                m_modals.showDeleteRegionDialog = true;
                m_modals.editingRegionId = region.id;
            }
            ImGui::EndPopup();
        }
        
        if (regionOpen) {
            // Show rooms in this region
            int roomCount = 0;
            for (auto& room : model.rooms) {
                if (room.parentRegionGroupId == region.id) {
                    // Filter by search (case-insensitive)
                    if (!searchTerm.empty() && 
                        !containsIgnoreCase(room.name, searchTerm)) {
                        continue;
                    }
                    
                    roomCount++;
                    
                    ImGui::PushID(room.id.c_str());
                    
                    // Room leaf node
                    ImGuiTreeNodeFlags roomFlags = 
                        ImGuiTreeNodeFlags_Leaf |
                        ImGuiTreeNodeFlags_NoTreePushOnOpen |
                        ImGuiTreeNodeFlags_SpanAvailWidth;
                    
                    bool isSelected = (m_canvasPanel.selectedRoomId == room.id);
                    if (isSelected) {
                        roomFlags |= ImGuiTreeNodeFlags_Selected;
                    }
                    
                    // Color indicator
                    ImVec4 roomColor = room.color.ToImVec4();
                    ImGui::ColorButton("##color", roomColor, 
                                     ImGuiColorEditFlags_NoTooltip, 
                                     ImVec2(12, 12));
                    ImGui::SameLine();
                    
                    ImGui::TreeNodeEx(room.name.c_str(), roomFlags);
                    
                    // Auto-scroll to hovered room (from canvas RoomSelect tool)
                    if (m_canvasPanel.hoveredRoomId == room.id) {
                        ImGui::SetScrollHereY(0.5f);
                    }
                    
                    // Hover to highlight room on canvas
                    if (ImGui::IsItemHovered()) {
                        m_canvasPanel.hoveredRoomId = room.id;
                    }
                    
                    // Click to select room
                    if (ImGui::IsItemClicked()) {
                        m_canvasPanel.selectedRoomId = room.id;
                        m_canvasPanel.selectedRegionGroupId = "";  // Clear region selection
                        
                        // Navigate to room
                        auto cells = model.GetRoomCells(room.id);
                        if (!cells.empty()) {
                            // Calculate center
                            int minX = INT_MAX, minY = INT_MAX;
                            int maxX = INT_MIN, maxY = INT_MIN;
                            for (const auto& cell : cells) {
                                minX = std::min(minX, cell.first);
                                maxX = std::max(maxX, cell.first);
                                minY = std::min(minY, cell.second);
                                maxY = std::max(maxY, cell.second);
                            }
                            int centerX = (minX + maxX) / 2;
                            int centerY = (minY + maxY) / 2;
                            
                            // Focus camera on room center and zoom to 110%
                            canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                            canvas.FocusOnTile(centerX, centerY, 
                                             model.grid.tileWidth, 
                                             model.grid.tileHeight);
                        }
                    }
                    
                    // Drag source: drag room
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("ROOM_DRAG", room.id.c_str(), room.id.size());
                        ImGui::Text("Move %s", room.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    
                    // Right-click context menu for room
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Rename")) {
                            m_modals.showRenameRoomDialog = true;
                            m_modals.editingRoomId = room.id;
                            strncpy(m_modals.renameBuffer, room.name.c_str(), 
                                   sizeof(m_modals.renameBuffer) - 1);
                            m_modals.renameBuffer[sizeof(m_modals.renameBuffer) - 1] = '\0';
                        }
                        if (ImGui::MenuItem("Remove from Region")) {
                            room.parentRegionGroupId = "";
                            model.MarkDirty();
                            AddConsoleMessage("Removed " + room.name + " from region", 
                                            MessageType::Success);
                        }
                        if (ImGui::MenuItem("Add Perimeter Walls")) {
                            auto changes = model.ComputeRoomPerimeterWallChanges(
                                room.id);
                            if (!changes.empty()) {
                                std::vector<ModifyEdgesCommand::EdgeChange> ec;
                                for (const auto& [eId, oldS, newS] : changes) {
                                    ModifyEdgesCommand::EdgeChange c;
                                    c.edgeId = eId;
                                    c.oldState = oldS;
                                    c.newState = newS;
                                    ec.push_back(c);
                                }
                                auto cmd = std::make_unique<ModifyEdgesCommand>(ec);
                                history.AddCommand(std::move(cmd), model, true);
                                AddConsoleMessage("Added " + 
                                    std::to_string(changes.size()) + 
                                    " wall segment(s) to " + room.name,
                                    MessageType::Success);
                            } else {
                                AddConsoleMessage("No walls to add - " + 
                                    room.name + " already has perimeter walls",
                                    MessageType::Info);
                            }
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete Room")) {
                            m_modals.showDeleteRoomDialog = true;
                            m_modals.editingRoomId = room.id;
                        }
                        ImGui::EndPopup();
                    }
                    
                    ImGui::PopID();
                }
            }
            
            if (roomCount == 0) {
                ImGui::Indent();
                ImGui::TextDisabled("(empty)");
                ImGui::Unindent();
            }
            
            ImGui::TreePop();
        }
        
        ImGui::PopID();
    }
    
    // Then, render unassigned rooms (at root level)
    for (auto& room : model.rooms) {
        if (!room.parentRegionGroupId.empty()) {
            continue;  // Skip rooms in regions
        }
        
        // Filter by search (case-insensitive)
        if (!searchTerm.empty() && 
            !containsIgnoreCase(room.name, searchTerm)) {
            continue;
        }
        
        hasAnyItems = true;
        
        ImGui::PushID(room.id.c_str());
        
        // Room leaf node at root level
        ImGuiTreeNodeFlags roomFlags = 
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        
        bool isSelected = (m_canvasPanel.selectedRoomId == room.id);
        if (isSelected) {
            roomFlags |= ImGuiTreeNodeFlags_Selected;
        }
        
        // Color indicator
        ImVec4 roomColor = room.color.ToImVec4();
        ImGui::ColorButton("##color", roomColor, 
                         ImGuiColorEditFlags_NoTooltip, 
                         ImVec2(12, 12));
        ImGui::SameLine();
        
        ImGui::TreeNodeEx(room.name.c_str(), roomFlags);
        
        // Auto-scroll to hovered room (from canvas RoomSelect tool)
        if (m_canvasPanel.hoveredRoomId == room.id) {
            ImGui::SetScrollHereY(0.5f);
        }
        
        // Hover to highlight room on canvas
        if (ImGui::IsItemHovered()) {
            m_canvasPanel.hoveredRoomId = room.id;
        }
        
        // Click to select room
        if (ImGui::IsItemClicked()) {
            m_canvasPanel.selectedRoomId = room.id;
            m_canvasPanel.selectedRegionGroupId = "";  // Clear region selection
            
            // Navigate to room
            auto cells = model.GetRoomCells(room.id);
            if (!cells.empty()) {
                // Calculate center
                int minX = INT_MAX, minY = INT_MAX;
                int maxX = INT_MIN, maxY = INT_MIN;
                for (const auto& cell : cells) {
                    minX = std::min(minX, cell.first);
                    maxX = std::max(maxX, cell.first);
                    minY = std::min(minY, cell.second);
                    maxY = std::max(maxY, cell.second);
                }
                int centerX = (minX + maxX) / 2;
                int centerY = (minY + maxY) / 2;
                
                // Focus camera on room center and zoom to 110%
                canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                canvas.FocusOnTile(centerX, centerY, 
                                 model.grid.tileWidth, 
                                 model.grid.tileHeight);
            }
        }
        
        // Drag source: drag room
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("ROOM_DRAG", room.id.c_str(), room.id.size());
            ImGui::Text("Move %s", room.name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag-and-drop target: drop onto empty space to unparent
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROOM_DRAG")) {
                std::string droppedRoomId = std::string((const char*)payload->Data, payload->DataSize);
                
                // Find the room and remove from region
                for (auto& r : model.rooms) {
                    if (r.id == droppedRoomId) {
                        r.parentRegionGroupId = "";
                        model.MarkDirty();
                        AddConsoleMessage("Removed " + r.name + " from region", 
                                        MessageType::Success);
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        // Right-click context menu for room
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_modals.showRenameRoomDialog = true;
                m_modals.editingRoomId = room.id;
                strncpy(m_modals.renameBuffer, room.name.c_str(), 
                       sizeof(m_modals.renameBuffer) - 1);
                m_modals.renameBuffer[sizeof(m_modals.renameBuffer) - 1] = '\0';
            }
            
            // Show "Move to Region" submenu (disabled if no regions exist)
            bool hasRegions = !model.regionGroups.empty();
            if (!hasRegions) {
                ImGui::BeginDisabled();
            }
            if (ImGui::BeginMenu("Move to Region")) {
                for (auto& region : model.regionGroups) {
                    if (ImGui::MenuItem(region.name.c_str())) {
                        room.parentRegionGroupId = region.id;
                        model.MarkDirty();
                        AddConsoleMessage("Moved " + room.name + " to " + 
                            region.name, MessageType::Success);
                    }
                }
                ImGui::EndMenu();
            }
            if (!hasRegions) {
                ImGui::EndDisabled();
            }
            
            if (ImGui::MenuItem("Add Perimeter Walls")) {
                auto changes = model.ComputeRoomPerimeterWallChanges(room.id);
                if (!changes.empty()) {
                    std::vector<ModifyEdgesCommand::EdgeChange> ec;
                    for (const auto& [eId, oldS, newS] : changes) {
                        ModifyEdgesCommand::EdgeChange c;
                        c.edgeId = eId;
                        c.oldState = oldS;
                        c.newState = newS;
                        ec.push_back(c);
                    }
                    auto cmd = std::make_unique<ModifyEdgesCommand>(ec);
                    history.AddCommand(std::move(cmd), model, true);
                    AddConsoleMessage("Added " + std::to_string(changes.size()) +
                        " wall segment(s) to " + room.name,
                        MessageType::Success);
                } else {
                    AddConsoleMessage("No walls to add - " + room.name +
                        " already has perimeter walls", MessageType::Info);
                }
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Room")) {
                m_modals.showDeleteRoomDialog = true;
                m_modals.editingRoomId = room.id;
            }
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    }
    
    // Show message if no items
    if (!hasAnyItems) {
        ImGui::TextDisabled("No rooms or regions");
        ImGui::TextDisabled("Right-click to create");
    }
    
    ImGui::EndChild();  // End HierarchyList
    
    // ========================================================================
    // PROPERTIES SECTION (for selected room/region)
    // ========================================================================
    
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::BeginChild("PropertiesDetail", ImVec2(0, 0), true);
    ImGui::PopStyleColor();
    
    // Selected room details
    if (!m_canvasPanel.selectedRoomId.empty()) {
        Room* room = model.FindRoom(m_canvasPanel.selectedRoomId);
        if (room) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "ROOM PROPERTIES");
            ImGui::Separator();
            ImGui::Spacing();
            
            // Helper lambda to capture current room properties
            auto captureRoomProps = [](const Room* r) -> RoomPropertiesSnapshot {
                RoomPropertiesSnapshot snap;
                if (r) {
                    snap.name = r->name;
                    snap.color = r->color;
                    snap.notes = r->notes;
                    snap.tags = r->tags;
                }
                return snap;
            };
            
            {
                // Room name
                char nameBuf[256];
                strncpy(nameBuf, room->name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                    room->name = nameBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit name
                if (ImGui::IsItemActivated()) {
                    m_roomEditStartState = captureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                // Create command when name edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRoomId == room->id) {
                    auto newProps = captureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Room color (hex input like palette)
                static char roomColorHex[16] = "";
                if (roomColorHex[0] == '\0') {
                    // Initialize hex from current color
                    strncpy(roomColorHex, room->color.ToHex(false).c_str(), 
                           sizeof(roomColorHex) - 1);
                }
                
                ImGui::Text("Color:");
                ImGui::SameLine();
                
                // Color preview button
                ImVec4 colorPreview = room->color.ToImVec4();
                if (ImGui::ColorButton("##roomColorPreview", colorPreview,
                                      ImGuiColorEditFlags_NoTooltip,
                                      ImVec2(24, 24))) {
                    // Click opens full color picker - capture state
                    ImGui::OpenPopup("##roomColorPicker");
                    m_roomEditStartState = captureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                ImGui::SameLine();
                
                // Hex input field
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("##roomColorHex", roomColorHex, 
                                    sizeof(roomColorHex))) {
                    // Parse hex and update color
                    Color newColor = Color::FromHex(roomColorHex);
                    room->color = newColor;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit hex color
                if (ImGui::IsItemActivated()) {
                    m_roomEditStartState = captureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                // Create command when hex edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRoomId == room->id) {
                    auto newProps = captureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Full color picker popup
                bool roomColorPickerIsOpen = ImGui::BeginPopup("##roomColorPicker");
                if (roomColorPickerIsOpen) {
                    float colorArray[4] = {
                        room->color.r, room->color.g,
                        room->color.b, room->color.a
                    };
                    if (ImGui::ColorPicker4("##picker", colorArray)) {
                        room->color = Color(colorArray[0], colorArray[1],
                                          colorArray[2], colorArray[3]);
                        // Update hex display
                        strncpy(roomColorHex, room->color.ToHex(false).c_str(),
                               sizeof(roomColorHex) - 1);
                        model.MarkDirty();
                    }
                    ImGui::EndPopup();
                }
                
                // Create command when color picker closes
                if (m_roomColorPickerWasOpen && !roomColorPickerIsOpen && 
                    m_editingRoomId == room->id) {
                    auto newProps = captureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                m_roomColorPickerWasOpen = roomColorPickerIsOpen;
                
                // Room description
                ImGui::Spacing();
                ImGui::Text("Description:");
                char notesBuf[1024];
                strncpy(notesBuf, room->notes.c_str(), sizeof(notesBuf) - 1);
                if (ImGui::InputTextMultiline("##description", notesBuf, 
                                             sizeof(notesBuf), 
                                             ImVec2(-1, 80))) {
                    room->notes = notesBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit notes
                if (ImGui::IsItemActivated()) {
                    m_roomEditStartState = captureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                // Create command when notes edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRoomId == room->id) {
                    auto newProps = captureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Tags
                ImGui::Spacing();
                ImGui::Text("Tags:");
                
                // Display existing tags as chips
                bool tagRemoved = false;
                std::string tagToRemove;
                for (const auto& tag : room->tags) {
                    ImGui::PushID(tag.c_str());
                    
                    // Tag chip with X button
                    ImVec4 chipColor(0.3f, 0.5f, 0.8f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, chipColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                        ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                        ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
                    
                    std::string chipLabel = tag + " X";
                    if (ImGui::SmallButton(chipLabel.c_str())) {
                        tagToRemove = tag;
                        tagRemoved = true;
                    }
                    
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                    ImGui::PopID();
                }
                
                if (tagRemoved) {
                    // Capture old state before removing tag
                    auto oldProps = captureRoomProps(room);
                    
                    room->tags.erase(
                        std::remove(room->tags.begin(), room->tags.end(), 
                                   tagToRemove),
                        room->tags.end()
                    );
                    model.MarkDirty();
                    
                    // Create command for tag removal
                    auto newProps = captureRoomProps(room);
                    auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                        room->id, oldProps, newProps);
                    history.AddCommand(std::move(cmd), model, false);
                }
                
                // If tags exist, add newline; otherwise keep tight
                if (!room->tags.empty()) {
                    ImGui::NewLine();
                }
                
                // Add new tag (inline layout)
                static char newTagBuf[64] = "";
                ImGui::SetNextItemWidth(-70);
                bool enterPressed = ImGui::InputTextWithHint(
                    "##newtag", "Add tag...", newTagBuf, sizeof(newTagBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );
                
                ImGui::SameLine();
                bool addClicked = ImGui::Button("Add");
                
                if ((enterPressed || addClicked) && strlen(newTagBuf) > 0) {
                    std::string newTag(newTagBuf);
                    // Check if tag already exists
                    if (std::find(room->tags.begin(), room->tags.end(), newTag) == 
                        room->tags.end()) {
                        // Capture old state before adding tag
                        auto oldProps = captureRoomProps(room);
                        
                        room->tags.push_back(newTag);
                        model.MarkDirty();
                        
                        // Create command for tag addition
                        auto newProps = captureRoomProps(room);
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, oldProps, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                    newTagBuf[0] = '\0';  // Clear input
                }
                
                // Cell count (read-only)
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                const auto& cells = model.GetRoomCells(room->id);
                ImGui::Text("Cell Count: %zu", cells.size());
                
                // Calculate bounding box
                if (!cells.empty()) {
                    int minX = INT_MAX, minY = INT_MAX;
                    int maxX = INT_MIN, maxY = INT_MIN;
                    for (const auto& cell : cells) {
                        minX = std::min(minX, cell.first);
                        minY = std::min(minY, cell.second);
                        maxX = std::max(maxX, cell.first);
                        maxY = std::max(maxY, cell.second);
                    }
                    int width = maxX - minX + 1;
                    int height = maxY - minY + 1;
                    ImGui::Text("Dimensions: %d x %d", width, height);
                }
                
                // Connected rooms (via doors)
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Text("Connected Rooms:");
                
                // Update connections if dirty
                if (room->connectionsDirty) {
                    model.UpdateRoomConnections(room->id);
                }
                
                if (room->connectedRoomIds.empty()) {
                    ImGui::Indent();
                    ImGui::TextDisabled("No connections");
                    ImGui::Unindent();
                } else {
                    ImGui::Indent();
                    for (const auto& connectedId : room->connectedRoomIds) {
                        Room* connectedRoom = model.FindRoom(connectedId);
                        if (connectedRoom) {
                            ImGui::PushID(connectedId.c_str());
                            
                            // Make entire row clickable with hover effect
                            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,
                                              ImVec2(0.0f, 0.5f));
                            
                            // Show color indicator
                            ImVec4 connColor = connectedRoom->color.ToImVec4();
                            ImGui::ColorButton("##connColor", connColor,
                                             ImGuiColorEditFlags_NoTooltip |
                                             ImGuiColorEditFlags_NoPicker,
                                             ImVec2(12, 12));
                            ImGui::SameLine();
                            
                            // Clickable room name
                            if (ImGui::Selectable(connectedRoom->name.c_str(),
                                                 false,
                                                 ImGuiSelectableFlags_None)) {
                                m_canvasPanel.selectedRoomId = connectedId;
                                m_canvasPanel.activeRoomId = connectedId;
                                
                                // Navigate camera to the connected room
                                auto cells = model.GetRoomCells(connectedId);
                                if (!cells.empty()) {
                                    int minX = INT_MAX, minY = INT_MAX;
                                    int maxX = INT_MIN, maxY = INT_MIN;
                                    for (const auto& cell : cells) {
                                        minX = std::min(minX, cell.first);
                                        maxX = std::max(maxX, cell.first);
                                        minY = std::min(minY, cell.second);
                                        maxY = std::max(maxY, cell.second);
                                    }
                                    int centerX = (minX + maxX) / 2;
                                    int centerY = (minY + maxY) / 2;
                                    // Zoom to 110% and focus on room center
                                    canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                                    canvas.FocusOnTile(centerX, centerY,
                                        model.grid.tileWidth,
                                        model.grid.tileHeight);
                                }
                            }
                            
                            // Auto-scroll to hovered room (from canvas RoomSelect)
                            if (m_canvasPanel.hoveredRoomId == connectedId) {
                                ImGui::SetScrollHereY(0.5f);
                            }
                            
                            // Show hand cursor and highlight room on hover
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                                m_canvasPanel.hoveredRoomId = connectedId;
                            }
                            
                            ImGui::PopStyleVar();
                            ImGui::PopID();
                        }
                    }
                    ImGui::Unindent();
                }
                
                // Room actions
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // Add Perimeter Walls button
                if (ImGui::Button("Add Perimeter Walls", ImVec2(-1, 0))) {
                    auto changes = model.ComputeRoomPerimeterWallChanges(
                        room->id);
                    if (!changes.empty()) {
                        std::vector<ModifyEdgesCommand::EdgeChange> ec;
                        for (const auto& [eId, oldS, newS] : changes) {
                            ModifyEdgesCommand::EdgeChange c;
                            c.edgeId = eId;
                            c.oldState = oldS;
                            c.newState = newS;
                            ec.push_back(c);
                        }
                        auto cmd = std::make_unique<ModifyEdgesCommand>(ec);
                        history.AddCommand(std::move(cmd), model, true);
                        AddConsoleMessage("Added " + 
                            std::to_string(changes.size()) + 
                            " wall segment(s) to " + room->name,
                            MessageType::Success);
                    } else {
                        AddConsoleMessage("No walls to add - " + room->name +
                            " already has perimeter walls", MessageType::Info);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Add walls around room boundaries\n"
                        "where there are no existing walls");
                }
                
                ImGui::Spacing();
                
                // Delete room button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                
                if (ImGui::Button("Delete Room", ImVec2(-1, 0))) {
                    // Confirm deletion
                    m_modals.editingRoomId = room->id;
                    m_modals.showDeleteRoomDialog = true;
                }
                
                ImGui::PopStyleColor(3);
            }
        }
    }
    
    // Selected region details
    if (!m_canvasPanel.selectedRegionGroupId.empty()) {
        RegionGroup* region = model.FindRegionGroup(m_canvasPanel.selectedRegionGroupId);
        if (region) {
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.5f, 1.0f), "REGION PROPERTIES");
            ImGui::Separator();
            ImGui::Spacing();
            
            // Helper lambda to capture current region properties
            auto captureRegionProps = [](const RegionGroup* r) 
                -> RegionPropertiesSnapshot {
                RegionPropertiesSnapshot snap;
                if (r) {
                    snap.name = r->name;
                    snap.description = r->description;
                    snap.tags = r->tags;
                }
                return snap;
            };
            
            {
                // Region name
                char nameBuf[256];
                strncpy(nameBuf, region->name.c_str(), sizeof(nameBuf) - 1);
                nameBuf[sizeof(nameBuf) - 1] = '\0';
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                    region->name = nameBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit name
                if (ImGui::IsItemActivated()) {
                    m_regionEditStartState = captureRegionProps(region);
                    m_editingRegionId = region->id;
                }
                
                // Create command when name edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRegionId == region->id) {
                    auto newProps = captureRegionProps(region);
                    if (newProps != m_regionEditStartState) {
                        auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                            region->id, m_regionEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Region description
                ImGui::Spacing();
                ImGui::Text("Description:");
                char descBuf[1024];
                strncpy(descBuf, region->description.c_str(), sizeof(descBuf) - 1);
                descBuf[sizeof(descBuf) - 1] = '\0';
                if (ImGui::InputTextMultiline("##regionDescription", descBuf, 
                                             sizeof(descBuf), 
                                             ImVec2(-1, 80))) {
                    region->description = descBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit description
                if (ImGui::IsItemActivated()) {
                    m_regionEditStartState = captureRegionProps(region);
                    m_editingRegionId = region->id;
                }
                
                // Create command when description edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRegionId == region->id) {
                    auto newProps = captureRegionProps(region);
                    if (newProps != m_regionEditStartState) {
                        auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                            region->id, m_regionEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Tags
                ImGui::Spacing();
                ImGui::Text("Tags:");
                
                // Display existing tags as chips
                bool tagRemoved = false;
                std::string tagToRemove;
                for (const auto& tag : region->tags) {
                    ImGui::PushID(tag.c_str());
                    
                    // Tag chip with X button
                    ImVec4 chipColor(0.3f, 0.5f, 0.8f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, chipColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                        ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                        ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
                    
                    std::string chipLabel = tag + " X";
                    if (ImGui::SmallButton(chipLabel.c_str())) {
                        tagToRemove = tag;
                        tagRemoved = true;
                    }
                    
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                    ImGui::PopID();
                }
                
                if (tagRemoved) {
                    // Capture old state before removing tag
                    auto oldProps = captureRegionProps(region);
                    
                    region->tags.erase(
                        std::remove(region->tags.begin(), region->tags.end(), 
                                   tagToRemove),
                        region->tags.end()
                    );
                    model.MarkDirty();
                    
                    // Create command for tag removal
                    auto newProps = captureRegionProps(region);
                    auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                        region->id, oldProps, newProps);
                    history.AddCommand(std::move(cmd), model, false);
                }
                
                // If tags exist, add newline; otherwise keep tight
                if (!region->tags.empty()) {
                    ImGui::NewLine();
                }
                
                // Add new tag (inline layout)
                static char newRegionTagBuf[64] = "";
                ImGui::SetNextItemWidth(-70);
                bool enterPressed = ImGui::InputTextWithHint(
                    "##newregiontag", "Add tag...", newRegionTagBuf, 
                    sizeof(newRegionTagBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );
                
                ImGui::SameLine();
                bool addClicked = ImGui::Button("Add");
                
                if ((enterPressed || addClicked) && strlen(newRegionTagBuf) > 0) {
                    std::string newTag(newRegionTagBuf);
                    // Check if tag already exists
                    if (std::find(region->tags.begin(), region->tags.end(), newTag) == 
                        region->tags.end()) {
                        // Capture old state before adding tag
                        auto oldProps = captureRegionProps(region);
                        
                        region->tags.push_back(newTag);
                        model.MarkDirty();
                        
                        // Create command for tag addition
                        auto newProps = captureRegionProps(region);
                        auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                            region->id, oldProps, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                    newRegionTagBuf[0] = '\0';  // Clear input
                }
                
                // Rooms in this region
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Text("Rooms in Region:");
                
                // Count and list rooms
                std::vector<Room*> roomsInRegion;
                for (auto& room : model.rooms) {
                    if (room.parentRegionGroupId == region->id) {
                        roomsInRegion.push_back(&room);
                    }
                }
                
                if (roomsInRegion.empty()) {
                    ImGui::Indent();
                    ImGui::TextDisabled("No rooms in this region");
                    ImGui::Unindent();
                } else {
                    ImGui::Indent();
                    ImGui::Text("Count: %zu", roomsInRegion.size());
                    ImGui::Spacing();
                    
                    for (Room* room : roomsInRegion) {
                        ImGui::PushID(room->id.c_str());
                        
                        // Make entire row clickable with hover effect
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,
                                          ImVec2(0.0f, 0.5f));
                        
                        // Show color indicator
                        ImVec4 roomColor = room->color.ToImVec4();
                        ImGui::ColorButton("##roomColor", roomColor,
                                         ImGuiColorEditFlags_NoTooltip |
                                         ImGuiColorEditFlags_NoPicker,
                                         ImVec2(12, 12));
                        ImGui::SameLine();
                        
                        // Clickable room name (navigate to room)
                        if (ImGui::Selectable(room->name.c_str(),
                                             false,
                                             ImGuiSelectableFlags_None)) {
                            m_canvasPanel.selectedRoomId = room->id;
                            m_canvasPanel.selectedRegionGroupId = "";  // Switch to room
                            
                            // Navigate camera to room
                            auto cells = model.GetRoomCells(room->id);
                            if (!cells.empty()) {
                                int minX = INT_MAX, minY = INT_MAX;
                                int maxX = INT_MIN, maxY = INT_MIN;
                                for (const auto& cell : cells) {
                                    minX = std::min(minX, cell.first);
                                    maxX = std::max(maxX, cell.first);
                                    minY = std::min(minY, cell.second);
                                    maxY = std::max(maxY, cell.second);
                                }
                                int centerX = (minX + maxX) / 2;
                                int centerY = (minY + maxY) / 2;
                                // Zoom to 110% and focus on room center
                                canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                                canvas.FocusOnTile(centerX, centerY, 
                                                 model.grid.tileWidth, 
                                                 model.grid.tileHeight);
                            }
                        }
                        
                        // Auto-scroll to hovered room (from canvas RoomSelect)
                        if (m_canvasPanel.hoveredRoomId == room->id) {
                            ImGui::SetScrollHereY(0.5f);
                        }
                        
                        // Show hand cursor and highlight room on canvas
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            m_canvasPanel.hoveredRoomId = room->id;
                        }
                        
                        ImGui::PopStyleVar();
                        ImGui::PopID();
                    }
                    ImGui::Unindent();
                }
                
                // Delete region button
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                
                if (ImGui::Button("Delete Region", ImVec2(-1, 0))) {
                    // Confirm deletion
                    m_modals.editingRegionId = region->id;
                    m_modals.showDeleteRegionDialog = true;
                }
                
                ImGui::PopStyleColor(3);
            }
        }
    }
    
    // Show message if nothing selected
    if (m_canvasPanel.selectedRoomId.empty() && m_canvasPanel.selectedRegionGroupId.empty()) {
        ImGui::TextDisabled("No room or region selected");
        ImGui::TextDisabled("Select an item from the hierarchy above");
    }
    
    ImGui::EndChild();  // End PropertiesDetail
    
    // New room dialog
    if (m_modals.showNewRoomDialog) {
        ImGui::OpenPopup("Create New Room");
        m_modals.showNewRoomDialog = false;
    }
    
    if (ImGui::BeginPopupModal("Create New Room", nullptr, 
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Room Name", m_modals.newRoomName, sizeof(m_modals.newRoomName));
        ImGui::ColorEdit3("Room Color", m_modals.newRoomColor);
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Create room via command (undoable)
            Room newRoom;
            newRoom.id = model.GenerateRoomId();
            newRoom.name = m_modals.newRoomName;
            newRoom.color = Color(m_modals.newRoomColor[0], m_modals.newRoomColor[1], 
                                 m_modals.newRoomColor[2], 1.0f);
            newRoom.regionId = -1;
            newRoom.cellsCacheDirty = true;
            newRoom.connectionsDirty = true;
            
            auto cmd = std::make_unique<CreateRoomCommand>(newRoom);
            history.AddCommand(std::move(cmd), model);
            
            m_canvasPanel.selectedRoomId = newRoom.id;
            
            // Reset form
            strncpy(m_modals.newRoomName, "New Room", 
                    sizeof(m_modals.newRoomName) - 1);
            m_modals.newRoomName[sizeof(m_modals.newRoomName) - 1] = '\0';
            m_modals.newRoomColor[0] = 1.0f;
            m_modals.newRoomColor[1] = 0.5f;
            m_modals.newRoomColor[2] = 0.5f;
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Note: Marker tool settings moved to Tools panel (shown when Marker 
    // tool is selected)
    
    ImGui::End();
}



void UI::RenderStatusBar(Model& model, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    
    // Check if we have an active error to display
    if (!m_statusError.empty() && m_statusErrorTime > 0.0f) {
        // Red background for errors
        ImGui::PushStyleColor(ImGuiCol_WindowBg, 
                             ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
        ImGui::Begin("Cartograph/Console", nullptr, flags);
        ImGui::PopStyleColor();
        
        // Display error prominently
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                          "⚠ ERROR: %s", m_statusError.c_str());
        
        // Countdown the error display time
        m_statusErrorTime -= 0.016f;  // Approximate frame time
        if (m_statusErrorTime <= 0.0f) {
            m_statusError.clear();
        }
    } else {
        // Normal status bar
        ImGui::Begin("Cartograph/Console", nullptr, flags);
        
        // Left section: Tile coordinates (if hovering canvas)
        if (m_canvasPanel.isHoveringCanvas) {
            ImGui::Text("Tile: %d, %d", m_canvasPanel.hoveredTileX, 
                       m_canvasPanel.hoveredTileY);
        } else {
            ImGui::TextDisabled("Tile: --, --");
        }
        
        // Separator
        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("|");
        
        // Middle section: Zoom
        ImGui::SameLine(0, 10);
        // Display zoom relative to default (2.5 internal = 100% display)
        float displayZoom = (canvas.zoom / Canvas::DEFAULT_ZOOM) * 100.0f;
        ImGui::Text("Zoom: %.0f%%", displayZoom);
        
        // Selection info (if there's an active selection)
        if (m_canvasPanel.hasSelection && 
            !m_canvasPanel.currentSelection.IsEmpty()) {
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 10);
            
            int tileCount = m_canvasPanel.currentSelection.TileCount();
            int edgeCount = m_canvasPanel.currentSelection.EdgeCount();
            int markerCount = m_canvasPanel.currentSelection.MarkerCount();
            
            // Build selection summary string
            std::string selInfo = "Selected: ";
            bool first = true;
            if (tileCount > 0) {
                selInfo += std::to_string(tileCount) + " tile";
                if (tileCount != 1) selInfo += "s";
                first = false;
            }
            if (edgeCount > 0) {
                if (!first) selInfo += ", ";
                selInfo += std::to_string(edgeCount) + " edge";
                if (edgeCount != 1) selInfo += "s";
                first = false;
            }
            if (markerCount > 0) {
                if (!first) selInfo += ", ";
                selInfo += std::to_string(markerCount) + " marker";
                if (markerCount != 1) selInfo += "s";
            }
            
            ImGui::TextColored(
                ImVec4(0.4f, 0.7f, 1.0f, 1.0f), 
                "%s", selInfo.c_str()
            );
        }
        
        // Separator
        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("|");
        
        // Console message section (remaining space)
        ImGui::SameLine(0, 10);
        
        if (!m_consoleMessages.empty()) {
            // Get the most recent message
            const ConsoleMessage& lastMsg = m_consoleMessages.back();
            
            // Choose icon and color based on message type
            const char* icon = "";
            ImVec4 color;
            switch (lastMsg.type) {
                case MessageType::Info:
                    icon = "ℹ";
                    color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);  // Light blue
                    break;
                case MessageType::Success:
                    icon = "✓";
                    color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);  // Green
                    break;
                case MessageType::Warning:
                    icon = "⚠";
                    color = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);  // Orange
                    break;
                case MessageType::Error:
                    icon = "✖";
                    color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
                    break;
            }
            
            // Display message with icon
            ImGui::TextColored(color, "%s", icon);
            ImGui::SameLine(0, 5);
            
            // Truncate long messages to single line (max 120 chars)
            std::string displayMsg = lastMsg.message;
            
            // Replace newlines with spaces
            for (char& c : displayMsg) {
                if (c == '\n' || c == '\r') {
                    c = ' ';
                }
            }
            
            // Truncate if too long
            const size_t maxLen = 120;
            if (displayMsg.length() > maxLen) {
                displayMsg = displayMsg.substr(0, maxLen - 3) + "...";
            }
            
            // Calculate time since message (for fading effect)
            double age = Platform::GetTime() - lastMsg.timestamp;
            if (age > 5.0) {
                // Fade out old messages
                ImGui::TextDisabled("%s", displayMsg.c_str());
            } else {
                ImGui::Text("%s", displayMsg.c_str());
            }
            
            // Right-click context menu to copy message
            if (ImGui::BeginPopupContextItem("##consoleMsgContext")) {
                if (ImGui::MenuItem("Copy Message")) {
                    ImGui::SetClipboardText(lastMsg.message.c_str());
                }
                ImGui::EndPopup();
            }
        } else {
            // No messages yet
            ImGui::TextDisabled("Ready");
        }
    }
    
    ImGui::PopStyleVar();
    ImGui::End();
}

void UI::RenderToasts(float deltaTime) {
    // Update and render toasts
    float yOffset = 100.0f;
    
    for (auto it = m_toasts.begin(); it != m_toasts.end(); ) {
        it->remainingTime -= deltaTime;
        
        if (it->remainingTime <= 0.0f) {
            it = m_toasts.erase(it);
            continue;
        }
        
        // Render toast
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;
        
        ImGui::SetNextWindowPos(
            ImVec2(workPos.x + workSize.x - 320, workPos.y + yOffset)
        );
        ImGui::SetNextWindowSize(ImVec2(300, 0));
        
        ImGuiWindowFlags flags = 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoInputs;
        
        ImGui::Begin(("##toast" + std::to_string((size_t)&(*it))).c_str(), 
            nullptr, flags);
        ImGui::Text("%s", it->message.c_str());
        ImGui::End();
        
        yOffset += 60.0f;
        ++it;
    }
}

void UI::BuildFixedLayout(ImGuiID dockspaceId) {
    // Clear any existing layout
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, 
        ImGuiDockNodeFlags_DockSpace);
    
    // Set dockspace size to fill the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);
    
    // Split bottom: 28px from bottom for status bar
    ImGuiID bottomId = 0;
    ImGuiID topRestId = 0;
    ImGui::DockBuilderSplitNode(
        dockspaceId, ImGuiDir_Down, 28.0f / viewport->WorkSize.y, 
        &bottomId, &topRestId
    );
    
    // Split left: 220px from left for tools (fixed width, non-resizable)
    ImGuiID leftId = 0;
    ImGuiID remainingId = 0;
    ImGui::DockBuilderSplitNode(
        topRestId, ImGuiDir_Left, 220.0f / viewport->WorkSize.x, 
        &leftId, &remainingId
    );
    
    ImGuiID centerId = remainingId;  // Default: center takes all remaining space
    ImGuiID rightId = 0;
    
    if (showPropertiesPanel) {
        // 3-column mode: Split right for hierarchy panel (360px)
        float rightWidth = 320.0f / (viewport->WorkSize.x - 220.0f);
        ImGui::DockBuilderSplitNode(
            remainingId, ImGuiDir_Right, rightWidth,
            &rightId, &centerId
        );
    }
    
    // Dock windows into their respective nodes
    ImGui::DockBuilderDockWindow("Cartograph/Tools", leftId);
    ImGui::DockBuilderDockWindow("Cartograph/Canvas", centerId);
    ImGui::DockBuilderDockWindow("Cartograph/Console", bottomId);
    
    if (showPropertiesPanel) {
        ImGui::DockBuilderDockWindow("Cartograph/Hierarchy", rightId);
    }
    
    // Configure node flags
    ImGuiDockNode* leftNode = ImGui::DockBuilderGetNode(leftId);
    ImGuiDockNode* centerNode = ImGui::DockBuilderGetNode(centerId);
    ImGuiDockNode* bottomNode = ImGui::DockBuilderGetNode(bottomId);
    
    if (leftNode) {
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoResize;  // Non-resizable
    }
    
    if (centerNode) {
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
    }
    
    if (showPropertiesPanel) {
        ImGuiDockNode* rightNode = ImGui::DockBuilderGetNode(rightId);
        if (rightNode) {
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoResize;  // Non-resizable
        }
    }
    
    if (bottomNode) {
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoResize;  // Non-resizable
    }
    
    // Finish building
    ImGui::DockBuilderFinish(dockspaceId);
}

void UI::ImportIcon(IconManager& iconManager, JobQueue& jobs) {
    // Use SDL3 native file dialog
    
    // Setup filters for image files
    static SDL_DialogFileFilter filters[] = {
        { "All Images", "png;jpg;jpeg;bmp;gif;tga;webp" },
        { "PNG Files", "png" },
        { "JPEG Files", "jpg;jpeg" },
        { "BMP Files", "bmp" },
        { "GIF Files", "gif" },
        { "TGA Files", "tga" },
        { "WebP Files", "webp" }
    };
    
    // Callback struct to capture references
    struct CallbackData {
        UI* ui;
        IconManager* iconManager;
        JobQueue* jobs;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->iconManager = &iconManager;
    dataPtr->jobs = &jobs;
    
    // Show native file dialog
    SDL_ShowOpenFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open file dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a file
            std::string path = filelist[0];
            
            // Extract base name from path
            namespace fs = std::filesystem;
            std::string baseName = fs::path(path).stem().string();
            
            // Generate unique name
            std::string iconName = data->iconManager->GenerateUniqueName(baseName);
            
            // Set loading state
            data->ui->isImportingIcon = true;
            data->ui->importingIconName = iconName;
            
            // Transfer ownership to job callback using shared_ptr
            // (job callback outlives this SDL callback)
            auto dataShared = std::shared_ptr<CallbackData>(data.release());
            
            // Enqueue processing job
            dataShared->jobs->Enqueue(
                JobType::ProcessIcon,
                [path, iconName, dataShared]() {
                    std::vector<uint8_t> pixels;
                    int width, height;
                    std::string errorMsg;
                    
                    if (!IconManager::ProcessIconFromFile(path, pixels, 
                                                          width, height, errorMsg)) {
                        throw std::runtime_error(errorMsg);
                    }
                    
                    if (!dataShared->iconManager->AddIconFromMemory(
                            iconName, pixels.data(), width, height, "marker")) {
                        throw std::runtime_error("Failed to add icon to memory");
                    }
                },
                [dataShared, iconName](bool success, const std::string& error) {
                    dataShared->ui->isImportingIcon = false;
                    
                    if (success) {
                        // Build atlas on main thread
                        dataShared->iconManager->BuildAtlas();
                        
                        dataShared->ui->ShowToast("Icon imported: " + iconName, 
                                          Toast::Type::Success, 2.0f);
                        dataShared->ui->m_canvasPanel.selectedIconName = iconName;
                    } else {
                        dataShared->ui->ShowToast("Failed to import: " + error, 
                                          Toast::Type::Error, 3.0f);
                    }
                    
                    // shared_ptr cleans up automatically when all lambdas done
                }
            );
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now, could pass SDL window)
        nullptr,
        // Filters
        filters,
        // Number of filters
        7,
        // Default location
        nullptr,
        // Allow multiple files
        false
    );
}

void UI::ShowSaveProjectDialog(App& app) {
    // Callback struct
    struct CallbackData {
        UI* ui;
        App* app;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->app = &app;
    
    // Show native folder picker dialog
    SDL_ShowOpenFolderDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open folder dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a folder
            std::string folderPath = filelist[0];
            
            // Save project to folder
            data->app->SaveProjectFolder(folderPath);
            
            // unique_ptr cleans up automatically at end of scope
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

void UI::ShowExportPackageDialog(App& app) {
    // Callback struct
    struct CallbackData {
        UI* ui;
        App* app;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->app = &app;
    
    // Setup filter for .cart files
    static SDL_DialogFileFilter filter = { 
        "Cartograph Package", "cart" 
    };
    
    // Build default filename from project title
    std::string defaultFilename = 
        SanitizeFilename(app.GetModel().meta.title) + ".cart";
    
    // Show native save file dialog
    SDL_ShowSaveFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filterIndex) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open save dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a path
            std::string path = filelist[0];
            
            // Ensure .cart extension
            if (path.size() < 5 || 
                path.substr(path.size() - 5) != ".cart") {
                path += ".cart";
            }
            
            // Export package
            data->app->ExportPackage(path);
            
            // unique_ptr cleans up automatically at end of scope
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Filters
        &filter,
        // Number of filters
        1,
        // Default filename (pre-filled in save dialog)
        defaultFilename.c_str()
    );
}

void UI::ShowExportPngDialog(App& app) {
    // Callback struct
    struct CallbackData {
        UI* ui;
        App* app;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->app = &app;
    
    // Setup filters for PNG files
    static SDL_DialogFileFilter filters[] = {
        { "PNG Image", "png" },
        { "All Files", "*" }
    };
    
    // Build default filename from project title
    std::string defaultFilename = 
        SanitizeFilename(app.GetModel().meta.title) + ".png";
    
    // Show native save file dialog
    SDL_ShowSaveFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filterIndex) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open save dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a path
            std::string path = filelist[0];
            
            // Ensure .png extension
            if (path.size() < 4 || 
                path.substr(path.size() - 4) != ".png") {
                path += ".png";
            }
            
            // Check for empty content
            ContentBounds bounds = 
                data->app->GetModel().CalculateContentBounds();
            
            if (bounds.isEmpty) {
                data->ui->ShowToast(
                    "Cannot export: No content drawn yet",
                    Toast::Type::Error);
                return;
            }
            
            // Export PNG
            data->app->ExportPng(path);
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Filters
        filters,
        // Number of filters
        2,
        // Default filename (pre-filled in save dialog)
        defaultFilename.c_str()
    );
}

void UI::HandleDroppedFile(const std::string& filePath, App& app,
                           JobQueue& jobs, IconManager& icons) {
    // Check if we're in Welcome Screen
    if (app.GetState() == AppState::Welcome) {
        // Try to import as project
        bool isValidProject = false;
        
        // Check if it's a .cart file
        if (filePath.size() >= 5 && 
            filePath.substr(filePath.size() - 5) == ".cart") {
            // Validate it's a real file and has ZIP magic bytes
            namespace fs = std::filesystem;
            if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
                isValidProject = true;
            }
        }
        // Check if it's a project folder
        else if (ProjectFolder::IsProjectFolder(filePath)) {
            isValidProject = true;
        }
        
        if (isValidProject) {
            // Start async loading
            m_modals.showLoadingModal = true;
            m_modals.loadingFilePath = filePath;
            m_modals.loadingCancelled = false;
            m_modals.loadingStartTime = Platform::GetTime();
            
            // Extract filename for display
            size_t lastSlash = filePath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                m_modals.loadingFileName = filePath.substr(lastSlash + 1);
            } else {
                m_modals.loadingFileName = filePath;
            }
            
            // Create shared model to load into
            auto loadedModel = std::make_shared<Model>();
            auto tempIcons = std::make_shared<IconManager>();
            
            // Capture necessary variables
            std::string capturedPath = filePath;
            
            // Enqueue async loading job
            jobs.Enqueue(
                JobType::LoadProject,
                [capturedPath, loadedModel, tempIcons, this]() {
                    // Check for cancellation before starting
                    if (m_modals.loadingCancelled) {
                        throw std::runtime_error("Cancelled by user");
                    }
                    
                    bool success = false;
                    bool isCartFile = (capturedPath.size() >= 5 && 
                                      capturedPath.substr(capturedPath.size() - 5) == ".cart");
                    if (isCartFile) {
                        success = Package::Load(capturedPath, *loadedModel, 
                                              tempIcons.get());
                    } else {
                        success = ProjectFolder::Load(capturedPath, *loadedModel,
                                                     tempIcons.get());
                    }
                    
                    if (!success) {
                        throw std::runtime_error("Failed to load project");
                    }
                },
                [this, &app, loadedModel, tempIcons, &icons, capturedPath](
                    bool success, const std::string& error) {
                    m_modals.showLoadingModal = false;
                    
                    if (success && !m_modals.loadingCancelled) {
                        // Swap loaded model into app (main thread safe)
                        app.OpenProject(capturedPath);
                        
                        // Merge loaded icons into main icon manager
                        // (Icons were already added to tempIcons in background)
                        icons.BuildAtlas();  // Rebuild atlas on main thread
                        
                        // Transition to editor
                        app.ShowEditor();
                        
                        ShowToast("Project loaded", Toast::Type::Success);
                    } else if (m_modals.loadingCancelled) {
                        ShowToast("Loading cancelled", Toast::Type::Info);
                    } else {
                        ShowToast("Failed to load project: " + error, 
                                 Toast::Type::Error);
                    }
                }
            );
        } else {
            // Invalid format
            ShowToast(
                "Invalid format. Drop a .cart file or project folder.",
                Toast::Type::Warning,
                4.0f
            );
        }
    } else {
        // In Editor state - handle as icon import (existing behavior)
        droppedFilePath = filePath;
        hasDroppedFile = true;
    }
}



} // namespace Cartograph
