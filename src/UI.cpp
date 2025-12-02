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
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>

// OpenGL for texture loading
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

// SDL_CursorDeleter implementation (outside namespace)
void SDL_CursorDeleter::operator()(SDL_Cursor* cursor) const {
    if (cursor) {
        SDL_DestroyCursor(cursor);
    }
}

namespace Cartograph {

UI::UI() : m_modals(*this), m_welcomeScreen(*this) {
    // Connect canvas panel to UI state for shared members
    m_canvasPanel.showPropertiesPanel = &showPropertiesPanel;
    m_canvasPanel.layoutInitialized = &m_layoutInitialized;
    
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
    
    // Update menu state (native on macOS, ImGui on Windows/Linux)
    m_nativeMenu->Update(app, model, canvas, history, icons, jobs);
    
    // For ImGui menus, we need to pass the showPropertiesPanel pointer
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
    RenderToolsPanel(model, history, icons, jobs);
    RenderHierarchyPanel(model, history, canvas);
    
    if (showPropertiesPanel) {
        RenderPropertiesPanel(model, icons, jobs);
    }
    
    // Render toasts
    RenderToasts(deltaTime);
    
    // Render modal dialogs
    m_modals.RenderAll(
        app, model, canvas, history, icons, jobs, keymap,
        m_canvasPanel.selectedIconName,
        m_canvasPanel.selectedMarker,
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
            
            std::string settingsShortcut = Platform::FormatShortcut(",");
            if (ImGui::MenuItem("Settings...", settingsShortcut.c_str())) {
                m_modals.showSettingsModal = true;
            }
            
            ImGui::EndMenu();
        }
        
        // View Menu
        if (ImGui::BeginMenu("View")) {
            std::string propPanelShortcut = Platform::FormatShortcut("P");
            if (ImGui::MenuItem("Properties Panel", propPanelShortcut.c_str(),
                               showPropertiesPanel)) {
                showPropertiesPanel = !showPropertiesPanel;
                m_layoutInitialized = false;  // Trigger layout rebuild
            }
            
            ImGui::Separator();
            
            ImGui::MenuItem("Show Grid", "G", &canvas.showGrid);
            
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
                          JobQueue& jobs) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Tools", nullptr, flags);
    
    ImGui::Text("Tools");
    ImGui::Separator();
    
    const char* toolNames[] = {
        "Move", "Select", "Paint", "Erase", "Fill", 
        "Marker", "Eyedropper"
    };
    
    const char* toolIconNames[] = {
        "move", "square-dashed", "paintbrush", "paint-bucket", "eraser",
        "map-pinned", "pipette"
    };
    
    const char* toolShortcuts[] = {
        "V", "S", "B", "E", "F", "M", "I"
    };
    
    // Icon button size and spacing
    const ImVec2 iconButtonSize(36.0f, 36.0f);
    const float iconSpacing = 6.0f;
    const float panelPadding = 8.0f;
    const int TOOLS_PER_ROW = 4;  // Fixed 4-column grid
    
    // Add padding
    ImGui::Dummy(ImVec2(0, panelPadding * 0.5f));
    
    for (int i = 0; i < 7; ++i) {
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
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(i);
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 6) {
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
        "Room Paint", "Room Erase", "Room Fill"
    };
    
    const char* roomToolIconNames[] = {
        "paintbrush", "eraser", "paint-bucket"
    };
    
    const char* roomToolShortcuts[] = {
        "Shift+R", "Shift+E", "Shift+F"
    };
    
    const ImVec4 maroonBg(0.55f, 0.1f, 0.1f, 1.0f);
    const ImVec4 maroonBgHover(0.65f, 0.2f, 0.2f, 1.0f);
    const ImVec4 maroonBgActive(0.45f, 0.05f, 0.05f, 1.0f);
    
    for (int i = 0; i < 3; ++i) {
        int toolIdx = 7 + i;  // RoomPaint=7, RoomErase=8, RoomFill=9
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
                case 0: // Room Paint
                    ImGui::Text("Room Paint Tool [Shift+R]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click/drag to assign cells to active room");
                    break;
                case 1: // Room Erase
                    ImGui::Text("Room Erase Tool [Shift+E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click/drag to remove cells from rooms");
                    break;
                case 2: // Room Fill
                    ImGui::Text("Room Fill Tool [Shift+F]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to flood-fill area into active room");
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
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(toolIdx);
            
            // Auto-create room if none active
            if (m_canvasPanel.activeRoomId.empty() && !model.rooms.empty()) {
                // Use first room as default
                m_canvasPanel.activeRoomId = model.rooms[0].id;
            } else if (m_canvasPanel.activeRoomId.empty()) {
                // Create a new room
                Room newRoom;
                newRoom.id = model.GenerateRoomId();
                newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
                newRoom.regionId = -1;
                newRoom.color = model.GenerateDistinctRoomColor();
                newRoom.cellsCacheDirty = true;
                newRoom.connectionsDirty = true;
                model.rooms.push_back(newRoom);
                m_canvasPanel.activeRoomId = newRoom.id;
                model.MarkDirty();
            }
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 2) {
            ImGui::SameLine(0, iconSpacing);
        }
        
        ImGui::PopID();
    }
    
    // Show active room for room tools
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::RoomPaint ||
        m_canvasPanel.currentTool == CanvasPanel::Tool::RoomErase ||
        m_canvasPanel.currentTool == CanvasPanel::Tool::RoomFill) {
        ImGui::Spacing();
        ImGui::Text("Active Room:");
        ImGui::Separator();
        
        Room* activeRoom = model.FindRoom(m_canvasPanel.activeRoomId);
        if (activeRoom) {
            // Show color indicator
            ImVec4 roomColor = activeRoom->color.ToImVec4();
            ImGui::ColorButton("##activeRoomColor", roomColor, 
                              ImGuiColorEditFlags_NoTooltip, 
                              ImVec2(24, 24));
            ImGui::SameLine();
            ImGui::Text("%s", activeRoom->name.c_str());
            
            // Button to select different room
            if (ImGui::Button("Change Room")) {
                // This would open a room selector (for now, cycle through rooms)
                if (!model.rooms.empty()) {
                    auto it = std::find_if(model.rooms.begin(), model.rooms.end(),
                        [&](const Room& r) { return r.id == m_canvasPanel.activeRoomId; });
                    if (it != model.rooms.end()) {
                        ++it;
                        if (it == model.rooms.end()) it = model.rooms.begin();
                        m_canvasPanel.activeRoomId = it->id;
                    }
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No active room");
        }
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
        
        if (m_canvasPanel.isHoveringCanvas && m_canvasPanel.hoveredTileX >= 0 && m_canvasPanel.hoveredTileY >= 0) {
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
        
        // Label input
        char labelBuf[128];
        std::strncpy(labelBuf, m_canvasPanel.markerLabel.c_str(), sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
        
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
            m_canvasPanel.markerLabel = labelBuf;
            
            // Update selected marker if editing
            if (m_canvasPanel.selectedMarker) {
                m_canvasPanel.selectedMarker->label = m_canvasPanel.markerLabel;
                m_canvasPanel.selectedMarker->showLabel = !m_canvasPanel.markerLabel.empty();
                model.MarkDirty();
            }
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
        }
        
        // Color picker popup
        if (ImGui::BeginPopup("ColorPicker")) {
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
                m_canvasPanel.markerColorHex[sizeof(m_canvasPanel.markerColorHex) - 1] = '\0';
                
                // Update selected marker if editing
                if (m_canvasPanel.selectedMarker) {
                    m_canvasPanel.selectedMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
            ImGui::EndPopup();
        }
        
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
                if (m_canvasPanel.selectedMarker) {
                    m_canvasPanel.selectedMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
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
                
                // Responsive grid layout
                float buttonSize = 80.0f;  // Size of each icon button
                float spacing = 8.0f;
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                // Calculate responsive columns (min 2, max 4)
                int columns = std::max(2, 
                    static_cast<int>((availWidth + spacing) / 
                                    (buttonSize + spacing)));
                columns = std::min(columns, 4);
                
                // Calculate centering offset
                float totalWidth = columns * buttonSize + 
                                  (columns - 1) * spacing;
                float leftPadding = std::max(0.0f, 
                    (availWidth - totalWidth) * 0.5f);
                
                for (size_t i = 0; i < iconNames.size(); ++i) {
                    const std::string& iconName = iconNames[i];
                    const Icon* icon = icons.GetIcon(iconName);
                    
                    if (!icon) continue;
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    // Begin new row with left padding for centering
                    if (i % columns == 0) {
                        ImGui::SetCursorPosX(
                            ImGui::GetCursorPosX() + leftPadding);
                    } else {
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
                            m_canvasPanel.selectedIconName = iconName;
                            
                            // Update selected marker if editing
                            if (m_canvasPanel.selectedMarker) {
                                m_canvasPanel.selectedMarker->icon = m_canvasPanel.selectedIconName;
                                model.MarkDirty();
                            }
                        }
                    } else {
                        // Fallback if no texture
                        if (ImGui::Button("##icon", 
                                         ImVec2(buttonSize, buttonSize))) {
                            m_canvasPanel.selectedIconName = iconName;
                            
                            if (m_canvasPanel.selectedMarker) {
                                m_canvasPanel.selectedMarker->icon = m_canvasPanel.selectedIconName;
                                model.MarkDirty();
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
        
        // Actions
        ImGui::Separator();
        
        if (m_canvasPanel.selectedMarker) {
            ImGui::Text("Editing marker:");
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               m_canvasPanel.selectedMarker->x, m_canvasPanel.selectedMarker->y);
            
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                m_canvasPanel.selectedMarker = nullptr;
            }
        }
    }
    
    // TODO: Add layers toggles here
    
    ImGui::End();
}

void UI::RenderHierarchyPanel(Model& model, History& history, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Hierarchy", nullptr, flags);
    
    ImGui::Text("Hierarchy");
    ImGui::Separator();
    
    // Toolbar buttons
    if (ImGui::Button("+ Room")) {
        // Create new room
        Room newRoom;
        newRoom.id = model.GenerateRoomId();
        newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
        newRoom.regionId = -1;
        newRoom.color = model.GenerateDistinctRoomColor();
        newRoom.cellsCacheDirty = true;
        newRoom.connectionsDirty = true;
        model.rooms.push_back(newRoom);
        m_canvasPanel.activeRoomId = newRoom.id;
        model.MarkDirty();
        AddConsoleMessage("Created " + newRoom.name, MessageType::Success);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("+ Region")) {
        // Create new region group
        RegionGroup newRegion;
        newRegion.id = model.GenerateRegionGroupId();
        newRegion.name = "Region " + std::to_string(model.regionGroups.size() + 1);
        model.regionGroups.push_back(newRegion);
        model.MarkDirty();
        AddConsoleMessage("Created " + newRegion.name, MessageType::Success);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Detect Rooms")) {
        // Detect all enclosed rooms
        auto detectedRooms = model.DetectAllEnclosedRooms();
        
        if (detectedRooms.empty()) {
            AddConsoleMessage("No enclosed rooms found", MessageType::Warning);
        } else {
            // Show confirmation dialog would go here
            // For now, just create them
            int created = 0;
            for (const auto& detected : detectedRooms) {
                if (detected.isEnclosed && !detected.cells.empty()) {
                    Room room = model.CreateRoomFromCells(detected.cells);
                    created++;
                }
            }
            
            if (created > 0) {
                AddConsoleMessage("Detected and created " + std::to_string(created) + 
                                " room(s)", MessageType::Success);
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Search/filter box
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search...", searchBuffer, 
                            sizeof(searchBuffer));
    std::string searchTerm(searchBuffer);
    
    ImGui::Spacing();
    
    // Region Groups tree
    if (ImGui::CollapsingHeader("Region Groups", 
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (model.regionGroups.empty()) {
            ImGui::Indent();
            ImGui::TextDisabled("No region groups");
            ImGui::Unindent();
        } else {
            for (auto& region : model.regionGroups) {
                // Filter by search
                if (!searchTerm.empty() && 
                    region.name.find(searchTerm) == std::string::npos) {
                    continue;
                }
                
                ImGui::PushID(region.id.c_str());
                
                // Region group node (collapsible)
                ImGuiTreeNodeFlags nodeFlags = 
                    ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_OpenOnDoubleClick |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                
                bool regionOpen = ImGui::TreeNodeEx(
                    ("🗺️ " + region.name).c_str(), nodeFlags
                );
                
                // Context menu for region
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        // TODO: Open rename dialog
                    }
                    if (ImGui::MenuItem("Delete")) {
                        // TODO: Delete with confirmation
                    }
                    ImGui::EndPopup();
                }
                
                if (regionOpen) {
                    ImGui::Indent();
                    
                    // Show rooms in this region
                    int roomCount = 0;
                    for (const auto& room : model.rooms) {
                        if (room.parentRegionGroupId == region.id) {
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
                            
                            ImGui::TreeNodeEx(("🏠 " + room.name).c_str(), roomFlags);
                            
                            if (ImGui::IsItemClicked()) {
                                m_canvasPanel.selectedRoomId = room.id;
                                m_canvasPanel.activeRoomId = room.id;
                            }
                            
                            // Context menu for room
                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("Rename")) {
                                    // TODO: Open rename dialog
                                }
                                if (ImGui::MenuItem("Change Color")) {
                                    // TODO: Open color picker
                                }
                                if (ImGui::MenuItem("Remove from Region")) {
                                    // TODO: Unassign from region
                                }
                                ImGui::Separator();
                                if (ImGui::MenuItem("Delete")) {
                                    // TODO: Delete with confirmation
                                }
                                ImGui::EndPopup();
                            }
                            
                            ImGui::PopID();
                        }
                    }
                    
                    if (roomCount == 0) {
                        ImGui::TextDisabled("No rooms in this region");
                    }
                    
                    ImGui::Unindent();
                    ImGui::TreePop();
                }
                
                ImGui::PopID();
            }
        }
    }
    
    ImGui::Spacing();
    
    // Unassigned Rooms tree
    if (ImGui::CollapsingHeader("Unassigned Rooms", 
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        bool hasUnassigned = false;
        
        for (auto& room : model.rooms) {
            if (!room.parentRegionGroupId.empty()) {
                continue;  // Skip rooms in regions
            }
            
            // Filter by search
            if (!searchTerm.empty() && 
                room.name.find(searchTerm) == std::string::npos) {
                continue;
            }
            
            hasUnassigned = true;
            
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
            
            ImGui::TreeNodeEx(("🏠 " + room.name).c_str(), roomFlags);
            
            if (ImGui::IsItemClicked()) {
                m_canvasPanel.selectedRoomId = room.id;
                m_canvasPanel.activeRoomId = room.id;
            }
            
            // Context menu for room
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rename")) {
                    // TODO: Open rename dialog
                }
                if (ImGui::MenuItem("Change Color")) {
                    // TODO: Open color picker
                }
                if (ImGui::MenuItem("Move to Region...")) {
                    // TODO: Show region selector
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete")) {
                    // TODO: Delete with confirmation
                }
                ImGui::EndPopup();
            }
            
            ImGui::PopID();
        }
        
        if (!hasUnassigned) {
            ImGui::Indent();
            ImGui::TextDisabled("No unassigned rooms");
            ImGui::Unindent();
        }
    }
    
    // Toggle room overlays
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Checkbox("Show Room Overlays", &m_canvasPanel.showRoomOverlays);
    
    ImGui::End();
}

void UI::RenderPropertiesPanel(Model& model, IconManager& icons, JobQueue& jobs) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Properties", nullptr, flags);
    
    ImGui::Text("Properties");
    ImGui::Separator();
    
    ImGui::Spacing();
    
    // Selected room details
    if (!m_canvasPanel.selectedRoomId.empty()) {
        Room* room = model.FindRoom(m_canvasPanel.selectedRoomId);
        if (room) {
            if (ImGui::CollapsingHeader("Selected Room", 
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                // Room name
                char nameBuf[256];
                strncpy(nameBuf, room->name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                    room->name = nameBuf;
                    model.MarkDirty();
                }
                
                // Room color
                float colorArray[4] = {
                    room->color.r, room->color.g, 
                    room->color.b, room->color.a
                };
                if (ImGui::ColorEdit4("Color", colorArray)) {
                    room->color = Color(colorArray[0], colorArray[1], 
                                       colorArray[2], colorArray[3]);
                    model.MarkDirty();
                }
                
                // Room notes (renamed to Description for consistency)
                char notesBuf[1024];
                strncpy(notesBuf, room->notes.c_str(), sizeof(notesBuf) - 1);
                if (ImGui::InputTextMultiline("Description", notesBuf, 
                                             sizeof(notesBuf), 
                                             ImVec2(-1, 80))) {
                    room->notes = notesBuf;
                    model.MarkDirty();
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
                    room->tags.erase(
                        std::remove(room->tags.begin(), room->tags.end(), tagToRemove),
                        room->tags.end()
                    );
                    model.MarkDirty();
                }
                
                ImGui::NewLine();
                
                // Add new tag
                static char newTagBuf[64] = "";
                ImGui::SetNextItemWidth(-80);
                bool enterPressed = ImGui::InputTextWithHint(
                    "##newtag", "Add tag...", newTagBuf, sizeof(newTagBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );
                
                ImGui::SameLine();
                bool addClicked = ImGui::Button("Add Tag");
                
                if ((enterPressed || addClicked) && strlen(newTagBuf) > 0) {
                    std::string newTag(newTagBuf);
                    // Check if tag already exists
                    if (std::find(room->tags.begin(), room->tags.end(), newTag) == 
                        room->tags.end()) {
                        room->tags.push_back(newTag);
                        model.MarkDirty();
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
                            // Show color indicator
                            ImVec4 connColor = connectedRoom->color.ToImVec4();
                            ImGui::ColorButton("##connColor", connColor,
                                             ImGuiColorEditFlags_NoTooltip,
                                             ImVec2(12, 12));
                            ImGui::SameLine();
                            
                            // Room name
                            ImGui::Text("%s", connectedRoom->name.c_str());
                            ImGui::SameLine();
                            
                            // Jump button
                            ImGui::PushID(connectedId.c_str());
                            if (ImGui::SmallButton("Jump")) {
                                m_canvasPanel.selectedRoomId = connectedId;
                                m_canvasPanel.activeRoomId = connectedId;
                                // TODO: Center camera on room
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::Unindent();
                }
                
                // Delete room button
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                
                if (ImGui::Button("Delete Room", ImVec2(-1, 0))) {
                    // Confirm deletion
                    m_modals.editingRoomId = room->id;
                    m_modals.showDeleteRoomDialog = true;
                }
                
                ImGui::PopStyleColor(3);
                
                ImGui::Separator();
                
                // Room paint mode toggle
                if (m_canvasPanel.roomPaintMode) {
                    if (ImGui::Button("Exit Room Paint Mode")) {
                        m_canvasPanel.roomPaintMode = false;
                    }
                    ImGui::TextWrapped(
                        "Paint cells to assign them to this room. "
                        "Right-click to remove cells."
                    );
                } else {
                    if (ImGui::Button("Paint Room Cells")) {
                        m_canvasPanel.roomPaintMode = true;
                    }
                }
                
                ImGui::Separator();
                
                // Room actions
                if (ImGui::Button("Demote Room")) {
                    ImGui::OpenPopup("Demote Room?");
                }
                
                // Demote confirmation popup
                if (ImGui::BeginPopupModal("Demote Room?", nullptr, 
                                          ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("What would you like to do?");
                    ImGui::Separator();
                    
                    if (ImGui::Button("Remove room metadata only", 
                                     ImVec2(-1, 0))) {
                        // Remove room but keep cells/tiles
                        model.ClearAllCellsForRoom(room->id);
                        auto it = std::find_if(
                            model.rooms.begin(), 
                            model.rooms.end(),
                            [&](const Room& r) { return r.id == m_canvasPanel.selectedRoomId; }
                        );
                        if (it != model.rooms.end()) {
                            model.rooms.erase(it);
                        }
                        m_canvasPanel.selectedRoomId.clear();
                        m_canvasPanel.roomPaintMode = false;
                        model.MarkDirty();
                        ImGui::CloseCurrentPopup();
                    }
                    
                    if (ImGui::Button("Remove room AND clear all cells", 
                                     ImVec2(-1, 0))) {
                        // Clear all cells assigned to this room
                        for (auto it = model.cellRoomAssignments.begin(); 
                             it != model.cellRoomAssignments.end(); ) {
                            if (it->second == room->id) {
                                // Also clear the tile at this position
                                model.SetTileAt("", it->first.first, 
                                              it->first.second, 0);
                                it = model.cellRoomAssignments.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        // Remove room
                        auto it = std::find_if(
                            model.rooms.begin(), 
                            model.rooms.end(),
                            [&](const Room& r) { return r.id == m_canvasPanel.selectedRoomId; }
                        );
                        if (it != model.rooms.end()) {
                            model.rooms.erase(it);
                        }
                        m_canvasPanel.selectedRoomId.clear();
                        m_canvasPanel.roomPaintMode = false;
                        model.MarkDirty();
                        ImGui::CloseCurrentPopup();
                    }
                    
                    if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    
                    ImGui::EndPopup();
                }
            }
        }
    }
    
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
            // Generate unique room ID
            std::string roomId = "room_" + std::to_string(model.rooms.size());
            
            Room newRoom;
            newRoom.id = roomId;
            newRoom.name = m_modals.newRoomName;
            newRoom.color = Color(m_modals.newRoomColor[0], m_modals.newRoomColor[1], 
                                 m_modals.newRoomColor[2], 1.0f);
            newRoom.regionId = -1;
            
            model.rooms.push_back(newRoom);
            m_canvasPanel.selectedRoomId = roomId;
            model.MarkDirty();
            
            // Reset form
            strcpy(m_modals.newRoomName, "New Room");
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
        if (m_canvasPanel.isHoveringCanvas && m_canvasPanel.hoveredTileX >= 0 && m_canvasPanel.hoveredTileY >= 0) {
            ImGui::Text("Tile: %d, %d", m_canvasPanel.hoveredTileX, m_canvasPanel.hoveredTileY);
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
        // 3-column mode: Split right for properties panel (320px)
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
        ImGui::DockBuilderDockWindow("Cartograph/Inspector", rightId);
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
        // Default location
        nullptr
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
        // Default location
        nullptr
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
