#include "UI.h"
#include "App.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "Jobs.h"
#include "render/Renderer.h"
#include "IOJson.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include "platform/Time.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
#include <nlohmann/json.hpp>
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

// stb_image for PNG loading (already included in Icons.cpp)
#include <stb/stb_image.h>

namespace Cartograph {

// ============================================================================
// Helper functions
// ============================================================================

/**
 * Get all tiles along a line from (x0, y0) to (x1, y1).
 * Uses Bresenham's line algorithm to ensure continuous painting.
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @return Vector of (x, y) tile coordinates along the line
 */
static std::vector<std::pair<int, int>> GetTilesAlongLine(
    int x0, int y0, 
    int x1, int y1
) {
    std::vector<std::pair<int, int>> tiles;
    
    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0;
    int y = y0;
    
    while (true) {
        tiles.push_back({x, y});
        
        if (x == x1 && y == y1) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return tiles;
}

UI::UI() {
}

void UI::SetupDockspace() {
    // Docking setup happens in first frame of Render()
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
    // Render menu bar outside dockspace (ensures it's on top)
    RenderMenuBar(app, model, canvas, history, icons, jobs);
    
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
            showExportModal = true;
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
    RenderCanvasPanel(renderer, model, canvas, history, icons, keymap);
    RenderStatusBar(model, canvas);
    RenderToolsPanel(model, icons, jobs);
    
    if (showPropertiesPanel) {
        RenderPropertiesPanel(model, icons, jobs);
    }
    
    // Render toasts
    RenderToasts(deltaTime);
    
    // Render modal dialogs
    if (showExportModal) {
        RenderExportModal(model, canvas);
    }
    if (shouldShowExportPngDialog) {
        shouldShowExportPngDialog = false;
        ShowExportPngDialog(app);
    }
    if (showSettingsModal) {
        RenderSettingsModal(model);
    }
    if (showRenameIconModal) {
        RenderRenameIconModal(model, icons);
    }
    
    // Quit confirmation modal (when closing with unsaved changes)
    if (showQuitConfirmationModal) {
        RenderQuitConfirmationModal(app, model);
    }
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

void UI::RenderWelcomeScreen(App& app, Model& model) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;
    
    ImGui::Begin("CartographWelcome", nullptr, windowFlags);
    
    // Center content horizontally and vertically
    ImVec2 windowSize = ImGui::GetWindowSize();
    float contentHeight = 480.0f;  // Height for treasure map parchment
    
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Monospace
    
    // Treasure map scroll with parchment texture
    const char* mapLine1 = 
        "####++++++++++++++++++++++++++++++++++++++++++++++++++++####";
    const char* mapLine2 = 
        "###++++++++++--------+--------++++++---------++++++++++++###";
    const char* mapLine3 = 
        "##+++++-------+----+----+----------+----+-------+--++++++##";
    const char* mapLine4 = 
        "##++++---+-------+----------+------+----------+------++++##";
    const char* mapLine5 = 
        "##++++------+----------++----------+------+----------++++##";
    const char* mapLine6 = 
        "##++++---+--------++----+------+------+--+------------+++##";
    const char* mapLine7 = 
        "##+++-------+----------+------+--------+------+-------+++##";
    
    // Cartograph ASCII integrated into map
    const char* titleLine1 = 
        "##+++   ___          _                             _  +++##";
    const char* titleLine2 = 
        "##++   / __\\__ _ _ _| |_ ___   __ _ _ __ __ _ _ __| |__++##";
    const char* titleLine3 = 
        "##++  / /  / _` | '__| __/ _ \\ / _` | '__/ _` | '_ \\ '_ ++##";
    const char* titleLine4 = 
        "##++ / /__| (_| | |  | || (_) | (_| | | | (_| | |_) | |+++##";
    const char* titleLine5 = 
        "##++ \\____/\\__,_|_|   \\__\\___/ \\__, |_|  \\__,_| .__/|_|+++##";
    const char* titleLine6 = 
        "##+++                          |___/          |_|      +++##";
    
    // Bottom map texture
    const char* mapLine8 = 
        "##++++------+----------+------+--------+------+-------+++##";
    const char* mapLine9 = 
        "##++++---+--------++----+------+------+--+------------+++##";
    const char* mapLine10 = 
        "##+++++------+----------++----------+------+----------+++##";
    const char* mapLine11 = 
        "##++++++---+-------+----------+------+----------+---+++++##";
    const char* mapLine12 = 
        "###+++++++++-------+----+----+----------+----+--+++++++++##";
    const char* mapLine13 = 
        "####++++++++++++++--------+--------++++++---------+++++####";
    
    // Calculate centering based on longest line
    float scrollWidth = ImGui::CalcTextSize(mapLine1).x;
    float scrollStartX = (windowSize.x - scrollWidth) * 0.5f;
    float scrollStartY = (windowSize.y - contentHeight) * 0.2f;
    
    ImGui::SetCursorPos(ImVec2(scrollStartX, scrollStartY));
    ImGui::BeginGroup();
    
    // Top map texture (brown/parchment color)
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine1);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine2);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine3);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine4);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine5);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine6);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine7);
    
    // Cartograph title (blue text on parchment)
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine1);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine2);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine3);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine4);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine5);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine6);
    
    // Bottom map texture
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine8);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine9);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine10);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine11);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine12);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine13);
    
    ImGui::PopFont();
    
    ImGui::Spacing();
    
    // Subtitle - centered
    const char* subtitle = 
        "M e t r o i d v a n i a   M a p   E d i t o r";
    float subtitleWidth = ImGui::CalcTextSize(subtitle).x;
    float subtitleStartX = (windowSize.x - subtitleWidth) * 0.5f;
    
    ImGui::SetCursorPosX(subtitleStartX);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", subtitle);
    
    // Add spacing between ASCII art and buttons
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Main action buttons - side by side, centered
    float buttonWidth = 200.0f;
    float buttonHeight = 50.0f;
    float buttonSpacing = 16.0f;
    float buttonsStartX = (windowSize.x - (buttonWidth * 2 + 
                                           buttonSpacing)) * 0.5f;
    
    ImGui::SetCursorPosX(buttonsStartX);
    if (ImGui::Button("Create New Project", ImVec2(buttonWidth, 
                                                    buttonHeight))) {
        showNewProjectModal = true;
        // Reset config to defaults
        std::strcpy(newProjectConfig.projectName, "New Map");
        newProjectConfig.gridPreset = GridPreset::Square;
        newProjectConfig.mapWidth = 100;
        newProjectConfig.mapHeight = 100;
        
        // Initialize default save path
        newProjectConfig.saveDirectory = Platform::GetDefaultProjectsDir();
        UpdateNewProjectPath();
        
        // Ensure default directory exists
        Platform::EnsureDirectoryExists(newProjectConfig.saveDirectory);
    }
    
    ImGui::SameLine(0.0f, buttonSpacing);
    if (ImGui::Button("Import Project", ImVec2(buttonWidth, 
                                                buttonHeight))) {
        // TODO: Implement file picker dialog for .cart files
        ShowToast("Import feature coming soon!", Toast::Type::Info);
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Recent Projects Section
    if (!recentProjects.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        
        // "Recent Projects" header - centered
        const char* headerText = "Recent Projects";
        float headerWidth = ImGui::CalcTextSize(headerText).x;
        float headerStartX = (windowSize.x - headerWidth) * 0.5f;
        
        ImGui::SetCursorPosX(headerStartX);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", headerText);
        ImGui::Spacing();
        ImGui::Spacing();
        
        // 3-card horizontal display
        RenderRecentProjectsList(app);
        
        // "View more" button if there are more than 3 projects
        if (recentProjects.size() > 3) {
            ImGui::Spacing();
            ImGui::Spacing();
            
            float viewMoreWidth = 120.0f;
            float viewMoreStartX = (windowSize.x - viewMoreWidth) * 0.5f;
            ImGui::SetCursorPosX(viewMoreStartX);
            
            if (ImGui::Button("View more...", ImVec2(viewMoreWidth, 0))) {
                showProjectBrowserModal = true;
            }
        }
    }
    
    ImGui::EndGroup();
    
    // What's New button in corner
    ImGui::SetCursorPos(ImVec2(windowSize.x - 140, windowSize.y - 40));
    if (ImGui::Button("What's New?", ImVec2(120, 30))) {
        showWhatsNew = !showWhatsNew;
    }
    
    ImGui::End();
    
    // Render modal dialogs
    if (showNewProjectModal) {
        RenderNewProjectModal(app, model);
    }
    
    if (showProjectBrowserModal) {
        RenderProjectBrowserModal(app);
    }
    
    if (showWhatsNew) {
        RenderWhatsNewPanel();
    }
    
    if (showAutosaveRecoveryModal) {
        RenderAutosaveRecoveryModal(app, model);
    }
    
    // Render toasts
    RenderToasts(0.016f);
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
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Cmd+N")) {
                // TODO: New project
            }
            if (ImGui::MenuItem("Open...", "Cmd+O")) {
                // TODO: Open project
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Project", "Cmd+S")) {
                app.SaveProject();
            }
            if (ImGui::MenuItem("Save Project As...", "Cmd+Shift+S")) {
                // Save as project folder (git-friendly)
                ShowSaveProjectDialog(app);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Package (.cart)...", 
                               "Cmd+Shift+E")) {
                // Export as .cart package (single file distribution)
                ShowExportPackageDialog(app);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export PNG...", "Cmd+E")) {
                showExportModal = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Cmd+Q")) {
                app.RequestQuit();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = history.CanUndo();
            bool canRedo = history.CanRedo();
            
            if (ImGui::MenuItem("Undo", "Cmd+Z", false, canUndo)) {
                history.Undo(model);
            }
            if (ImGui::MenuItem("Redo", "Cmd+Y", false, canRedo)) {
                history.Redo(model);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Settings...")) {
                showSettingsModal = true;
            }
            ImGui::EndMenu();
        }
        
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
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Show Properties Panel", "Cmd+P", 
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
        
        bool selected = (selectedTileId == tile.id);
        ImVec4 color = tile.color.ToImVec4();
        
        // Color button
        if (ImGui::ColorButton("##color", color, 0, ImVec2(24, 24))) {
            selectedTileId = tile.id;
        }
        
        ImGui::SameLine();
        
        // Selectable name
        if (ImGui::Selectable(tile.name.c_str(), selected)) {
            selectedTileId = tile.id;
        }
        
        ImGui::PopID();
    }
    
    ImGui::End();
}

void UI::RenderToolsPanel(Model& model, IconManager& icons, JobQueue& jobs) {
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
        bool selected = (static_cast<int>(currentTool) == i);
        
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
            currentTool = static_cast<Tool>(i);
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
    
    // Show palette when Paint tool is active
    if (currentTool == Tool::Paint || currentTool == Tool::Erase || 
        currentTool == Tool::Fill) {
        ImGui::Text("Paint Color");
        ImGui::Separator();
        
        // Display palette tiles inline
        for (const auto& tile : model.palette) {
            if (tile.id == 0) continue;  // Skip empty tile
            
            ImGui::PushID(tile.id);
            
            bool selected = (selectedTileId == tile.id);
            ImVec4 color = tile.color.ToImVec4();
            
            // Color button
            if (ImGui::ColorButton("##color", color, 0, ImVec2(24, 24))) {
                selectedTileId = tile.id;
            }
            
            ImGui::SameLine();
            
            // Selectable name
            if (ImGui::Selectable(tile.name.c_str(), selected, 0, 
                                  ImVec2(0, 24))) {
                selectedTileId = tile.id;
            }
            
            ImGui::PopID();
        }
    }
    
    // Show marker options when Marker tool is active
    if (currentTool == Tool::Marker) {
        ImGui::Text("Marker Settings");
        ImGui::Separator();
        
        // Label input
        char labelBuf[128];
        std::strncpy(labelBuf, markerLabel.c_str(), sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
        
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
            markerLabel = labelBuf;
            
            // Update selected marker if editing
            if (selectedMarker) {
                selectedMarker->label = markerLabel;
                selectedMarker->showLabel = !markerLabel.empty();
                model.MarkDirty();
            }
        }
        
        // Color picker
        float colorArray[4] = {
            markerColor.r, markerColor.g, markerColor.b, markerColor.a
        };
        
        if (ImGui::ColorEdit4("Color", colorArray)) {
            markerColor = Color(
                colorArray[0], colorArray[1], 
                colorArray[2], colorArray[3]
            );
            
            // Update selected marker if editing
            if (selectedMarker) {
                selectedMarker->color = markerColor;
                model.MarkDirty();
            }
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
                            selectedIconName = capturedIconName;
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
                    bool isSelected = (selectedIconName == iconName);
                    
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
                            selectedIconName = iconName;
                            
                            // Update selected marker if editing
                            if (selectedMarker) {
                                selectedMarker->icon = selectedIconName;
                                model.MarkDirty();
                            }
                        }
                    } else {
                        // Fallback if no texture
                        if (ImGui::Button("##icon", 
                                         ImVec2(buttonSize, buttonSize))) {
                            selectedIconName = iconName;
                            
                            if (selectedMarker) {
                                selectedMarker->icon = selectedIconName;
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
                            showRenameIconModal = true;
                            renameIconOldName = iconName;
                            std::strncpy(renameIconNewName, 
                                        iconName.c_str(), 
                                        sizeof(renameIconNewName) - 1);
                            renameIconNewName[sizeof(renameIconNewName) - 1] 
                                = '\0';
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
        
        if (selectedMarker) {
            ImGui::Text("Editing marker:");
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               selectedMarker->x, selectedMarker->y);
            
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                selectedMarker = nullptr;
            }
        }
    }
    
    // TODO: Add layers toggles here
    
    ImGui::End();
}

void UI::RenderPropertiesPanel(Model& model, IconManager& icons, JobQueue& jobs) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Inspector", nullptr, flags);
    
    ImGui::Text("Inspector");
    ImGui::Separator();
    
    // Rooms management
    if (ImGui::CollapsingHeader("Rooms", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Room overlay toggle
        if (ImGui::Checkbox("Show Overlays", &showRoomOverlays)) {
            // Visual change only, no model change
        }
        
        // New room button
        if (ImGui::Button("Create Room")) {
            showNewRoomDialog = true;
        }
        
        ImGui::Separator();
        
        // List existing rooms
        if (model.rooms.empty()) {
            ImGui::TextDisabled("No rooms created yet");
        } else {
            for (auto& room : model.rooms) {
                bool isSelected = (selectedRoomId == room.id);
                ImGuiTreeNodeFlags nodeFlags = 
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    (isSelected ? ImGuiTreeNodeFlags_Selected : 0);
                
                // Color indicator
                ImVec4 roomColor(room.color.r, room.color.g, 
                                room.color.b, room.color.a);
                ImGui::ColorButton("##colorIndicator", roomColor, 
                                  ImGuiColorEditFlags_NoTooltip, 
                                  ImVec2(16, 16));
                ImGui::SameLine();
                
                // Room name (selectable)
                ImGui::TreeNodeEx(room.name.c_str(), nodeFlags);
                if (ImGui::IsItemClicked()) {
                    selectedRoomId = room.id;
                    roomPaintMode = false;  // Reset paint mode on select
                }
            }
        }
    }
    
    ImGui::Spacing();
    
    // Selected room details
    if (!selectedRoomId.empty()) {
        Room* room = model.FindRoom(selectedRoomId);
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
                
                // Room notes
                char notesBuf[1024];
                strncpy(notesBuf, room->notes.c_str(), sizeof(notesBuf) - 1);
                if (ImGui::InputTextMultiline("Notes", notesBuf, 
                                             sizeof(notesBuf), 
                                             ImVec2(-1, 80))) {
                    room->notes = notesBuf;
                    model.MarkDirty();
                }
                
                ImGui::Separator();
                
                // Room paint mode toggle
                if (roomPaintMode) {
                    if (ImGui::Button("Exit Room Paint Mode")) {
                        roomPaintMode = false;
                    }
                    ImGui::TextWrapped(
                        "Paint cells to assign them to this room. "
                        "Right-click to remove cells."
                    );
                } else {
                    if (ImGui::Button("Paint Room Cells")) {
                        roomPaintMode = true;
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
                            [&](const Room& r) { return r.id == selectedRoomId; }
                        );
                        if (it != model.rooms.end()) {
                            model.rooms.erase(it);
                        }
                        selectedRoomId.clear();
                        roomPaintMode = false;
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
                            [&](const Room& r) { return r.id == selectedRoomId; }
                        );
                        if (it != model.rooms.end()) {
                            model.rooms.erase(it);
                        }
                        selectedRoomId.clear();
                        roomPaintMode = false;
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
    if (showNewRoomDialog) {
        ImGui::OpenPopup("Create New Room");
        showNewRoomDialog = false;
    }
    
    if (ImGui::BeginPopupModal("Create New Room", nullptr, 
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Room Name", newRoomName, sizeof(newRoomName));
        ImGui::ColorEdit3("Room Color", newRoomColor);
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Generate unique room ID
            std::string roomId = "room_" + std::to_string(model.rooms.size());
            
            Room newRoom;
            newRoom.id = roomId;
            newRoom.name = newRoomName;
            newRoom.color = Color(newRoomColor[0], newRoomColor[1], 
                                 newRoomColor[2], 1.0f);
            newRoom.regionId = -1;
            
            model.rooms.push_back(newRoom);
            selectedRoomId = roomId;
            model.MarkDirty();
            
            // Reset form
            strcpy(newRoomName, "New Room");
            newRoomColor[0] = 1.0f;
            newRoomColor[1] = 0.5f;
            newRoomColor[2] = 0.5f;
            
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

bool UI::DetectEdgeHover(
    float mouseX, float mouseY,
    const Canvas& canvas,
    const GridConfig& grid,
    EdgeId* outEdgeId,
    EdgeSide* outEdgeSide
) {
    // Convert mouse to world coordinates
    float worldX, worldY;
    canvas.ScreenToWorld(mouseX, mouseY, &worldX, &worldY);
    
    // Calculate which tile we're in (using floor for proper tile indexing)
    int tx = static_cast<int>(std::floor(worldX / grid.tileWidth));
    int ty = static_cast<int>(std::floor(worldY / grid.tileHeight));
    
    // Calculate position within the tile (0.0 to 1.0)
    float tileWorldX = tx * grid.tileWidth;
    float tileWorldY = ty * grid.tileHeight;
    float relX = (worldX - tileWorldX) / grid.tileWidth;
    float relY = (worldY - tileWorldY) / grid.tileHeight;
    
    // Clamp to [0, 1] (should already be, but just in case)
    relX = std::max(0.0f, std::min(1.0f, relX));
    relY = std::max(0.0f, std::min(1.0f, relY));
    
    // Threshold for edge detection (configurable)
    float threshold = grid.edgeHoverThreshold;
    
    // Find the closest edge
    float distToNorth = relY;
    float distToSouth = 1.0f - relY;
    float distToWest = relX;
    float distToEast = 1.0f - relX;
    
    float minDist = std::min({distToNorth, distToSouth, distToWest, distToEast});
    
    // Only trigger if within threshold
    if (minDist > threshold) {
        return false;
    }
    
    // Return the closest edge
    if (minDist == distToNorth) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::North);
        *outEdgeSide = EdgeSide::North;
        return true;
    } else if (minDist == distToSouth) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::South);
        *outEdgeSide = EdgeSide::South;
        return true;
    } else if (minDist == distToWest) {
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::West);
        *outEdgeSide = EdgeSide::West;
        return true;
    } else {  // distToEast
        *outEdgeId = MakeEdgeId(tx, ty, EdgeSide::East);
        *outEdgeSide = EdgeSide::East;
        return true;
    }
}

void UI::RenderCanvasPanel(
    IRenderer& renderer,
    Model& model, 
    Canvas& canvas, 
    History& history,
    IconManager& icons,
    KeymapManager& keymap
) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Cartograph/Canvas", nullptr, flags);
    ImGui::PopStyleVar();
    
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    
    // Reserve space for canvas
    ImGui::InvisibleButton("canvas", canvasSize);
    
    // Accept drag-drop of marker icons
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARKER_ICON")) {
            // Get the icon name from payload
            const char* droppedIconName = (const char*)payload->Data;
            
            // Get mouse position
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert to world coordinates
            float wx, wy;
            canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
            
            // Convert to fractional tile coordinates
            float tileX = wx / model.grid.tileWidth;
            float tileY = wy / model.grid.tileHeight;
            
            // Snap to nearest snap point based on grid preset
            auto snapPoints = model.GetMarkerSnapPoints();
            int baseTileX = static_cast<int>(std::floor(tileX));
            int baseTileY = static_cast<int>(std::floor(tileY));
            float fractionalX = tileX - baseTileX;
            float fractionalY = tileY - baseTileY;
            
            float minDist = FLT_MAX;
            float bestSnapX = 0.5f, bestSnapY = 0.5f;
            
            for (const auto& snap : snapPoints) {
                float dx = fractionalX - snap.first;
                float dy = fractionalY - snap.second;
                float dist = dx*dx + dy*dy;
                if (dist < minDist) {
                    minDist = dist;
                    bestSnapX = snap.first;
                    bestSnapY = snap.second;
                }
            }
            
            tileX = baseTileX + bestSnapX;
            tileY = baseTileY + bestSnapY;
            
            // Place marker at drop location
            Marker newMarker;
            newMarker.id = model.GenerateMarkerId();
            newMarker.roomId = "";
            newMarker.x = tileX;
            newMarker.y = tileY;
            newMarker.kind = "custom";
            newMarker.label = markerLabel;
            newMarker.icon = droppedIconName;
            newMarker.color = markerColor;
            newMarker.size = 0.6f;
            newMarker.showLabel = !markerLabel.empty();
            
            auto cmd = std::make_unique<PlaceMarkerCommand>(newMarker, true);
            history.AddCommand(std::move(cmd), model);
        }
        ImGui::EndDragDropTarget();
    }
    
    // Global keyboard shortcuts for tool switching (work even when not hovering)
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        // Toggle properties panel
        if (keymap.IsActionTriggered("togglePropertiesPanel")) {
            showPropertiesPanel = !showPropertiesPanel;
            m_layoutInitialized = false;  // Trigger layout rebuild
        }
        
        // Tool switching shortcuts
        if (keymap.IsActionTriggered("toolMove")) {
            currentTool = Tool::Move;
        }
        if (keymap.IsActionTriggered("toolSelect")) {
            currentTool = Tool::Select;
        }
        if (keymap.IsActionTriggered("toolPaint")) {
            currentTool = Tool::Paint;
        }
        if (keymap.IsActionTriggered("toolErase")) {
            currentTool = Tool::Erase;
        }
        if (keymap.IsActionTriggered("toolFill")) {
            currentTool = Tool::Fill;
        }
        if (keymap.IsActionTriggered("toolEyedropper")) {
            currentTool = Tool::Eyedropper;
        }
    }
    
    // Handle input
    if (ImGui::IsItemHovered()) {
        // Mouse wheel zoom (available in all tools)
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float zoomFactor = (wheel > 0) ? 1.1f : 0.9f;
            canvas.SetZoom(canvas.zoom * zoomFactor);
        }
        
        // Middle mouse button panning (universal shortcut)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            canvas.Pan(-delta.x, -delta.y);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }
        
        // Tool-specific input handling
        if (currentTool == Tool::Move) {
            // Move tool: Left mouse drag to pan
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                canvas.Pan(-delta.x, -delta.y);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
        }
        else if (currentTool == Tool::Select) {
            // Select tool: Left mouse drag to create selection rectangle
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Start selection
                ImVec2 mousePos = ImGui::GetMousePos();
                selectionStartX = mousePos.x;
                selectionStartY = mousePos.y;
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                isSelecting = true;
            }
            
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isSelecting) {
                // Update selection end position
                ImVec2 mousePos = ImGui::GetMousePos();
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
            }
            
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isSelecting) {
                // Finish selection
                ImVec2 mousePos = ImGui::GetMousePos();
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                // Note: Keep selection visible until a different action
            }
        }
        else if (currentTool == Tool::Paint) {
            // Paint tool: 
            // - Room paint mode: assign/unassign cells to selected room
            // - Hover over edge: highlight and show state
            // - Click edge: cycle through None -> Wall -> Door -> None
            // - W key + click: set to Wall
            // - D key + click: set to Door
            // - Otherwise: paint/erase tiles
            
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Room paint mode: assign cells to selected room
            if (roomPaintMode && !selectedRoomId.empty()) {
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Left click: assign cell to room
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    // Check if moved to new cell or just started
                    if (!isPaintingRoomCells || tx != lastRoomPaintX || 
                        ty != lastRoomPaintY) {
                        
                        // Get cells to assign (interpolate if dragging)
                        std::vector<std::pair<int, int>> cellsToAssign;
                        if (isPaintingRoomCells && lastRoomPaintX >= 0 && 
                            lastRoomPaintY >= 0) {
                            // Interpolate from last position to current
                            cellsToAssign = GetTilesAlongLine(
                                lastRoomPaintX, lastRoomPaintY, tx, ty
                            );
                        } else {
                            // First cell
                            cellsToAssign.push_back({tx, ty});
                        }
                        
                        // Assign all cells along the line
                        for (const auto& cell : cellsToAssign) {
                            int cellX = cell.first;
                            int cellY = cell.second;
                            
                            std::string oldRoomId = model.GetCellRoom(
                                cellX, cellY
                            );
                            
                            // Only assign if different
                            if (oldRoomId != selectedRoomId) {
                                ModifyRoomAssignmentsCommand::CellAssignment 
                                    assignment;
                                assignment.x = cellX;
                                assignment.y = cellY;
                                assignment.oldRoomId = oldRoomId;
                                assignment.newRoomId = selectedRoomId;
                                
                                currentRoomAssignments.push_back(assignment);
                                
                                // Apply immediately for visual feedback
                                model.SetCellRoom(cellX, cellY, selectedRoomId);
                            }
                        }
                        
                        lastRoomPaintX = tx;
                        lastRoomPaintY = ty;
                        isPaintingRoomCells = true;
                    }
                }
                // Right click (two-finger): unassign cell from room
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    // Check if moved to new cell or just started
                    if (!isPaintingRoomCells || tx != lastRoomPaintX || 
                        ty != lastRoomPaintY) {
                        
                        // Get cells to unassign (interpolate if dragging)
                        std::vector<std::pair<int, int>> cellsToUnassign;
                        if (isPaintingRoomCells && lastRoomPaintX >= 0 && 
                            lastRoomPaintY >= 0) {
                            // Interpolate from last position to current
                            cellsToUnassign = GetTilesAlongLine(
                                lastRoomPaintX, lastRoomPaintY, tx, ty
                            );
                        } else {
                            // First cell
                            cellsToUnassign.push_back({tx, ty});
                        }
                        
                        // Unassign all cells along the line
                        for (const auto& cell : cellsToUnassign) {
                            int cellX = cell.first;
                            int cellY = cell.second;
                            
                            std::string currentRoomId = model.GetCellRoom(
                                cellX, cellY
                            );
                            
                            // Only unassign if currently assigned to selected room
                            if (currentRoomId == selectedRoomId) {
                                ModifyRoomAssignmentsCommand::CellAssignment 
                                    assignment;
                                assignment.x = cellX;
                                assignment.y = cellY;
                                assignment.oldRoomId = currentRoomId;
                                assignment.newRoomId = "";  // Empty = unassign
                                
                                currentRoomAssignments.push_back(assignment);
                                
                                // Apply immediately for visual feedback
                                model.ClearCellRoom(cellX, cellY);
                            }
                        }
                        
                        lastRoomPaintX = tx;
                        lastRoomPaintY = ty;
                        isPaintingRoomCells = true;
                    }
                }
                
                // When mouse is released, commit the room assignment command
                bool mouseReleased = 
                    ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                    ImGui::IsMouseReleased(ImGuiMouseButton_Right);
                
                if (isPaintingRoomCells && mouseReleased) {
                    if (!currentRoomAssignments.empty()) {
                        auto cmd = std::make_unique<
                            ModifyRoomAssignmentsCommand
                        >(currentRoomAssignments);
                        // Changes already applied, just store for undo/redo
                        history.AddCommand(std::move(cmd), model, false);
                        currentRoomAssignments.clear();
                    }
                    isPaintingRoomCells = false;
                    lastRoomPaintX = -1;
                    lastRoomPaintY = -1;
                }
            }
            // Regular paint mode: edges and tiles
            else {
                // First, check if we're hovering near an edge
                EdgeId edgeId;
                EdgeSide edgeSide;
                isHoveringEdge = DetectEdgeHover(
                    mousePos.x, mousePos.y, canvas, model.grid,
                    &edgeId, &edgeSide
                );
                
                if (isHoveringEdge) {
                hoveredEdge = edgeId;
                
                // Handle edge clicking
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    EdgeState currentState = model.GetEdgeState(edgeId);
                    EdgeState newState;
                    
                    // Check for direct state shortcuts
                    if (ImGui::IsKeyDown(ImGuiKey_W)) {
                        newState = EdgeState::Wall;
                    } else if (ImGui::IsKeyDown(ImGuiKey_D)) {
                        newState = EdgeState::Door;
                    } else {
                        // Cycle through states
                        newState = model.CycleEdgeState(currentState);
                    }
                    
                    // Only modify if state changed
                    if (newState != currentState) {
                        ModifyEdgesCommand::EdgeChange change;
                        change.edgeId = edgeId;
                        change.oldState = currentState;
                        change.newState = newState;
                        
                        currentEdgeChanges.push_back(change);
                        
                        // Apply immediately for visual feedback
                        model.SetEdgeState(edgeId, newState);
                        
                        // Trigger grid expansion if needed
                        int tx, ty;
                        canvas.ScreenToTile(
                            mousePos.x, mousePos.y,
                            model.grid.tileWidth, model.grid.tileHeight,
                            &tx, &ty
                        );
                        model.ExpandGridIfNeeded(tx, ty);
                        
                        isModifyingEdges = true;
                    }
                }
                
                // When mouse is released, commit edge changes
                if (isModifyingEdges && 
                    ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    if (!currentEdgeChanges.empty()) {
                        auto cmd = std::make_unique<ModifyEdgesCommand>(
                            currentEdgeChanges
                        );
                        history.AddCommand(std::move(cmd), model, false);
                        currentEdgeChanges.clear();
                    }
                    isModifyingEdges = false;
                }
                } else {
                // Not hovering edge, handle tile painting/erasing
                bool shouldPaint = false;
                bool shouldErase = false;
                
                // Check for two-finger gesture (acts as erase)
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    shouldErase = true;
                    twoFingerEraseActive = true;
                }
                // Check for E key + left mouse (erase modifier)
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && 
                         ImGui::IsKeyDown(ImGuiKey_E)) {
                    shouldErase = true;
                }
                // Check for left mouse button (primary paint input)
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    shouldPaint = true;
                }
                
                if (shouldPaint) {
                    // Convert mouse position to tile coordinates
                    int tx, ty;
                    canvas.ScreenToTile(
                        mousePos.x, mousePos.y,
                        model.grid.tileWidth, model.grid.tileHeight,
                        &tx, &ty
                    );
                    
                    // Check if we've moved to a new tile or just started
                    if (!isPainting || tx != lastPaintedTileX || 
                        ty != lastPaintedTileY) {
                        
                        // Paint tiles globally (using "" as roomId)
                        const std::string globalRoomId = "";
                        
                        // Get tiles to paint (interpolate if dragging)
                        std::vector<std::pair<int, int>> tilesToPaint;
                        if (isPainting && lastPaintedTileX >= 0 && 
                            lastPaintedTileY >= 0) {
                            // Interpolate from last position to current
                            tilesToPaint = GetTilesAlongLine(
                                lastPaintedTileX, lastPaintedTileY, 
                                tx, ty
                            );
                        } else {
                            // First tile
                            tilesToPaint.push_back({tx, ty});
                        }
                        
                        // Paint all tiles along the line
                        for (const auto& tile : tilesToPaint) {
                            int tileX = tile.first;
                            int tileY = tile.second;
                            
                            int oldTileId = model.GetTileAt(
                                globalRoomId, tileX, tileY
                            );
                            
                            // Only paint if the tile is different
                            if (oldTileId != selectedTileId) {
                                PaintTilesCommand::TileChange change;
                                change.roomId = globalRoomId;
                                change.x = tileX;
                                change.y = tileY;
                                change.oldTileId = oldTileId;
                                change.newTileId = selectedTileId;
                                
                                currentPaintChanges.push_back(change);
                                
                                // Apply immediately for visual feedback
                                model.SetTileAt(
                                    globalRoomId, tileX, tileY, selectedTileId
                                );
                            }
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
                
                
                // Handle erase with E+Mouse1 or right-click/two-finger
                if (shouldErase) {
                    // Convert mouse position to tile coordinates
                    int tx, ty;
                    canvas.ScreenToTile(
                        mousePos.x, mousePos.y,
                        model.grid.tileWidth, model.grid.tileHeight,
                        &tx, &ty
                    );
                    
                    // Check if we've moved to a new tile or just started
                    if (!isPainting || tx != lastPaintedTileX || 
                        ty != lastPaintedTileY) {
                        
                        // Erase tiles globally (using "" as roomId)
                        const std::string globalRoomId = "";
                        
                        // Get tiles to erase (interpolate if dragging)
                        std::vector<std::pair<int, int>> tilesToErase;
                        if (isPainting && lastPaintedTileX >= 0 && 
                            lastPaintedTileY >= 0) {
                            // Interpolate from last position to current
                            tilesToErase = GetTilesAlongLine(
                                lastPaintedTileX, lastPaintedTileY, 
                                tx, ty
                            );
                        } else {
                            // First tile
                            tilesToErase.push_back({tx, ty});
                        }
                        
                        // Erase all tiles along the line
                        for (const auto& tile : tilesToErase) {
                            int tileX = tile.first;
                            int tileY = tile.second;
                            
                            int oldTileId = model.GetTileAt(
                                globalRoomId, tileX, tileY
                            );
                            
                            // Only erase if there's something to erase
                            if (oldTileId != 0) {
                                PaintTilesCommand::TileChange change;
                                change.roomId = globalRoomId;
                                change.x = tileX;
                                change.y = tileY;
                                change.oldTileId = oldTileId;
                                change.newTileId = 0;  // 0 = empty/erase
                                
                                currentPaintChanges.push_back(change);
                                
                                // Apply immediately for visual feedback
                                model.SetTileAt(globalRoomId, tileX, tileY, 0);
                            }
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
                }  // End of "not hovering edge" block
            }  // End of "regular paint mode" block
        }
        else if (currentTool == Tool::Erase) {
            // Erase tool: Left mouse to erase (primary input)
            // - Hover over edge: highlight and erase on click
            // - Otherwise: erase tiles
            // Right-click also supported for consistency
            
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // First, check if we're hovering near an edge
            EdgeId edgeId;
            EdgeSide edgeSide;
            isHoveringEdge = DetectEdgeHover(
                mousePos.x, mousePos.y, canvas, model.grid,
                &edgeId, &edgeSide
            );
            
            if (isHoveringEdge) {
                hoveredEdge = edgeId;
                
                // Handle edge deletion via hover (precise mode)
                bool shouldEraseEdge = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                                      ImGui::IsMouseDown(ImGuiMouseButton_Right);
                
                if (shouldEraseEdge && !isModifyingEdges) {
                    EdgeState currentState = model.GetEdgeState(edgeId);
                    
                    // Only delete if there's an edge to delete
                    if (currentState != EdgeState::None) {
                        ModifyEdgesCommand::EdgeChange change;
                        change.edgeId = edgeId;
                        change.oldState = currentState;
                        change.newState = EdgeState::None;  // Delete edge
                        
                        currentEdgeChanges.push_back(change);
                        
                        // Apply immediately for visual feedback
                        model.SetEdgeState(edgeId, EdgeState::None);
                        
                        isModifyingEdges = true;
                    }
                }
            }
            
            // Handle tile erasing (independent of edge hover state)
            bool shouldErase = false;
            
            // Check for left mouse button (primary erase input)
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                shouldErase = true;
            }
            // Also support right-click for two-finger trackpad gestures
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                shouldErase = true;
            }
            
            if (shouldErase) {
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Check if we've moved to a new tile or just started
                if (!isPainting || tx != lastPaintedTileX || 
                    ty != lastPaintedTileY) {
                    
                    // Erase tiles globally (using "" as roomId)
                    const std::string globalRoomId = "";
                    
                    // Get tiles to erase (interpolate if dragging)
                    std::vector<std::pair<int, int>> tilesToErase;
                    if (isPainting && lastPaintedTileX >= 0 && 
                        lastPaintedTileY >= 0) {
                        // Interpolate from last position to current
                        tilesToErase = GetTilesAlongLine(
                            lastPaintedTileX, lastPaintedTileY, tx, ty
                        );
                    } else {
                        // First tile
                        tilesToErase.push_back({tx, ty});
                    }
                    
                    // Track previous tile for edge crossing detection
                    int prevX = lastPaintedTileX;
                    int prevY = lastPaintedTileY;
                    
                    // Erase all tiles along the line
                    for (const auto& tile : tilesToErase) {
                        int tileX = tile.first;
                        int tileY = tile.second;
                        
                        int oldTileId = model.GetTileAt(
                            globalRoomId, tileX, tileY
                        );
                        
                        // Only erase if there's something to erase
                        if (oldTileId != 0) {
                            PaintTilesCommand::TileChange change;
                            change.roomId = globalRoomId;
                            change.x = tileX;
                            change.y = tileY;
                            change.oldTileId = oldTileId;
                            change.newTileId = 0;  // 0 = empty/erase
                            
                            currentPaintChanges.push_back(change);
                            
                            // Apply immediately for visual feedback
                            model.SetTileAt(globalRoomId, tileX, tileY, 0);
                        }
                        
                        // Check for crossed edges when dragging
                        if (prevX >= 0 && prevY >= 0) {
                            // Moved horizontally - crossed vertical edge
                            if (tileX != prevX) {
                                EdgeSide side = (tileX > prevX) ? 
                                    EdgeSide::East : EdgeSide::West;
                                EdgeId crossedEdge = MakeEdgeId(
                                    prevX, prevY, side
                                );
                                EdgeState edgeState = model.GetEdgeState(
                                    crossedEdge
                                );
                                
                                if (edgeState != EdgeState::None) {
                                    ModifyEdgesCommand::EdgeChange change;
                                    change.edgeId = crossedEdge;
                                    change.oldState = edgeState;
                                    change.newState = EdgeState::None;
                                    
                                    currentEdgeChanges.push_back(change);
                                    model.SetEdgeState(
                                        crossedEdge, EdgeState::None
                                    );
                                }
                            }
                            
                            // Moved vertically - crossed horizontal edge
                            if (tileY != prevY) {
                                EdgeSide side = (tileY > prevY) ? 
                                    EdgeSide::South : EdgeSide::North;
                                EdgeId crossedEdge = MakeEdgeId(
                                    prevX, prevY, side
                                );
                                EdgeState edgeState = model.GetEdgeState(
                                    crossedEdge
                                );
                                
                                if (edgeState != EdgeState::None) {
                                    ModifyEdgesCommand::EdgeChange change;
                                    change.edgeId = crossedEdge;
                                    change.oldState = edgeState;
                                    change.newState = EdgeState::None;
                                    
                                    currentEdgeChanges.push_back(change);
                                    model.SetEdgeState(
                                        crossedEdge, EdgeState::None
                                    );
                                }
                            }
                        }
                        
                        // Update prev for next iteration
                        prevX = tileX;
                        prevY = tileY;
                    }
                    
                    lastPaintedTileX = tx;
                    lastPaintedTileY = ty;
                    isPainting = true;
                }
            }
        }
        else if (currentTool == Tool::Fill) {
            // Fill tool: Left-click to flood fill connected tiles
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Fill tiles globally (using "" as roomId)
                // TODO: Implement flood-fill with wall boundaries
                const std::string globalRoomId = "";
                
                // Get the original tile ID to replace
                int originalTileId = model.GetTileAt(globalRoomId, tx, ty);
                
                // Only fill if we're changing to a different tile
                if (originalTileId != selectedTileId) {
                    // Perform flood fill using BFS
                    std::vector<PaintTilesCommand::TileChange> fillChanges;
                    std::vector<std::pair<int, int>> toVisit;
                    std::set<std::pair<int, int>> visited;
                    
                    toVisit.push_back({tx, ty});
                    
                    while (!toVisit.empty()) {
                        auto [x, y] = toVisit.back();
                        toVisit.pop_back();
                        
                        // Skip if already visited
                        if (visited.count({x, y})) {
                            continue;
                        }
                        visited.insert({x, y});
                        
                        // Check grid bounds
                        if (x < 0 || x >= model.grid.cols ||
                            y < 0 || y >= model.grid.rows) {
                            continue;
                        }
                        
                        // Get current tile
                        int currentTile = model.GetTileAt(globalRoomId, x, y);
                        
                        // Skip if not matching original tile
                        if (currentTile != originalTileId) {
                            continue;
                        }
                        
                        // Add this tile to changes
                        PaintTilesCommand::TileChange change;
                        change.roomId = globalRoomId;
                        change.x = x;
                        change.y = y;
                        change.oldTileId = originalTileId;
                        change.newTileId = selectedTileId;
                        fillChanges.push_back(change);
                        
                        // Add neighbors to visit (4-way connectivity)
                        toVisit.push_back({x + 1, y});
                        toVisit.push_back({x - 1, y});
                        toVisit.push_back({x, y + 1});
                        toVisit.push_back({x, y - 1});
                    }
                    
                    // Apply all changes and add to history
                    if (!fillChanges.empty()) {
                        // Apply changes immediately
                        for (const auto& change : fillChanges) {
                            model.SetTileAt(
                                change.roomId, 
                                change.x, 
                                change.y, 
                                change.newTileId
                            );
                        }
                        
                        // Add to history
                        auto cmd = std::make_unique<FillTilesCommand>(
                            fillChanges
                        );
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
            }
        }
        else if (currentTool == Tool::Eyedropper) {
            // Eyedropper tool: Left-click to pick tile color/ID
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Pick tile globally (using "" as roomId)
                const std::string globalRoomId = "";
                int pickedTileId = model.GetTileAt(globalRoomId, tx, ty);
                
                // Only pick non-empty tiles
                if (pickedTileId != 0) {
                    selectedTileId = pickedTileId;
                    ShowToast("Picked tile #" + std::to_string(pickedTileId), 
                             Toast::Type::Success, 1.5f);
                    
                    // Auto-switch to Paint tool after picking
                    currentTool = Tool::Paint;
                }
                // Note: "No tile to pick" message shown in console only
            }
        }
        else if (currentTool == Tool::Marker) {
            // Marker tool: Left-click to place/edit markers
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert to world coordinates
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                // Convert to fractional tile coordinates
                float tileX = wx / model.grid.tileWidth;
                float tileY = wy / model.grid.tileHeight;
                
                // Snap to nearest snap point based on grid preset
                auto snapPoints = model.GetMarkerSnapPoints();
                int baseTileX = static_cast<int>(std::floor(tileX));
                int baseTileY = static_cast<int>(std::floor(tileY));
                float fractionalX = tileX - baseTileX;
                float fractionalY = tileY - baseTileY;
                
                float minDist = FLT_MAX;
                float bestSnapX = 0.5f, bestSnapY = 0.5f;
                
                for (const auto& snap : snapPoints) {
                    float dx = fractionalX - snap.first;
                    float dy = fractionalY - snap.second;
                    float dist = dx*dx + dy*dy;
                    if (dist < minDist) {
                        minDist = dist;
                        bestSnapX = snap.first;
                        bestSnapY = snap.second;
                    }
                }
                
                tileX = baseTileX + bestSnapX;
                tileY = baseTileY + bestSnapY;
                
                // Check if we clicked near an existing marker
                Marker* clickedMarker = 
                    model.FindMarkerNear(tileX, tileY, 0.5f);
                
                if (clickedMarker && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                    // Shift+Click: Delete marker
                    auto cmd = std::make_unique<DeleteMarkerCommand>(
                        clickedMarker->id
                    );
                    history.AddCommand(std::move(cmd), model);
                    
                    selectedMarker = nullptr;  // Clear selection
                } else if (clickedMarker) {
                    // Click existing marker: Select and start drag
                    selectedMarker = clickedMarker;
                    isDraggingMarker = true;
                    dragStartX = clickedMarker->x;
                    dragStartY = clickedMarker->y;
                    
                    // Load marker properties into UI
                    selectedIconName = clickedMarker->icon;
                    markerLabel = clickedMarker->label;
                    markerColor = clickedMarker->color;
                } else {
                    // Place new marker
                    Marker newMarker;
                    newMarker.id = model.GenerateMarkerId();
                    newMarker.roomId = "";  // Global for now
                    newMarker.x = tileX;
                    newMarker.y = tileY;
                    newMarker.kind = "custom";
                    newMarker.label = markerLabel;
                    newMarker.icon = selectedIconName;
                    newMarker.color = markerColor;
                    newMarker.size = 0.6f;
                    newMarker.showLabel = !markerLabel.empty();
                    
                    auto cmd = std::make_unique<PlaceMarkerCommand>(
                        newMarker, true
                    );
                    history.AddCommand(std::move(cmd), model);
                }
            }
            
            // Right-click (two-finger) to delete marker
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert to world coordinates
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                // Convert to fractional tile coordinates
                float tileX = wx / model.grid.tileWidth;
                float tileY = wy / model.grid.tileHeight;
                
                // Check if we right-clicked near an existing marker
                Marker* clickedMarker = 
                    model.FindMarkerNear(tileX, tileY, 0.5f);
                
                if (clickedMarker) {
                    // Delete marker
                    auto cmd = std::make_unique<DeleteMarkerCommand>(
                        clickedMarker->id
                    );
                    history.AddCommand(std::move(cmd), model);
                    
                    // Clear selection if we deleted the selected marker
                    if (selectedMarker && selectedMarker->id == clickedMarker->id) {
                        selectedMarker = nullptr;
                    }
                }
            }
            
            // Handle marker dragging
            if (isDraggingMarker && selectedMarker) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    // Update marker position while dragging
                    ImVec2 mousePos = ImGui::GetMousePos();
                    
                    float wx, wy;
                    canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                    
                    float tileX = wx / model.grid.tileWidth;
                    float tileY = wy / model.grid.tileHeight;
                    
                    // Snap to nearest snap point based on grid preset
                    auto snapPoints = model.GetMarkerSnapPoints();
                    int baseTileX = static_cast<int>(std::floor(tileX));
                    int baseTileY = static_cast<int>(std::floor(tileY));
                    float fractionalX = tileX - baseTileX;
                    float fractionalY = tileY - baseTileY;
                    
                    float minDist = FLT_MAX;
                    float bestSnapX = 0.5f, bestSnapY = 0.5f;
                    
                    for (const auto& snap : snapPoints) {
                        float dx = fractionalX - snap.first;
                        float dy = fractionalY - snap.second;
                        float dist = dx*dx + dy*dy;
                        if (dist < minDist) {
                            minDist = dist;
                            bestSnapX = snap.first;
                            bestSnapY = snap.second;
                        }
                    }
                    
                    tileX = baseTileX + bestSnapX;
                    tileY = baseTileY + bestSnapY;
                    
                    // Update marker position
                    selectedMarker->x = tileX;
                    selectedMarker->y = tileY;
                    model.MarkDirty();
                } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    // Finish dragging, create command for undo
                    if (dragStartX != selectedMarker->x || 
                        dragStartY != selectedMarker->y) {
                        auto cmd = std::make_unique<MoveMarkersCommand>(
                            selectedMarker->id,
                            dragStartX, dragStartY,
                            selectedMarker->x, selectedMarker->y
                        );
                        history.AddCommand(std::move(cmd), model, false);
                    }
                    
                    isDraggingMarker = false;
                }
            }
        }
    }
    
    // Handle mouse release for Paint/Erase tools (outside hover check)
    // This ensures releases are detected even if mouse drifts outside canvas
    if (currentTool == Tool::Paint || currentTool == Tool::Erase) {
        // Check for paint/erase tile release
        bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                             ImGui::IsMouseReleased(ImGuiMouseButton_Right);
        
        if (isPainting && mouseReleased) {
            if (!currentPaintChanges.empty()) {
                auto cmd = std::make_unique<PaintTilesCommand>(
                    currentPaintChanges
                );
                // Changes already applied, just store for undo/redo
                history.AddCommand(std::move(cmd), model, false);
                currentPaintChanges.clear();
            }
            isPainting = false;
            lastPaintedTileX = -1;
            lastPaintedTileY = -1;
            twoFingerEraseActive = false;
        }
        
        // Check for edge modification release (Erase tool)
        if (isModifyingEdges && mouseReleased) {
            if (!currentEdgeChanges.empty()) {
                auto cmd = std::make_unique<ModifyEdgesCommand>(
                    currentEdgeChanges
                );
                history.AddCommand(std::move(cmd), model, false);
                currentEdgeChanges.clear();
            }
            isModifyingEdges = false;
        }
    }
    
    // Clear selection if we click outside canvas
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
        !ImGui::IsItemHovered() && isSelecting) {
        isSelecting = false;
    }
    
    // Keyboard shortcuts for markers
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // Copy selected marker
        if (keymap.IsActionTriggered("copy")) {
            if (selectedMarker) {
                copiedMarkers.clear();
                copiedMarkers.push_back(*selectedMarker);
            }
        }
        
        // Paste marker
        if (keymap.IsActionTriggered("paste")) {
            if (!copiedMarkers.empty()) {
                // Paste at mouse position or offset from original
                ImVec2 mousePos = ImGui::GetMousePos();
                float wx, wy;
                canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
                
                float tileX = wx / model.grid.tileWidth;
                float tileY = wy / model.grid.tileHeight;
                
                // Snap to nearest snap point based on grid preset
                auto snapPoints = model.GetMarkerSnapPoints();
                int baseTileX = static_cast<int>(std::floor(tileX));
                int baseTileY = static_cast<int>(std::floor(tileY));
                float fractionalX = tileX - baseTileX;
                float fractionalY = tileY - baseTileY;
                
                float minDist = FLT_MAX;
                float bestSnapX = 0.5f, bestSnapY = 0.5f;
                
                for (const auto& snap : snapPoints) {
                    float dx = fractionalX - snap.first;
                    float dy = fractionalY - snap.second;
                    float dist = dx*dx + dy*dy;
                    if (dist < minDist) {
                        minDist = dist;
                        bestSnapX = snap.first;
                        bestSnapY = snap.second;
                    }
                }
                
                tileX = baseTileX + bestSnapX;
                tileY = baseTileY + bestSnapY;
                
                for (const auto& marker : copiedMarkers) {
                    Marker newMarker = marker;
                    newMarker.id = model.GenerateMarkerId();
                    newMarker.x = tileX;
                    newMarker.y = tileY;
                    
                    auto cmd = std::make_unique<PlaceMarkerCommand>(
                        newMarker, true
                    );
                    history.AddCommand(std::move(cmd), model);
                }
            }
        }
        
        // Delete selected marker
        if (selectedMarker && 
            (keymap.IsActionTriggered("delete") || 
             keymap.IsActionTriggered("deleteAlt"))) {
            auto cmd = std::make_unique<DeleteMarkerCommand>(
                selectedMarker->id
            );
            history.AddCommand(std::move(cmd), model);   
            selectedMarker = nullptr;
        }
    }
    
    // Draw canvas overlays using window DrawList
    // (renders above canvas texture but below UI elements like tooltips/menus)
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Clip all canvas drawing to window bounds (prevent overlap with other panels)
    ImVec2 canvasMin = ImGui::GetWindowPos();
    ImVec2 canvasMax = ImVec2(canvasMin.x + ImGui::GetWindowSize().x,
                               canvasMin.y + ImGui::GetWindowSize().y);
    drawList->PushClipRect(canvasMin, canvasMax, true);
    
    // Draw background
    ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    drawList->AddRectFilled(canvasPos, 
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
        bgColor);
    
    // Update hovered tile coordinates (for status bar)
    if (ImGui::IsItemHovered()) {
        isHoveringCanvas = true;
        ImVec2 mousePos = ImGui::GetMousePos();
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &hoveredTileX, &hoveredTileY
        );
    } else {
        isHoveringCanvas = false;
        hoveredTileX = -1;
        hoveredTileY = -1;
    }
    
    // Update hovered marker (if Marker tool is active)
    if (currentTool == Tool::Marker && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float wx, wy;
        canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
        
        float tileX = wx / model.grid.tileWidth;
        float tileY = wy / model.grid.tileHeight;
        
        hoveredMarker = model.FindMarkerNear(tileX, tileY, 0.5f);
        
        // Show tooltip for hovered marker
        if (hoveredMarker && !isDraggingMarker) {
            ImGui::BeginTooltip();
            ImGui::Text("Marker: %s", hoveredMarker->label.empty() ? 
                       "(no label)" : hoveredMarker->label.c_str());
            ImGui::TextDisabled("Icon: %s", hoveredMarker->icon.c_str());
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               hoveredMarker->x, hoveredMarker->y);
            ImGui::Separator();
            ImGui::TextDisabled("Click: Select/Move");
            ImGui::TextDisabled("Shift+Click: Delete");
            ImGui::EndTooltip();
        }
    } else {
        hoveredMarker = nullptr;
    }
    
    // Render the actual canvas content (grid, tiles, rooms, doors, markers, overlays)
    canvas.Render(
        renderer,
        model,
        &icons,
        static_cast<int>(canvasPos.x),
        static_cast<int>(canvasPos.y),
        static_cast<int>(canvasSize.x),
        static_cast<int>(canvasSize.y),
        isHoveringEdge ? &hoveredEdge : nullptr,
        showRoomOverlays,  // Pass room overlay toggle state
        selectedMarker,    // Pass selected marker for highlight
        hoveredMarker      // Pass hovered marker for highlight
    );
    
    // Note: Thumbnail capture moved to App::Render() after ImGui draw data
    // is rendered to ensure pixels are actually in the framebuffer
    
    // Draw selection rectangle if Select tool is active
    if (currentTool == Tool::Select && isSelecting) {
        // Calculate rectangle bounds
        float minX = std::min(selectionStartX, selectionEndX);
        float minY = std::min(selectionStartY, selectionEndY);
        float maxX = std::max(selectionStartX, selectionEndX);
        float maxY = std::max(selectionStartY, selectionEndY);
        
        // Draw semi-transparent fill
        ImU32 fillColor = ImGui::GetColorU32(ImVec4(0.3f, 0.6f, 1.0f, 0.2f));
        drawList->AddRectFilled(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            fillColor
        );
        
        // Draw border
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.3f, 0.6f, 1.0f, 0.8f));
        drawList->AddRect(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            borderColor,
            0.0f,
            0,
            2.0f
        );
    }
    
    // Draw paint cursor preview if Paint or Erase tool is active
    if ((currentTool == Tool::Paint || currentTool == Tool::Erase) && 
        ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Convert back to screen coordinates (snapped to grid)
        float wx, wy;
        canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                          model.grid.tileHeight, &wx, &wy);
        
        float sx, sy;
        canvas.WorldToScreen(wx, wy, &sx, &sy);
        
        float sw = model.grid.tileWidth * canvas.zoom;
        float sh = model.grid.tileHeight * canvas.zoom;
        
        // Draw preview based on tool and state
        // TODO: Make these colors customizable in theme/settings
        if (currentTool == Tool::Erase || 
            (currentTool == Tool::Paint && ImGui::IsKeyDown(ImGuiKey_E))) {
            // Erase preview (red fill + red outline)
            ImU32 eraseColor = ImGui::GetColorU32(
                ImVec4(1.0f, 0.3f, 0.3f, 0.6f)
            );
            drawList->AddRect(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                eraseColor,
                0.0f,
                0,
                2.0f
            );
        } else {
            // Paint preview (brightened tile color + white border)
            Color tileColor(0.8f, 0.8f, 0.8f, 0.4f);
            for (const auto& tile : model.palette) {
                if (tile.id == selectedTileId) {
                    tileColor = tile.color;
                    break;
                }
            }
            
            // Brighten color by 30% for visibility
            // TODO: Make brightness boost customizable
            float brightenAmount = 0.3f;
            Color brightened;
            brightened.r = std::min(tileColor.r + brightenAmount, 1.0f);
            brightened.g = std::min(tileColor.g + brightenAmount, 1.0f);
            brightened.b = std::min(tileColor.b + brightenAmount, 1.0f);
            brightened.a = 0.6f;  // Semi-transparent
            
            // Draw preview fill
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                brightened.ToU32()
            );
            
            // Draw white border for visibility
            // TODO: Make border color/thickness customizable
            ImU32 borderColor = ImGui::GetColorU32(
                ImVec4(1.0f, 1.0f, 1.0f, 0.9f)
            );
            float borderThickness = 3.0f;
            drawList->AddRect(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                borderColor,
                0.0f,
                0,
                borderThickness
            );
        }
        
        // Draw edge hover preview for Paint tool
        if (currentTool == Tool::Paint && isHoveringEdge) {
            // Calculate edge line endpoints based on hovered edge
            int x1 = hoveredEdge.x1;
            int y1 = hoveredEdge.y1;
            int x2 = hoveredEdge.x2;
            int y2 = hoveredEdge.y2;
            
            // Determine if this is a vertical or horizontal edge
            bool isVertical = (x1 != x2);
            
            float wx1, wy1, wx2, wy2;
            if (isVertical) {
                // Vertical edge
                wx1 = std::max(x1, x2) * model.grid.tileWidth;
                wx2 = wx1;
                wy1 = std::min(y1, y2) * model.grid.tileHeight;
                wy2 = wy1 + model.grid.tileHeight;
            } else {
                // Horizontal edge
                wy1 = std::max(y1, y2) * model.grid.tileHeight;
                wy2 = wy1;
                wx1 = std::min(x1, x2) * model.grid.tileWidth;
                wx2 = wx1 + model.grid.tileWidth;
            }
            
            // Convert to screen coordinates
            float esx1, esy1, esx2, esy2;
            canvas.WorldToScreen(wx1, wy1, &esx1, &esy1);
            canvas.WorldToScreen(wx2, wy2, &esx2, &esy2);
            
            // Get current edge state
            EdgeState currentState = model.GetEdgeState(hoveredEdge);
            
            // Draw ghost preview showing what will happen
            ImU32 edgePreviewColor;
            if (currentState == EdgeState::None) {
                // No edge exists - show wall preview (green)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(0.3f, 1.0f, 0.3f, 0.7f)
                );
            } else if (currentState == EdgeState::Wall) {
                // Wall exists - show door preview (blue)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(0.3f, 0.6f, 1.0f, 0.7f)
                );
            } else {
                // Door exists - show none/delete preview (red)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(1.0f, 0.3f, 0.3f, 0.7f)
                );
            }
            
            // Draw preview line (thicker to be visible)
            drawList->AddLine(
                ImVec2(esx1, esy1),
                ImVec2(esx2, esy2),
                edgePreviewColor,
                4.0f * canvas.zoom
            );
        }
    }
    
    // Draw fill cursor preview if Fill tool is active
    if (currentTool == Tool::Fill && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Convert back to screen coordinates (snapped to grid)
        float wx, wy;
        canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                          model.grid.tileHeight, &wx, &wy);
        
        float sx, sy;
        canvas.WorldToScreen(wx, wy, &sx, &sy);
        
        float sw = model.grid.tileWidth * canvas.zoom;
        float sh = model.grid.tileHeight * canvas.zoom;
        
        // Get selected tile color for preview
        Color tileColor(0.8f, 0.8f, 0.8f, 0.6f);
        for (const auto& tile : model.palette) {
            if (tile.id == selectedTileId) {
                tileColor = tile.color;
                tileColor.a = 0.6f;  // More opaque for fill preview
                break;
            }
        }
        
        // Draw bucket icon indicator (center cross)
        float centerX = sx + sw / 2.0f;
        float centerY = sy + sh / 2.0f;
        float crossSize = std::min(sw, sh) * 0.3f;
        ImU32 crossColor = ImGui::GetColorU32(
            ImVec4(1.0f, 1.0f, 1.0f, 0.8f)
        );
        
        // Vertical line of cross
        drawList->AddLine(
            ImVec2(centerX, centerY - crossSize),
            ImVec2(centerX, centerY + crossSize),
            crossColor,
            2.0f
        );
        
        // Horizontal line of cross
        drawList->AddLine(
            ImVec2(centerX - crossSize, centerY),
            ImVec2(centerX + crossSize, centerY),
            crossColor,
            2.0f
        );
        
        // Draw semi-transparent fill preview
        ImU32 previewColor = tileColor.ToU32();
        drawList->AddRectFilled(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            previewColor
        );
        
        // Draw border
        drawList->AddRect(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.6f)),
            0.0f,
            0,
            2.0f
        );
    }
    
    // Draw marker snap point preview if Marker tool is active
    if (currentTool == Tool::Marker && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to world coordinates
        float wx, wy;
        canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
        
        // Convert to fractional tile coordinates
        float tileX = wx / model.grid.tileWidth;
        float tileY = wy / model.grid.tileHeight;
        
        // Get base tile
        int baseTileX = static_cast<int>(std::floor(tileX));
        int baseTileY = static_cast<int>(std::floor(tileY));
        float fractionalX = tileX - baseTileX;
        float fractionalY = tileY - baseTileY;
        
        // Get snap points for current preset
        auto snapPoints = model.GetMarkerSnapPoints();
        
        // Find nearest snap point
        float minDist = FLT_MAX;
        float bestSnapX = 0.5f, bestSnapY = 0.5f;
        
        for (const auto& snap : snapPoints) {
            float dx = fractionalX - snap.first;
            float dy = fractionalY - snap.second;
            float dist = dx*dx + dy*dy;
            if (dist < minDist) {
                minDist = dist;
                bestSnapX = snap.first;
                bestSnapY = snap.second;
            }
        }
        
        // Calculate final snapped position
        float snappedTileX = baseTileX + bestSnapX;
        float snappedTileY = baseTileY + bestSnapY;
        
        // Convert to world coordinates
        float snappedWx = snappedTileX * model.grid.tileWidth;
        float snappedWy = snappedTileY * model.grid.tileHeight;
        
        // Convert to screen coordinates
        float snappedSx, snappedSy;
        canvas.WorldToScreen(snappedWx, snappedWy, &snappedSx, &snappedSy);
        
        // Draw all snap points for current tile (subtle indicators)
        for (const auto& snap : snapPoints) {
            float snapWx = (baseTileX + snap.first) * model.grid.tileWidth;
            float snapWy = (baseTileY + snap.second) * model.grid.tileHeight;
            
            float snapSx, snapSy;
            canvas.WorldToScreen(snapWx, snapWy, &snapSx, &snapSy);
            
            // Draw small dot at snap point
            float dotRadius = 3.0f;
            ImU32 dotColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.4f));
            drawList->AddCircleFilled(
                ImVec2(snapSx, snapSy),
                dotRadius,
                dotColor,
                8
            );
        }
        
        // Draw ghost marker at snapped position (larger, highlighted)
        float minDim = static_cast<float>(std::min(model.grid.tileWidth, model.grid.tileHeight));
        float markerSize = minDim * canvas.zoom * 0.6f;
        
        // Draw ghost marker with pulsing effect
        ImU32 ghostColor = ImGui::GetColorU32(ImVec4(
            markerColor.r, 
            markerColor.g, 
            markerColor.b, 
            0.5f  // Semi-transparent
        ));
        
        drawList->AddCircleFilled(
            ImVec2(snappedSx, snappedSy),
            markerSize / 2.0f,
            ghostColor,
            16
        );
        
        // Draw border around ghost marker
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
        drawList->AddCircle(
            ImVec2(snappedSx, snappedSy),
            markerSize / 2.0f,
            borderColor,
            16,
            2.0f
        );
        
        // Draw crosshair at snap point for precision
        float crossSize = 8.0f;
        ImU32 crossColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.8f));
        
        drawList->AddLine(
            ImVec2(snappedSx - crossSize, snappedSy),
            ImVec2(snappedSx + crossSize, snappedSy),
            crossColor,
            1.5f
        );
        drawList->AddLine(
            ImVec2(snappedSx, snappedSy - crossSize),
            ImVec2(snappedSx, snappedSy + crossSize),
            crossColor,
            1.5f
        );
    }
    
    // Draw drag-drop preview when dragging icon over canvas
    if (ImGui::IsDragDropActive() && ImGui::IsItemHovered()) {
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        if (payload && payload->IsDataType("MARKER_ICON")) {
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert to world coordinates
            float wx, wy;
            canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
            
            // Convert to fractional tile coordinates
            float tileX = wx / model.grid.tileWidth;
            float tileY = wy / model.grid.tileHeight;
            
            // Snap to nearest snap point
            auto snapPoints = model.GetMarkerSnapPoints();
            int baseTileX = static_cast<int>(std::floor(tileX));
            int baseTileY = static_cast<int>(std::floor(tileY));
            float fractionalX = tileX - baseTileX;
            float fractionalY = tileY - baseTileY;
            
            float minDist = FLT_MAX;
            float bestSnapX = 0.5f, bestSnapY = 0.5f;
            
            for (const auto& snap : snapPoints) {
                float dx = fractionalX - snap.first;
                float dy = fractionalY - snap.second;
                float dist = dx*dx + dy*dy;
                if (dist < minDist) {
                    minDist = dist;
                    bestSnapX = snap.first;
                    bestSnapY = snap.second;
                }
            }
            
            float snappedTileX = baseTileX + bestSnapX;
            float snappedTileY = baseTileY + bestSnapY;
            
            // Convert to screen coordinates
            float snappedWx = snappedTileX * model.grid.tileWidth;
            float snappedWy = snappedTileY * model.grid.tileHeight;
            float snappedSx, snappedSy;
            canvas.WorldToScreen(snappedWx, snappedWy, &snappedSx, &snappedSy);
            
            // Draw ghost marker preview
            float minDim = static_cast<float>(std::min(model.grid.tileWidth, model.grid.tileHeight));
            float markerSize = minDim * canvas.zoom * 0.6f;
            
            ImU32 ghostColor = ImGui::GetColorU32(ImVec4(
                markerColor.r, markerColor.g, markerColor.b, 0.5f
            ));
            
            drawList->AddCircleFilled(
                ImVec2(snappedSx, snappedSy),
                markerSize / 2.0f,
                ghostColor,
                16
            );
            
            // Draw border
            ImU32 borderColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
            drawList->AddCircle(
                ImVec2(snappedSx, snappedSy),
                markerSize / 2.0f,
                borderColor,
                16,
                2.0f
            );
            
            // Draw "drop here" text
            const char* dropText = "Drop to place marker";
            ImVec2 textSize = ImGui::CalcTextSize(dropText);
            ImVec2 textPos(snappedSx - textSize.x / 2.0f, 
                          snappedSy + markerSize / 2.0f + 8.0f);
            
            // Text background
            drawList->AddRectFilled(
                ImVec2(textPos.x - 4, textPos.y - 2),
                ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
                ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f))
            );
            
            drawList->AddText(textPos, 
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 
                dropText);
        }
    }
    
    // Pop clip rect before ending window
    drawList->PopClipRect();
    
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
        if (isHoveringCanvas && hoveredTileX >= 0 && hoveredTileY >= 0) {
            ImGui::Text("Tile: %d, %d", hoveredTileX, hoveredTileY);
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

void UI::RenderExportModal(Model& model, Canvas& canvas) {
    ImGui::OpenPopup("Export PNG");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
                           ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Export PNG", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Calculate content dimensions for display
        ContentBounds bounds = model.CalculateContentBounds();
        
        // Check if there's content to export
        if (bounds.isEmpty) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                "Warning: No content to export!");
            ImGui::Text("Draw some tiles, walls, or markers first.");
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                showExportModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }
        
        int contentWidthTiles = bounds.maxX - bounds.minX + 1;
        int contentHeightTiles = bounds.maxY - bounds.minY + 1;
        int contentWidthPx = contentWidthTiles * model.grid.tileWidth;
        int contentHeightPx = contentHeightTiles * model.grid.tileHeight;
        
        // Display content info
        ImGui::Text("Content Area: %d × %d pixels (%d × %d tiles)",
                   contentWidthPx, contentHeightPx,
                   contentWidthTiles, contentHeightTiles);
        ImGui::Separator();
        
        // Size mode selection
        ImGui::Text("Size Mode:");
        
        int mode = (int)exportOptions.sizeMode;
        if (ImGui::RadioButton("Scale", &mode, 0)) {
            exportOptions.sizeMode = ExportOptions::SizeMode::Scale;
        }
        
        if (exportOptions.sizeMode == ExportOptions::SizeMode::Scale) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderInt("##scale", &exportOptions.scale, 1, 4);
            
            int outW = (contentWidthPx + exportOptions.padding * 2) * 
                      exportOptions.scale;
            int outH = (contentHeightPx + exportOptions.padding * 2) * 
                      exportOptions.scale;
            ImGui::SameLine();
            ImGui::Text("→ %d × %d px", outW, outH);
        }
        
        if (ImGui::RadioButton("Custom Dimensions", &mode, 1)) {
            exportOptions.sizeMode = 
                ExportOptions::SizeMode::CustomDimensions;
        }
        
        if (exportOptions.sizeMode == 
            ExportOptions::SizeMode::CustomDimensions) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Width", &exportOptions.customWidth);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Height", &exportOptions.customHeight);
            
            // Clamp to reasonable range
            exportOptions.customWidth = std::clamp(
                exportOptions.customWidth, 64, 
                ExportOptions::MAX_DIMENSION);
            exportOptions.customHeight = std::clamp(
                exportOptions.customHeight, 64, 
                ExportOptions::MAX_DIMENSION);
            
            ImGui::Text("(scales to fit, maintains aspect ratio)");
            ImGui::Unindent();
        }
        
        ImGui::Separator();
        
        // Padding control
        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("Padding (px)", &exportOptions.padding, 0, 64);
        
        ImGui::Separator();
        
        // Transparency
        ImGui::Checkbox("Transparency", &exportOptions.transparency);
        
        if (!exportOptions.transparency) {
            ImGui::SameLine();
            ImGui::ColorEdit3("Background", &exportOptions.bgColorR);
        }
        
        ImGui::Separator();
        ImGui::Text("Layers");
        ImGui::Checkbox("Grid", &exportOptions.layerGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Tiles", &exportOptions.layerTiles);
        ImGui::Checkbox("Walls & Doors", &exportOptions.layerDoors);
        ImGui::SameLine();
        ImGui::Checkbox("Markers", &exportOptions.layerMarkers);
        
        ImGui::Separator();
        
        // Export button - triggers file dialog
        if (ImGui::Button("Export...", ImVec2(120, 0))) {
            showExportModal = false;
            ImGui::CloseCurrentPopup();
            // Trigger the export dialog (will be called after modal closes)
            shouldShowExportPngDialog = true;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showExportModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderSettingsModal(Model& model) {
    ImGui::OpenPopup("Settings");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Settings", nullptr, 
        ImGuiWindowFlags_NoResize)) {
        
        // Apply increased padding for better spacing
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        
        // Style the tab bar for better visibility
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, 
            ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, 
            ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
        
        if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
            
            // ============================================================
            // TAB 1: PROJECT
            // ============================================================
            if (ImGui::BeginTabItem("Project")) {
                settingsModalSelectedTab = 0;
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::Text("Project Information");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Project metadata
        char titleBuf[256];
                strncpy(titleBuf, model.meta.title.c_str(), 
                    sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            model.meta.title = titleBuf;
            model.MarkDirty();
        }
        
        char authorBuf[256];
                strncpy(authorBuf, model.meta.author.c_str(), 
                    sizeof(authorBuf) - 1);
        authorBuf[sizeof(authorBuf) - 1] = '\0';
        if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
            model.meta.author = authorBuf;
            model.MarkDirty();
        }
        
        ImGui::Spacing();
        
        // Project description (multi-line)
        char descBuf[2048];
                strncpy(descBuf, model.meta.description.c_str(), 
                    sizeof(descBuf) - 1);
        descBuf[sizeof(descBuf) - 1] = '\0';
        ImGui::Text("Description");
        if (ImGui::InputTextMultiline("##description", descBuf, 
                                      sizeof(descBuf),
                                      ImVec2(-1, 120.0f),
                                      ImGuiInputTextFlags_AllowTabInput)) {
            model.meta.description = descBuf;
            model.MarkDirty();
        }
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Brief description of your project (optional)");
        
        ImGui::Spacing();
        ImGui::Separator();
                ImGui::Spacing();
                
                // Preview info
                ImGui::Text("Canvas Information");
                ImGui::Separator();
                ImGui::Spacing();
                
                int totalCells = model.grid.cols * model.grid.rows;
                int pixelWidth = model.grid.cols * model.grid.tileWidth;
                int pixelHeight = model.grid.rows * model.grid.tileHeight;
                
                ImGui::Text("Total cells: %d", totalCells);
                ImGui::Text("Canvas size: %d × %d pixels", 
                    pixelWidth, pixelHeight);
                ImGui::Text("Cell size: %d × %d pixels", 
                    model.grid.tileWidth, model.grid.tileHeight);
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 2: GRID & CANVAS
            // ============================================================
            if (ImGui::BeginTabItem("Grid & Canvas")) {
                settingsModalSelectedTab = 1;
                ImGui::Spacing();
                ImGui::Spacing();
        
        // Grid Cell Type
                ImGui::Text("Grid Cell Type");
                ImGui::Separator();
                ImGui::Spacing();
        
        bool canChangePreset = model.CanChangeGridPreset();
        
        if (!canChangePreset) {
            ImGui::BeginDisabled();
        }
        
        bool isSquare = (model.grid.preset == GridPreset::Square);
        if (ImGui::RadioButton("Square (16×16)", isSquare)) {
            if (canChangePreset) {
                model.ApplyGridPreset(GridPreset::Square);
            } else {
                        ShowToast(
                            "Cannot change cell type - delete all markers first",
                    Toast::Type::Warning, 3.0f);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Square cells for top-down games. "
                            "Markers snap to center only.");
        }
        
                bool isRectangle = 
                    (model.grid.preset == GridPreset::Rectangle);
        if (ImGui::RadioButton("Rectangle (32×16)", isRectangle)) {
            if (canChangePreset) {
                model.ApplyGridPreset(GridPreset::Rectangle);
            } else {
                        ShowToast(
                            "Cannot change cell type - delete all markers first",
                    Toast::Type::Warning, 3.0f);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Rectangular cells for side-scrollers. "
                            "Markers snap to left/right positions.");
        }
        
        if (!canChangePreset) {
            ImGui::EndDisabled();
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f),
                "🔒 Locked (%zu marker%s placed)",
                model.markers.size(),
                model.markers.size() == 1 ? "" : "s");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Delete all markers to change cell type");
            }
        }
        
        // Show current dimensions (read-only info)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Cell Dimensions: %d×%d px",
            model.grid.tileWidth, model.grid.tileHeight);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
                // Edge Configuration
                ImGui::Text("Edge/Wall Configuration");
        ImGui::Separator();
        ImGui::Spacing();
        
                ImGui::Checkbox("Auto-expand grid", 
                    &model.grid.autoExpandGrid);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically expand grid when placing edges "
                            "near boundaries");
        }
        
        ImGui::SetNextItemWidth(250.0f);
        ImGui::SliderInt("Expansion threshold (cells)", 
                        &model.grid.expansionThreshold, 1, 20);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Distance from grid boundary to trigger "
                            "expansion");
        }
        
        ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Expansion factor", 
                    &model.grid.expansionFactor, 
                          1.1f, 3.0f, "%.1fx");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Grid growth multiplier "
                            "(e.g., 1.5x = 50%% growth)");
        }
        
        ImGui::SetNextItemWidth(250.0f);
        ImGui::SliderFloat("Edge hover threshold", 
                          &model.grid.edgeHoverThreshold, 
                          0.1f, 0.5f, "%.2f");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Distance from cell edge to activate edge mode "
                            "(0.2 = 20%% of cell size)");
        }
        
        ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 3: KEYBINDINGS
            // ============================================================
            if (ImGui::BeginTabItem("Keybindings")) {
                settingsModalSelectedTab = 2;
                ImGui::Spacing();
                ImGui::Spacing();
        
                ImGui::Text("Keyboard Shortcuts");
                ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::BeginTable("KeybindingsTable", 2, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            
                    ImGui::TableSetupColumn("Action", 
                        ImGuiTableColumnFlags_WidthFixed, 250.0f);
                    ImGui::TableSetupColumn("Binding", 
                        ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            
            // Display key tool bindings
            const char* actions[] = {
                        "Paint (primary)", 
                        "Erase (primary)", 
                        "Erase (alternate)",
                        "Tool: Move", 
                        "Tool: Paint", 
                        "Tool: Erase", 
                        "Tool: Eyedropper"
            };
            const char* keys[] = {
                "paint", "erase", "eraseAlt",
                "toolMove", "toolPaint", "toolErase", "eyedropper"
            };
            
            for (int i = 0; i < 7; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", actions[i]);
                
                ImGui::TableNextColumn();
                auto it = model.keymap.find(keys[i]);
                if (it != model.keymap.end()) {
                            ImGui::TextColored(
                                ImVec4(0.7f, 0.9f, 1.0f, 1.0f), 
                                      "%s", it->second.c_str());
                } else {
                            ImGui::TextColored(
                                ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
                                      "Not bound");
                }
                
                // TODO: Add button to rebind key
            }
            
            ImGui::EndTable();
        }
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "Note: Keybinding customization coming soon!");
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons (outside tab content, always visible)
        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            model.MarkDirty();
            showSettingsModal = false;
            ImGui::CloseCurrentPopup();
            ShowToast("Settings applied", Toast::Type::Success);
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showSettingsModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderRenameIconModal(Model& model, IconManager& icons) {
    ImGui::OpenPopup("Rename Icon");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Rename Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Current name: %s", renameIconOldName.c_str());
        ImGui::Spacing();
        
        // New name input
        ImGui::InputText("New name", renameIconNewName, 
                        sizeof(renameIconNewName));
        
        ImGui::Spacing();
        ImGui::Separator();
        
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            std::string newName(renameIconNewName);
            std::string errorMsg;
            
            // Validate and rename
            if (icons.RenameIcon(renameIconOldName, newName, errorMsg)) {
                // Update all markers using this icon
                int count = model.UpdateMarkerIconNames(
                    renameIconOldName, newName);
                
                // Update selected icon if it was the renamed one
                if (selectedIconName == renameIconOldName) {
                    selectedIconName = newName;
                }
                
                showRenameIconModal = false;
                ImGui::CloseCurrentPopup();
                
                std::string msg = "Icon renamed";
                if (count > 0) {
                    msg += " (" + std::to_string(count) + " marker";
                    if (count > 1) msg += "s";
                    msg += " updated)";
                }
                ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                ShowToast("Rename failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRenameIconModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderNewProjectModal(App& app, Model& model) {
    ImGui::OpenPopup("New Project");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New Project", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Project name
        ImGui::Text("Project Name:");
        if (ImGui::InputText("##projectname", newProjectConfig.projectName, 
            sizeof(newProjectConfig.projectName))) {
            // Update path when project name changes
            UpdateNewProjectPath();
        }
        
        ImGui::Spacing();
        
        // Save location
        ImGui::Text("Save Location:");
        
        // Display path in a framed box (styled like input field)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, 
                             ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("##savelocationdisplay", 
                         ImVec2(450, 30), 
                         true, 
                         ImGuiWindowFlags_NoScrollbar);
        
        ImGui::PushStyleColor(ImGuiCol_Text, 
                             ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::TextWrapped("%s", newProjectConfig.fullSavePath.c_str());
        ImGui::PopStyleColor();
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Choose Different Location...", 
                         ImVec2(240, 0))) {
            ShowNewProjectFolderPicker();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map Style Selection
        ImGui::Text("Choose your map style:");
        ImGui::Spacing();
        
        // Grid preset cards - side by side
        ImGui::BeginGroup();
        
        // Square preset card
        bool isSquareSelected = (newProjectConfig.gridPreset == GridPreset::Square);
        if (isSquareSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Square\n16×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Square;
        }
        ImGui::PopStyleVar();
        
        if (isSquareSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 20);
        
        // Rectangle preset card
        bool isRectangleSelected = (newProjectConfig.gridPreset == GridPreset::Rectangle);
        if (isRectangleSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Rectangle\n32×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Rectangle;
        }
        ImGui::PopStyleVar();
        
        if (isRectangleSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::EndGroup();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map dimensions with validation
        ImGui::PushItemWidth(150.0f);
        ImGui::InputInt("Map Width (cells)", &newProjectConfig.mapWidth);
        if (newProjectConfig.mapWidth < 16) newProjectConfig.mapWidth = 16;
        if (newProjectConfig.mapWidth > 1024) 
            newProjectConfig.mapWidth = 1024;
        
        ImGui::InputInt("Map Height (cells)", &newProjectConfig.mapHeight);
        if (newProjectConfig.mapHeight < 16) 
            newProjectConfig.mapHeight = 16;
        if (newProjectConfig.mapHeight > 1024) 
            newProjectConfig.mapHeight = 1024;
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        
        // Preview info
        int totalCells = newProjectConfig.mapWidth * 
            newProjectConfig.mapHeight;
        
        // Get cell dimensions from preset
        int cellWidth = (newProjectConfig.gridPreset == GridPreset::Square) ? 16 : 32;
        int cellHeight = 16;
        
        int pixelWidth = newProjectConfig.mapWidth * cellWidth;
        int pixelHeight = newProjectConfig.mapHeight * cellHeight;
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Total cells: %d | Canvas size: %dx%d px",
            totalCells, pixelWidth, pixelHeight);
        
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Validate path
            if (newProjectConfig.fullSavePath.empty()) {
                ShowToast("Please select a save location", 
                         Toast::Type::Error);
            } else {
                // Apply configuration to model
                model.meta.title = std::string(newProjectConfig.projectName);
                
                // Apply grid preset (sets tileWidth/tileHeight automatically)
                model.ApplyGridPreset(newProjectConfig.gridPreset);
                model.grid.cols = newProjectConfig.mapWidth;
                model.grid.rows = newProjectConfig.mapHeight;
                model.grid.locked = false;  // New project starts unlocked
                
                // Initialize other defaults
                model.InitDefaultPalette();
                model.InitDefaultKeymap();
                model.InitDefaultTheme("Dark");
                
                showNewProjectModal = false;
                ImGui::CloseCurrentPopup();
                
                // Create project with specified path
                app.NewProject(newProjectConfig.fullSavePath);
                
                // Transition to editor
                app.ShowEditor();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProjectModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderRecentProjectsList(App& app) {
    // Show max 3 most recent projects side-by-side
    const size_t maxDisplay = std::min(recentProjects.size(), size_t(3));
    if (maxDisplay == 0) {
        return;  // No projects to show
    }
    
    // Card dimensions (16:9 aspect ratio thumbnails)
    const float cardWidth = 260.0f;
    const float cardHeight = 174.0f;
    const float thumbnailHeight = 146.0f;  // 260*9/16 = 146.25
    const float cardSpacing = 16.0f;
    const float titleHeight = 28.0f;
    
    // Load textures for visible projects
    for (size_t i = 0; i < maxDisplay; ++i) {
        LoadThumbnailTexture(recentProjects[i]);
    }
    
    // Center the cards horizontally
    ImVec2 windowSize = ImGui::GetWindowSize();
    float totalWidth = maxDisplay * cardWidth + (maxDisplay - 1) * cardSpacing;
    float startX = (windowSize.x - totalWidth) * 0.5f;
    
    ImGui::SetCursorPosX(startX);
    
    // Render each card
    for (size_t i = 0; i < maxDisplay; ++i) {
        auto& project = recentProjects[i];
        
        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();
        
        // Get current cursor position for the card
        ImVec2 cardPos = ImGui::GetCursorScreenPos();
        
        // Thumbnail image
        if (project.thumbnailTextureId != 0) {
            ImVec2 thumbSize(cardWidth, thumbnailHeight);
            
            // Make thumbnail clickable
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                 ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                 ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
            
            if (ImGui::ImageButton(
                    ("##thumb" + std::to_string(i)).c_str(),
                    (ImTextureID)(intptr_t)project.thumbnailTextureId,
                    thumbSize)) {
                app.OpenProject(project.path);
            app.ShowEditor();
        }
            
            ImGui::PopStyleColor(3);
            
            // Tooltip on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", project.path.c_str());
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Last modified: %s", project.lastModified.c_str());
                ImGui::EndTooltip();
            }
            
            // Draw title overlay at bottom-left of thumbnail
            ImVec2 overlayMin(cardPos.x, cardPos.y + thumbnailHeight - 
                             titleHeight);
            ImVec2 overlayMax(cardPos.x + cardWidth, 
                             cardPos.y + thumbnailHeight);
            
            // Semi-transparent dark background for title
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(overlayMin, overlayMax, 
                IM_COL32(0, 0, 0, 180));
            
            // Title text - use ImGui widget for proper font handling
            ImVec2 textPos(cardPos.x + 10, cardPos.y + thumbnailHeight - 
                          titleHeight + 5);
            ImGui::SetCursorScreenPos(textPos);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                              "%s", project.name.c_str());
        }
        
        ImGui::EndGroup();
        ImGui::PopID();
        
        // Add spacing between cards (but not after last card)
        if (i < maxDisplay - 1) {
            ImGui::SameLine(0.0f, cardSpacing);
        }
    }
}

void UI::RenderProjectTemplates() {
    // This is handled inline in RenderNewProjectModal
    // Could be extracted if we want a separate template browser
}

void UI::RenderProjectBrowserModal(App& app) {
    ImGui::OpenPopup("Recent Projects");
    
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Recent Projects", &showProjectBrowserModal, 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        
        ImGui::Text("All Recent Projects");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "Sorted by last modified");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Search bar
        static char searchFilter[256] = "";
        ImGui::SetNextItemWidth(-1.0f);  // Full width
        if (ImGui::InputTextWithHint("##projectsearch", 
                                     "🔍 Search projects...", 
                                     searchFilter, 
                                     sizeof(searchFilter))) {
            // Filter updates on every keystroke
        }
        
        // Show count of filtered results
        if (searchFilter[0] != '\0') {
            int visibleCount = 0;
            for (const auto& project : recentProjects) {
                std::string lowerName = project.name;
                std::string lowerFilter = searchFilter;
                
                // Convert to lowercase for case-insensitive search
                std::transform(lowerName.begin(), lowerName.end(), 
                              lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), 
                              lowerFilter.begin(), ::tolower);
                
                if (lowerName.find(lowerFilter) != std::string::npos) {
                    visibleCount++;
                }
            }
            
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Showing %d of %zu projects", visibleCount, 
                recentProjects.size());
        }
        
        ImGui::Spacing();
        
        // Scrollable content area
        ImGui::BeginChild("ProjectList", ImVec2(0, -40), true);
        
        // Card dimensions for modal view (smaller than welcome screen)
        const float cardWidth = 250.0f;
        const float cardHeight = 180.0f;
        const float thumbnailHeight = 141.0f;  // 250*9/16
        const float titleHeight = 25.0f;
        const float cardSpacing = 15.0f;
        const int cardsPerRow = 3;
        
        // Load all thumbnails
        for (auto& project : recentProjects) {
            LoadThumbnailTexture(project);
        }
        
        // Render projects in a grid (with search filtering)
        int visibleIndex = 0;
        for (size_t i = 0; i < recentProjects.size(); ++i) {
            auto& project = recentProjects[i];
            
            // Apply search filter
            if (searchFilter[0] != '\0') {
                std::string lowerName = project.name;
                std::string lowerFilter = searchFilter;
                
                // Convert to lowercase for case-insensitive search
                std::transform(lowerName.begin(), lowerName.end(), 
                              lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), 
                              lowerFilter.begin(), ::tolower);
                
                // Skip if doesn't match search
                if (lowerName.find(lowerFilter) == std::string::npos) {
                    continue;
                }
            }
            
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            
            // Get current cursor position for the card
            ImVec2 cardPos = ImGui::GetCursorScreenPos();
            
            // Thumbnail image
            if (project.thumbnailTextureId != 0) {
                ImVec2 thumbSize(cardWidth, thumbnailHeight);
                
                // Make thumbnail clickable
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                     ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                     ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
                
                if (ImGui::ImageButton(
                        ("##thumb" + std::to_string(i)).c_str(),
                        (ImTextureID)(intptr_t)project.thumbnailTextureId,
                        thumbSize)) {
                    showProjectBrowserModal = false;
                    app.OpenProject(project.path);
                    app.ShowEditor();
                }
                
                ImGui::PopStyleColor(3);
                
                // Tooltip on hover
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", project.path.c_str());
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "Last modified: %s", project.lastModified.c_str());
                    ImGui::EndTooltip();
                }
                
                // Draw title overlay at bottom-left of thumbnail
                ImVec2 overlayMin(cardPos.x, cardPos.y + thumbnailHeight - 
                                 titleHeight);
                ImVec2 overlayMax(cardPos.x + cardWidth, 
                                 cardPos.y + thumbnailHeight);
                
                // Semi-transparent dark background for title
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(overlayMin, overlayMax, 
                    IM_COL32(0, 0, 0, 180));
                
                // Title text - use ImGui widget for proper font handling
                ImVec2 textPos(cardPos.x + 8, cardPos.y + thumbnailHeight - 
                              titleHeight + 4);
                ImGui::SetCursorScreenPos(textPos);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                                  "%s", project.name.c_str());
            }
            
            ImGui::EndGroup();
            ImGui::PopID();
            
            // Grid layout: add spacing or new line
            // Use visibleIndex for grid layout (not original index i)
            visibleIndex++;
            if (visibleIndex % cardsPerRow != 0) {
                ImGui::SameLine(0.0f, cardSpacing);
            } else {
                ImGui::Spacing();
            }
        }
        
        // Show "no results" message if search yielded nothing
        if (searchFilter[0] != '\0' && visibleIndex == 0) {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "No projects found matching \"%s\"", searchFilter);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "Try a different search term");
        }
        
        ImGui::EndChild();
        
        // Close button at bottom
        ImGui::Spacing();
        float closeButtonWidth = 120.0f;
        float closeButtonX = (ImGui::GetWindowWidth() - closeButtonWidth) * 0.5f;
        ImGui::SetCursorPosX(closeButtonX);
        
        if (ImGui::Button("Close", ImVec2(closeButtonWidth, 0))) {
            showProjectBrowserModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderWhatsNewPanel() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("What's New in Cartograph", &showWhatsNew)) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), 
            "Version 0.1.0 - Initial Release");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::BulletText("Welcome screen with project templates");
        ImGui::BulletText("Pan/zoom canvas with grid");
        ImGui::BulletText("Room and tile painting tools");
        ImGui::BulletText("Door and marker placement");
        ImGui::BulletText("PNG export with layers");
        ImGui::BulletText("Undo/redo support");
        ImGui::BulletText("Autosave functionality");
        ImGui::BulletText("Theme support (Dark/Light)");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
            "Coming Soon:");
        ImGui::BulletText("Reachability analysis");
        ImGui::BulletText("Minimap panel");
        ImGui::BulletText("Legend generation");
        ImGui::BulletText("SVG icon support");
        ImGui::BulletText("Web build");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showWhatsNew = false;
        }
    }
    ImGui::End();
}

void UI::RenderAutosaveRecoveryModal(App& app, Model& model) {
    ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Always);
    ImGui::OpenPopup("Autosave Recovery");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Autosave Recovery", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Unsaved Work Detected");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "Cartograph detected unsaved work from a previous session. "
            "Would you like to recover it?");
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Note: Recovering will load the autosaved data. You can "
            "manually save it when ready.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        if (ImGui::Button("Recover", ImVec2(buttonWidth, 0))) {
            // Load autosave
            std::string autosavePath = Platform::GetAutosaveDir() + "autosave.json";
            Model recoveredModel;
            if (IOJson::LoadFromFile(autosavePath, recoveredModel)) {
                model = recoveredModel;
                model.MarkDirty();  // Mark as dirty so user must save
                ShowToast("Recovered from autosave", Toast::Type::Success);
                app.ShowEditor();
            } else {
                ShowToast("Failed to load autosave", Toast::Type::Error);
            }
            showAutosaveRecoveryModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        if (ImGui::Button("Discard", ImVec2(buttonWidth, 0))) {
            // Clean up autosave and continue
            std::string autosaveDir = Platform::GetAutosaveDir();
            try {
                std::filesystem::remove(autosaveDir + "autosave.json");
                std::filesystem::remove(autosaveDir + "metadata.json");
            } catch (...) {
                // Ignore errors
            }
            showAutosaveRecoveryModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderQuitConfirmationModal(App& app, Model& model) {
    ImGui::SetNextWindowSize(ImVec2(450, 180), ImGuiCond_Always);
    ImGui::OpenPopup("Unsaved Changes");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Warning: Unsaved Changes");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "You have unsaved changes. Do you want to save your work "
            "before quitting?");
        ImGui::Spacing();
        
        if (app.GetState() != AppState::Editor) {
            ImGui::TextDisabled("Current project has not been saved.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showQuitConfirmationModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            // Quit without saving
            showQuitConfirmationModal = false;
            ImGui::CloseCurrentPopup();
            app.ForceQuit();
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            // Save then quit
            app.SaveProject();
            
            // Check if save was successful (dirty flag should be cleared)
            if (!model.dirty) {
                showQuitConfirmationModal = false;
                ImGui::CloseCurrentPopup();
                app.ForceQuit();
            } else {
                // Save failed or was canceled, stay in modal
                ShowToast("Please save the project before quitting", 
                         Toast::Type::Warning);
            }
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}

void UI::ApplyTemplate(ProjectTemplate tmpl) {
    switch (tmpl) {
        case ProjectTemplate::Small:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 128;
            newProjectConfig.mapHeight = 128;
            break;
            
        case ProjectTemplate::Medium:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Large:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 512;
            newProjectConfig.mapHeight = 512;
            break;
            
        case ProjectTemplate::Metroidvania:
            newProjectConfig.gridPreset = GridPreset::Rectangle;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Custom:
        default:
            // Keep current settings
            break;
    }
}

void UI::LoadRecentProjects() {
    recentProjects.clear();
    
    namespace fs = std::filesystem;
    
    // Get default projects directory
    std::string projectsDir = Platform::GetDefaultProjectsDir();
    
    // Check if directory exists
    if (!fs::exists(projectsDir) || !fs::is_directory(projectsDir)) {
        return;
    }
    
    try {
        // Scan directory for project folders
        std::vector<std::pair<fs::file_time_type, fs::path>> projectPaths;
        
        for (const auto& entry : fs::directory_iterator(projectsDir)) {
            if (!entry.is_directory()) continue;
            
            // Check if it has a project.json file
            fs::path projectJson = entry.path() / "project.json";
            if (!fs::exists(projectJson)) continue;
            
            // Get last modified time
            auto modTime = fs::last_write_time(entry.path());
            projectPaths.push_back({modTime, entry.path()});
        }
        
        // Sort by last modified time (newest first)
        std::sort(projectPaths.begin(), projectPaths.end(),
            [](const auto& a, const auto& b) {
                return a.first > b.first;
            });
        
        // Convert to RecentProject entries (limit to 10)
        size_t count = std::min(projectPaths.size(), size_t(10));
        for (size_t i = 0; i < count; ++i) {
            const auto& [modTime, path] = projectPaths[i];
            
            RecentProject project;
            project.path = path.string();
            project.name = path.filename().string();
            
            // Format last modified time
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                modTime - fs::file_time_type::clock::now() + 
                std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm = *std::localtime(&time_t);
            char timeStr[64];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tm);
            project.lastModified = timeStr;
            
            // Set thumbnail path if preview.png exists
            fs::path previewPath = path / "preview.png";
            if (fs::exists(previewPath)) {
                project.thumbnailPath = previewPath.string();
            }
            
            recentProjects.push_back(project);
        }
        
    } catch (const std::exception& e) {
        // Failed to scan directory - clear list and continue
        recentProjects.clear();
    }
}

void UI::AddRecentProject(const std::string& path) {
    // Note: This function no longer saves to JSON.
    // Recent projects are now loaded dynamically by scanning the directory.
    // We keep this function as a no-op for now in case it's called elsewhere,
    // but it doesn't need to do anything since LoadRecentProjects() handles
    // everything.
    
    // The directory scan approach means:
    // - No file I/O failures
    // - Always shows current state
    // - Self-healing (deleted projects auto-remove)
    // - Simpler and more reliable
}

void UI::LoadThumbnailTexture(RecentProject& project) {
    // Skip if already loaded
    if (project.thumbnailLoaded) {
        return;
    }
    
    namespace fs = std::filesystem;
    
    // Check if thumbnail path exists
    if (project.thumbnailPath.empty() || 
        !fs::exists(project.thumbnailPath)) {
        // Use placeholder
        if (placeholderTexture == 0) {
            placeholderTexture = GeneratePlaceholderTexture();
        }
        project.thumbnailTextureId = placeholderTexture;
        project.thumbnailLoaded = true;
        return;
    }
    
    // Load image from file
    int width, height, channels;
    unsigned char* data = stbi_load(
        project.thumbnailPath.c_str(),
        &width, &height, &channels, 4  // Force RGBA
    );
    
    if (!data) {
        // Failed to load - use placeholder
        if (placeholderTexture == 0) {
            placeholderTexture = GeneratePlaceholderTexture();
        }
        project.thumbnailTextureId = placeholderTexture;
        project.thumbnailLoaded = true;
        return;
    }
    
    // Create OpenGL texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );
    
    // Unbind and cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    
    project.thumbnailTextureId = texId;
    project.thumbnailLoaded = true;
}

void UI::UnloadThumbnailTextures() {
    // Unload all thumbnail textures
    for (auto& project : recentProjects) {
        if (project.thumbnailLoaded && 
            project.thumbnailTextureId != 0 &&
            project.thumbnailTextureId != placeholderTexture) {
            GLuint texId = project.thumbnailTextureId;
            glDeleteTextures(1, &texId);
            project.thumbnailTextureId = 0;
            project.thumbnailLoaded = false;
        }
    }
    
    // Delete placeholder texture
    if (placeholderTexture != 0) {
        GLuint texId = placeholderTexture;
        glDeleteTextures(1, &texId);
        placeholderTexture = 0;
    }
}

unsigned int UI::GeneratePlaceholderTexture() {
    // Generate a gradient/grid pattern for missing thumbnails
    const int width = 384;
    const int height = 216;
    const int gridSize = 16;  // Grid cell size in pixels
    
    std::vector<uint8_t> pixels(width * height * 4);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            
            // Create a subtle grid pattern
            bool onGridLine = (x % gridSize == 0) || (y % gridSize == 0);
            
            // Gradient from dark gray to darker gray
            float gradientFactor = 1.0f - (y / static_cast<float>(height)) * 0.3f;
            uint8_t baseColor = static_cast<uint8_t>(40 * gradientFactor);
            uint8_t gridColor = static_cast<uint8_t>(60 * gradientFactor);
            
            if (onGridLine) {
                pixels[idx + 0] = gridColor;      // R
                pixels[idx + 1] = gridColor;      // G
                pixels[idx + 2] = gridColor;      // B
            } else {
                pixels[idx + 0] = baseColor;      // R
                pixels[idx + 1] = baseColor;      // G
                pixels[idx + 2] = baseColor;      // B
            }
            pixels[idx + 3] = 255;                // A
        }
    }
    
    // Create OpenGL texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );
    
    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texId;
}

void UI::UpdateNewProjectPath() {
    // Sanitize project name for filesystem
    std::string sanitized = newProjectConfig.projectName;
    
    // Replace invalid characters with underscores
    for (char& c : sanitized) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    
    // Build full path
    if (!newProjectConfig.saveDirectory.empty()) {
        // Ensure directory path ends with separator
        std::string dir = newProjectConfig.saveDirectory;
        if (dir.back() != '/' && dir.back() != '\\') {
#ifdef _WIN32
            dir += '\\';
#else
            dir += '/';
#endif
        }
        
        newProjectConfig.fullSavePath = dir + sanitized + 
#ifdef _WIN32
            "\\";
#else
            "/";
#endif
    }
}

void UI::ShowNewProjectFolderPicker() {
    // Callback struct to capture UI reference
    struct CallbackData {
        UI* ui;
    };
    
    // Allocate callback data on heap (freed in callback)
    CallbackData* data = new CallbackData{this};
    
    // Show native folder picker dialog
    SDL_ShowOpenFolderDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            CallbackData* data = static_cast<CallbackData*>(userdata);
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open folder dialog", 
                                   Toast::Type::Error);
                delete data;
                return;
            }
            
            if (filelist[0] == nullptr) {
                // User canceled - keep existing path
                delete data;
                return;
            }
            
            // User selected a folder
            std::string folderPath = filelist[0];
            
            // Update save directory and regenerate full path
            data->ui->newProjectConfig.saveDirectory = folderPath;
            data->ui->UpdateNewProjectPath();
            
            delete data;
        },
        // Userdata
        data,
        // Window (NULL for now)
        nullptr,
        // Default location - start with current directory
        newProjectConfig.saveDirectory.empty() 
            ? nullptr 
            : newProjectConfig.saveDirectory.c_str(),
        // Allow multiple folders
        false
    );
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
    
    // Allocate callback data on heap (freed in callback)
    CallbackData* data = new CallbackData{this, &iconManager, &jobs};
    
    // Show native file dialog
    SDL_ShowOpenFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            CallbackData* data = static_cast<CallbackData*>(userdata);
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open file dialog", 
                                   Toast::Type::Error);
                delete data;
                return;
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                delete data;
                return;
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
            
            // Enqueue processing job
            data->jobs->Enqueue(
                JobType::ProcessIcon,
                [path, iconName, data]() {
                    std::vector<uint8_t> pixels;
                    int width, height;
                    std::string errorMsg;
                    
                    if (!IconManager::ProcessIconFromFile(path, pixels, 
                                                          width, height, errorMsg)) {
                        throw std::runtime_error(errorMsg);
                    }
                    
                    if (!data->iconManager->AddIconFromMemory(
                            iconName, pixels.data(), width, height, "marker")) {
                        throw std::runtime_error("Failed to add icon to memory");
                    }
                },
                [data, iconName](bool success, const std::string& error) {
                    data->ui->isImportingIcon = false;
                    
                    if (success) {
                        // Build atlas on main thread
                        data->iconManager->BuildAtlas();
                        
                        data->ui->ShowToast("Icon imported: " + iconName, 
                                          Toast::Type::Success, 2.0f);
                        data->ui->selectedIconName = iconName;
                    } else {
                        data->ui->ShowToast("Failed to import: " + error, 
                                          Toast::Type::Error, 3.0f);
                    }
                    
                    delete data;
                }
            );
        },
        // Userdata
        data,
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
    
    // Allocate callback data on heap (freed in callback)
    CallbackData* data = new CallbackData{this, &app};
    
    // Show native folder picker dialog
    SDL_ShowOpenFolderDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            CallbackData* data = static_cast<CallbackData*>(userdata);
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open folder dialog", 
                                   Toast::Type::Error);
                delete data;
                return;
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                delete data;
                return;
            }
            
            // User selected a folder
            std::string folderPath = filelist[0];
            
            // Save project to folder
            data->app->SaveProjectFolder(folderPath);
            
            delete data;
        },
        // Userdata
        data,
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
    
    // Allocate callback data on heap (freed in callback)
    CallbackData* data = new CallbackData{this, &app};
    
    // Setup filter for .cart files
    static SDL_DialogFileFilter filter = { 
        "Cartograph Package", "cart" 
    };
    
    // Show native save file dialog
    SDL_ShowSaveFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filterIndex) {
            CallbackData* data = static_cast<CallbackData*>(userdata);
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open save dialog", 
                                   Toast::Type::Error);
                delete data;
                return;
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                delete data;
                return;
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
            
            delete data;
        },
        // Userdata
        data,
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
    
    // Allocate callback data on heap (freed in callback)
    CallbackData* data = new CallbackData{this, &app};
    
    // Setup filters for PNG files
    static SDL_DialogFileFilter filters[] = {
        { "PNG Image", "png" },
        { "All Files", "*" }
    };
    
    // Show native save file dialog
    SDL_ShowSaveFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filterIndex) {
            CallbackData* data = static_cast<CallbackData*>(userdata);
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open save dialog", 
                                   Toast::Type::Error);
                delete data;
                return;
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                delete data;
                return;
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
                delete data;
                return;
            }
            
            // Export PNG
            data->app->ExportPng(path);
            
            delete data;
        },
        // Userdata
        data,
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

void UI::HandleDroppedFile(const std::string& filePath) {
    droppedFilePath = filePath;
    hasDroppedFile = true;
}

} // namespace Cartograph

