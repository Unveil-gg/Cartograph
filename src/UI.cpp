#include "UI.h"
#include "App.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "Jobs.h"
#include "render/Renderer.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <set>
#include <filesystem>

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
    IRenderer& renderer,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs,
    float deltaTime
) {
    // Create fullscreen dockspace window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_MenuBar |
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
    
    // Render menu bar
    RenderMenuBar(model, canvas, history, icons, jobs);
    
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
    RenderToolsPanel(model);
    RenderCanvasPanel(renderer, model, canvas, history, icons);
    RenderPropertiesPanel(model, icons);
    RenderStatusBar(model, canvas);
    
    // Render toasts
    RenderToasts(deltaTime);
    
    // Render modal dialogs
    if (showExportModal) {
        RenderExportModal(model, canvas);
    }
    if (showSettingsModal) {
        RenderSettingsModal(model);
    }
}

void UI::ShowToast(
    const std::string& message, 
    Toast::Type type,
    float duration
) {
    Toast toast;
    toast.message = message;
    toast.type = type;
    toast.remainingTime = duration;
    m_toasts.push_back(toast);
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
    float scrollStartY = (windowSize.y - contentHeight) * 0.5f;
    
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
    
    // Add extra spacing between ASCII art and buttons
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Main action buttons - side by side, centered
    float buttonWidth = 240.0f;
    float buttonHeight = 60.0f;
    float buttonSpacing = 20.0f;
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
        float sectionWidth = 600.0f;
        float sectionStartX = (windowSize.x - sectionWidth) * 0.5f;
        
        ImGui::SetCursorPosX(sectionStartX);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(sectionStartX);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), 
            "Recent Projects");
        ImGui::Spacing();
        
        RenderRecentProjectsList(app);
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
    
    if (showWhatsNew) {
        RenderWhatsNewPanel();
    }
    
    // Render toasts
    RenderToasts(0.016f);
}

void UI::RenderMenuBar(
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Cmd+N")) {
                // TODO: New project
            }
            if (ImGui::MenuItem("Open...", "Cmd+O")) {
                // TODO: Open project
            }
            if (ImGui::MenuItem("Save", "Cmd+S")) {
                // TODO: Save project
            }
            if (ImGui::MenuItem("Save As...")) {
                // TODO: Save As
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export PNG...", "Cmd+E")) {
                showExportModal = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Cmd+Q")) {
                // TODO: Quit
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
        
        ImGui::EndMenuBar();
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

void UI::RenderToolsPanel(Model& model) {
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
    
    const char* toolShortcuts[] = {
        "V", "S", "B", "", "F", "", "I"
    };
    
    for (int i = 0; i < 7; ++i) {
        bool selected = (static_cast<int>(currentTool) == i);
        
        // Show tool name with optional shortcut
        char toolLabel[64];
        if (toolShortcuts[i][0] != '\0') {
            snprintf(toolLabel, sizeof(toolLabel), "%s [%s]", 
                     toolNames[i], toolShortcuts[i]);
        } else {
            snprintf(toolLabel, sizeof(toolLabel), "%s", toolNames[i]);
        }
        
        if (ImGui::Selectable(toolLabel, selected)) {
            currentTool = static_cast<Tool>(i);
        }
        
        // Add tooltip with more info
        if (ImGui::IsItemHovered()) {
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
                case 3: // Erase
                    ImGui::Text("Erase Tool [E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to erase tiles");
                    break;
                case 8: // Eyedropper
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
    
    // TODO: Add icons palette section here
    // TODO: Add layers toggles here
    
    ImGui::End();
}

void UI::RenderPropertiesPanel(Model& model, IconManager& icons) {
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
    
    // Marker properties
    if (ImGui::CollapsingHeader("Markers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Marker Tool Settings");
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
        ImGui::Text("Selected Icon: %s", selectedIconName.c_str());
        
        // Icon picker
        if (ImGui::BeginChild("IconPicker", ImVec2(0, 150), true)) {
            ImGui::Text("Available Icons (%zu)", icons.GetIconCount());
            ImGui::Separator();
            
            // Default icon if no icons loaded
            if (icons.GetIconCount() == 0) {
                ImGui::TextDisabled("No icons loaded");
                
                bool isSelected = (selectedIconName == "dot");
                if (ImGui::Selectable("dot (fallback)", isSelected)) {
                    selectedIconName = "dot";
                }
            } else {
                // Show all loaded icons
                // Built-in icons first
                const char* builtInIcons[] = {
                    "dot", "bench", "chest", "skull"
                };
                
                ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), 
                                  "Built-in:");
                
                for (const char* iconName : builtInIcons) {
                    const Icon* icon = icons.GetIcon(iconName);
                    if (icon) {
                        bool isSelected = (selectedIconName == iconName);
                        
                        if (ImGui::Selectable(iconName, isSelected)) {
                            selectedIconName = iconName;
                            
                            // Update selected marker if editing
                            if (selectedMarker) {
                                selectedMarker->icon = selectedIconName;
                                model.MarkDirty();
                            }
                        }
                    }
                }
                
                // Imported icons
                // (Would need IconManager::GetAllIconNames() to list them)
                // For now, just show that more icons can be imported
                if (isImportingIcon) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Importing: %s...", 
                                       importingIconName.c_str());
                }
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
        } else {
            ImGui::TextDisabled("Click to place new marker");
        }
    }
    
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
    IconManager& icons
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
    
    // Global keyboard shortcuts for tool switching (work even when not hovering)
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_V)) {
            currentTool = Tool::Move;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S) && 
            !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper) {
            currentTool = Tool::Select;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_B)) {
            currentTool = Tool::Paint;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E) && 
            !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper) {
            currentTool = Tool::Erase;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            currentTool = Tool::Fill;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_I)) {
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
                } else {
                    ShowToast("No tile to pick", Toast::Type::Info, 1.5f);
                }
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
                    ShowToast("Marker deleted", Toast::Type::Info);
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
                    
                    ShowToast("Marker placed", Toast::Type::Success, 1.5f);
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
                    
                    ShowToast("Marker deleted", Toast::Type::Info);
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
                        
                        ShowToast("Marker moved", Toast::Type::Success, 1.5f);
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
        // Copy selected marker (Cmd/Ctrl+C)
        bool cmdC = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || 
                   ImGui::IsKeyDown(ImGuiKey_RightCtrl);
#ifdef __APPLE__
        cmdC = cmdC || ImGui::IsKeyDown(ImGuiKey_LeftSuper) || 
               ImGui::IsKeyDown(ImGuiKey_RightSuper);
#endif
        
        if (cmdC && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            if (selectedMarker) {
                copiedMarkers.clear();
                copiedMarkers.push_back(*selectedMarker);
                ShowToast("Marker copied", Toast::Type::Info, 1.5f);
            }
        }
        
        // Paste marker (Cmd/Ctrl+V)
        bool cmdV = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || 
                   ImGui::IsKeyDown(ImGuiKey_RightCtrl);
#ifdef __APPLE__
        cmdV = cmdV || ImGui::IsKeyDown(ImGuiKey_LeftSuper) || 
               ImGui::IsKeyDown(ImGuiKey_RightSuper);
#endif
        
        if (cmdV && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
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
                
                ShowToast("Marker pasted", Toast::Type::Success, 1.5f);
            }
        }
        
        // Delete selected marker (Delete/Backspace key)
        if (selectedMarker && 
            (ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
             ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
            auto cmd = std::make_unique<DeleteMarkerCommand>(
                selectedMarker->id
            );
            history.AddCommand(std::move(cmd), model);
            
            selectedMarker = nullptr;
            ShowToast("Marker deleted", Toast::Type::Info);
        }
    }
    
    // Draw canvas overlays using ImGui foreground DrawList
    // (foreground ensures previews are drawn on top of tiles)
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    
    // Draw background
    ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    drawList->AddRectFilled(canvasPos, 
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
        bgColor);
    
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
        
        // Left section: Current tool
        const char* toolName = "Unknown";
        const char* toolHint = "";
        switch (currentTool) {
            case Tool::Move:
                toolName = "Move Tool";
                toolHint = "Drag to pan | Wheel to zoom";
                break;
            case Tool::Select:
                toolName = "Select Tool";
                toolHint = "Drag to select region";
                break;
            case Tool::Paint:
                if (roomPaintMode) {
                    toolName = "Paint Tool (ROOM MODE)";
                    toolHint = "Click to assign cells | Right-click to unassign";
                } else {
                    toolName = "Paint Tool";
                    toolHint = "Paint tiles | Hover edges: W=Wall D=Door Click=Cycle";
                }
                break;
            case Tool::Erase:
                toolName = "Erase Tool";
                toolHint = "Click to erase tiles";
                break;
            case Tool::Fill:
                toolName = "Fill Tool";
                toolHint = "Click to fill area";
                break;
            case Tool::Marker:
                toolName = "Marker Tool";
                toolHint = "Click to place marker";
                break;
            case Tool::Eyedropper:
                toolName = "Eyedropper Tool";
                toolHint = "Click to pick tile color";
                break;
        }
        
        if (roomPaintMode) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "%s", toolName);
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", toolName);
        }
        ImGui::SameLine(0, 10);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "| %s", 
                          toolHint);
        
        // Middle section: Zoom
        ImGui::SameLine(0, 20);
        ImGui::Text("| Zoom: %.0f%%", canvas.zoom * 100.0f);
        
        // Modified indicator
        if (model.dirty) {
            ImGui::SameLine(0, 20);
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                "| ⚠ Modified");
        }
        
        // Right section: Tool shortcuts
        ImGui::SameLine(0, 20);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
            "| Paint: B | Edges: W=Wall D=Door | V=Move F=Fill E=Erase I=Eyedropper");
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
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Export PNG", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Export Options");
        ImGui::Separator();
        
        ImGui::SliderInt("Scale", &exportOptions.scale, 1, 4);
        ImGui::Checkbox("Transparency", &exportOptions.transparency);
        
        if (!exportOptions.transparency) {
            ImGui::ColorEdit3("Background", &exportOptions.bgColorR);
        }
        
        ImGui::Separator();
        ImGui::Text("Layers");
        ImGui::Checkbox("Grid", &exportOptions.layerGrid);
        ImGui::Checkbox("Room Outlines", &exportOptions.layerRoomOutlines);
        ImGui::Checkbox("Tiles", &exportOptions.layerTiles);
        ImGui::Checkbox("Doors", &exportOptions.layerDoors);
        ImGui::Checkbox("Markers", &exportOptions.layerMarkers);
        
        ImGui::Separator();
        
        if (ImGui::Button("Export", ImVec2(120, 0))) {
            // TODO: Trigger export
            showExportModal = false;
            ImGui::CloseCurrentPopup();
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
    
    if (ImGui::BeginPopupModal("Settings", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Project Settings");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Project metadata
        ImGui::Text("Project Information:");
        
        char titleBuf[256];
        strncpy(titleBuf, model.meta.title.c_str(), sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            model.meta.title = titleBuf;
            model.MarkDirty();
        }
        
        char authorBuf[256];
        strncpy(authorBuf, model.meta.author.c_str(), sizeof(authorBuf) - 1);
        authorBuf[sizeof(authorBuf) - 1] = '\0';
        if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
            model.meta.author = authorBuf;
            model.MarkDirty();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Grid Cell Type
        ImGui::Text("Grid Cell Type:");
        
        bool canChangePreset = model.CanChangeGridPreset();
        
        if (!canChangePreset) {
            ImGui::BeginDisabled();
        }
        
        bool isSquare = (model.grid.preset == GridPreset::Square);
        if (ImGui::RadioButton("Square (16×16)", isSquare)) {
            if (canChangePreset) {
                model.ApplyGridPreset(GridPreset::Square);
            } else {
                ShowToast("Cannot change cell type - delete all markers first",
                    Toast::Type::Warning, 3.0f);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("?##square_settings")) {
            ShowToast("Square cells for top-down games. "
                     "Markers snap to center only.",
                Toast::Type::Info, 4.0f);
        }
        
        bool isRectangle = (model.grid.preset == GridPreset::Rectangle);
        if (ImGui::RadioButton("Rectangle (32×16)", isRectangle)) {
            if (canChangePreset) {
                model.ApplyGridPreset(GridPreset::Rectangle);
            } else {
                ShowToast("Cannot change cell type - delete all markers first",
                    Toast::Type::Warning, 3.0f);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("?##rectangle_settings")) {
            ShowToast("Rectangular cells for side-scrollers. "
                     "Markers snap to left/right positions.",
                Toast::Type::Info, 4.0f);
        }
        
        if (!canChangePreset) {
            ImGui::EndDisabled();
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f),
                "🔒 Locked (%zu marker%s placed)",
                model.markers.size(),
                model.markers.size() == 1 ? "" : "s");
            ImGui::SameLine();
            if (ImGui::Button("?##locked_settings")) {
                ShowToast("Delete all markers to change cell type",
                    Toast::Type::Info, 3.0f);
            }
        }
        
        // Show current dimensions (read-only info)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Cell Dimensions: %d×%d px",
            model.grid.tileWidth, model.grid.tileHeight);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Grid Dimensions
        ImGui::Text("Grid Dimensions:");
        
        int tempCols = model.grid.cols;
        int tempRows = model.grid.rows;
        
        ImGui::InputInt("Grid Width (cells)", &tempCols);
        if (tempCols < 16) tempCols = 16;
        if (tempCols > 2048) tempCols = 2048;
        model.grid.cols = tempCols;
        
        ImGui::InputInt("Grid Height (cells)", &tempRows);
        if (tempRows < 16) tempRows = 16;
        if (tempRows > 2048) tempRows = 2048;
        model.grid.rows = tempRows;
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Preview info
        int totalCells = model.grid.cols * model.grid.rows;
        int pixelWidth = model.grid.cols * model.grid.tileWidth;
        int pixelHeight = model.grid.rows * model.grid.tileHeight;
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Total cells: %d | Canvas size: %dx%d px",
            totalCells, pixelWidth, pixelHeight);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Edge Configuration
        ImGui::Text("Edge/Wall Configuration:");
        
        ImGui::Checkbox("Auto-expand grid", &model.grid.autoExpandGrid);
        ImGui::SameLine();
        if (ImGui::Button("?##autoexpand_settings")) {
            ShowToast("Automatically expand grid when placing edges "
                     "near boundaries", Toast::Type::Info, 4.0f);
        }
        
        ImGui::SliderInt("Expansion threshold (cells)", 
                        &model.grid.expansionThreshold, 1, 20);
        ImGui::SameLine();
        if (ImGui::Button("?##threshold_settings")) {
            ShowToast("Distance from grid boundary to trigger expansion", 
                     Toast::Type::Info, 4.0f);
        }
        
        ImGui::SliderFloat("Expansion factor", &model.grid.expansionFactor, 
                          1.1f, 3.0f, "%.1fx");
        ImGui::SameLine();
        if (ImGui::Button("?##factor_settings")) {
            ShowToast("Grid growth multiplier (e.g., 1.5x = 50% growth)", 
                     Toast::Type::Info, 4.0f);
        }
        
        ImGui::SliderFloat("Edge hover threshold", 
                          &model.grid.edgeHoverThreshold, 
                          0.1f, 0.5f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("?##hover_settings")) {
            ShowToast("Distance from cell edge to activate edge mode "
                     "(0.2 = 20% of cell size)", Toast::Type::Info, 4.0f);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Keybindings section
        ImGui::Text("Keybindings:");
        ImGui::Spacing();
        
        if (ImGui::BeginTable("KeybindingsTable", 2, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 
                                   150.0f);
            ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            
            // Display key tool bindings
            const char* actions[] = {
                "Paint (primary)", "Erase (primary)", "Erase (alternate)",
                "Tool: Move", "Tool: Paint", "Tool: Erase", "Tool: Eyedropper"
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
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), 
                                      "%s", it->second.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
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
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons
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

void UI::RenderNewProjectModal(App& app, Model& model) {
    ImGui::OpenPopup("New Project");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New Project", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Create a New Map Project");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Project name
        ImGui::Text("Project Name:");
        ImGui::InputText("##projectname", newProjectConfig.projectName, 
            sizeof(newProjectConfig.projectName));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Cell Type Selection
        ImGui::Text("Choose your map style:");
        ImGui::Spacing();
        
        // Grid preset cards - side by side
        ImGui::BeginGroup();
        
        // Square preset card
        bool isSquareSelected = (newProjectConfig.gridPreset == GridPreset::Square);
        if (isSquareSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 20));
        if (ImGui::Button("   Square   \n\n    ┌───┐    \n    │ ● │    \n    └───┘    \n\n   16×16    \n\n Top-down  \n  dungeon   ", 
                         ImVec2(140, 180))) {
            newProjectConfig.gridPreset = GridPreset::Square;
        }
        ImGui::PopStyleVar();
        
        if (isSquareSelected) {
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 20);
        
        // Rectangle preset card
        bool isRectangleSelected = (newProjectConfig.gridPreset == GridPreset::Rectangle);
        if (isRectangleSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 20));
        if (ImGui::Button(" Rectangle \n\n ┌──────┐  \n │ ●  ● │  \n └──────┘  \n\n   32×16   \n\n Side-view \nplatformer ", 
                         ImVec2(140, 180))) {
            newProjectConfig.gridPreset = GridPreset::Rectangle;
        }
        ImGui::PopStyleVar();
        
        if (isRectangleSelected) {
            ImGui::PopStyleColor(2);
        }
        
        ImGui::EndGroup();
        
        ImGui::Spacing();
        
        // Show cell dimensions info (read-only)
        const char* presetName = (newProjectConfig.gridPreset == GridPreset::Square) 
            ? "Square (16×16 px)" 
            : "Rectangle (32×16 px)";
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Cell Type: %s", presetName);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map dimensions with validation
        ImGui::InputInt("Map Width (cells)", &newProjectConfig.mapWidth);
        if (newProjectConfig.mapWidth < 16) newProjectConfig.mapWidth = 16;
        if (newProjectConfig.mapWidth > 1024) 
            newProjectConfig.mapWidth = 1024;
        
        ImGui::InputInt("Map Height (cells)", &newProjectConfig.mapHeight);
        if (newProjectConfig.mapHeight < 16) 
            newProjectConfig.mapHeight = 16;
        if (newProjectConfig.mapHeight > 1024) 
            newProjectConfig.mapHeight = 1024;
        
        ImGui::Spacing();
        ImGui::Separator();
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
            
            // Transition to editor
            app.ShowEditor();
            
            ShowToast("Project created: " + model.meta.title, 
                Toast::Type::Success);
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
    // TODO: Implement recent projects list with thumbnails
    // This would display a scrollable list of recently opened projects
    // with small preview thumbnails and last modified dates
    
    ImGui::BeginChild("RecentProjects", ImVec2(0, 200), true);
    
    for (size_t i = 0; i < recentProjects.size() && i < 5; ++i) {
        const auto& recent = recentProjects[i];
        
        ImGui::PushID(static_cast<int>(i));
        
        // TODO: Add thumbnail preview here
        ImGui::Text("[Thumbnail]");
        ImGui::SameLine();
        
        ImGui::BeginGroup();
        if (ImGui::Selectable(recent.name.c_str(), false, 
            0, ImVec2(600, 0))) {
            app.OpenProject(recent.path);
            app.ShowEditor();
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "Last modified: %s", recent.lastModified.c_str());
        ImGui::EndGroup();
        
        ImGui::PopID();
        ImGui::Separator();
    }
    
    ImGui::EndChild();
}

void UI::RenderProjectTemplates() {
    // This is handled inline in RenderNewProjectModal
    // Could be extracted if we want a separate template browser
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
    // TODO: Load recent projects from persistent storage
    // This would read from a config file (JSON) in the app data directory
    // containing paths, names, and last modified dates
    
    recentProjects.clear();
    
    std::string configDir = Platform::GetUserDataDir();
    std::string recentPath = configDir + "recent_projects.json";
    
    // Stub: For now, just clear the list
    // In a full implementation:
    // 1. Read JSON file from recentPath
    // 2. Parse project entries
    // 3. Validate that files still exist
    // 4. Load thumbnails if available
    // 5. Populate recentProjects vector
}

void UI::AddRecentProject(const std::string& path) {
    // TODO: Add project to recent list and save to persistent storage
    // This would:
    // 1. Create a RecentProject entry
    // 2. Add to front of recentProjects list
    // 3. Remove duplicates
    // 4. Limit to max 10 entries
    // 5. Save updated list to JSON file
    // 6. Optionally generate/save thumbnail
    
    // Stub implementation
    (void)path; // Suppress unused warning
}

void UI::BuildFixedLayout(ImGuiID dockspaceId) {
    // Clear any existing layout
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, 
        ImGuiDockNodeFlags_DockSpace);
    
    // Set dockspace size to fill the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);
    
    // Split the dockspace into fixed regions
    // Layout structure:
    //   TopRest
    //   ├─ Left (300px)
    //   └─ RightRest
    //      ├─ Center (remaining)
    //      └─ Right (360px)
    //   Bottom (28px - thin status bar)
    
    // Split bottom: 28px from bottom for thin status bar
    ImGuiID bottomId = 0;
    ImGuiID topRestId = 0;
    ImGui::DockBuilderSplitNode(
        dockspaceId, ImGuiDir_Down, 28.0f / viewport->WorkSize.y, 
        &bottomId, &topRestId
    );
    
    // Split left: 300px from left
    ImGuiID leftId = 0;
    ImGuiID rightRestId = 0;
    ImGui::DockBuilderSplitNode(
        topRestId, ImGuiDir_Left, 300.0f / viewport->WorkSize.x, 
        &leftId, &rightRestId
    );
    
    // Split right: 360px from right
    ImGuiID rightId = 0;
    ImGuiID centerId = 0;
    float rightWidth = 360.0f / (viewport->WorkSize.x - 300.0f);
    ImGui::DockBuilderSplitNode(
        rightRestId, ImGuiDir_Right, rightWidth,
        &rightId, &centerId
    );
    
    // Dock windows into their respective nodes
    ImGui::DockBuilderDockWindow("Cartograph/Tools", leftId);
    ImGui::DockBuilderDockWindow("Cartograph/Canvas", centerId);
    ImGui::DockBuilderDockWindow("Cartograph/Inspector", rightId);
    ImGui::DockBuilderDockWindow("Cartograph/Console", bottomId);
    
    // Configure node flags to prevent user modifications
    // Note: These flags limit what users can do with the docked windows
    ImGuiDockNode* leftNode = ImGui::DockBuilderGetNode(leftId);
    ImGuiDockNode* centerNode = ImGui::DockBuilderGetNode(centerId);
    ImGuiDockNode* rightNode = ImGui::DockBuilderGetNode(rightId);
    ImGuiDockNode* bottomNode = ImGui::DockBuilderGetNode(bottomId);
    
    if (leftNode) {
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
    }
    
    if (centerNode) {
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
    }
    
    if (rightNode) {
        rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        rightNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        rightNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
    }
    
    if (bottomNode) {
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
    }
    
    // Finish building
    ImGui::DockBuilderFinish(dockspaceId);
}

void UI::ImportIcon(IconManager& iconManager, JobQueue& jobs) {
    // TODO: Use native file dialog when available
    // For now, show a simple ImGui input dialog
    
    static char pathBuffer[512] = "";
    static bool showInputDialog = false;
    
    if (!showInputDialog) {
        showInputDialog = true;
        pathBuffer[0] = '\0';
    }
    
    // Show simple input dialog
    ImGui::OpenPopup("Import Icon");
    if (ImGui::BeginPopupModal("Import Icon", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter path to PNG icon:");
        ImGui::InputText("##iconpath", pathBuffer, sizeof(pathBuffer));
        
        ImGui::Separator();
        
        if (ImGui::Button("Import", ImVec2(120, 0))) {
            std::string path = pathBuffer;
            
            if (!path.empty()) {
                // Extract base name from path
                namespace fs = std::filesystem;
                std::string baseName = fs::path(path).stem().string();
                
                // Generate unique name
                std::string iconName = iconManager.GenerateUniqueName(baseName);
                
                // Set loading state
                isImportingIcon = true;
                importingIconName = iconName;
                
                // Enqueue processing job
                jobs.Enqueue(
                    JobType::ProcessIcon,
                    // Work function (runs on worker thread)
                    [path, iconName]() {
                        // Process icon in background
                        std::vector<uint8_t> pixels;
                        int width, height;
                        std::string errorMsg;
                        
                        bool success = IconManager::ProcessIconFromFile(
                            path, pixels, width, height, errorMsg
                        );
                        
                        // Store result in thread-local storage for callback
                        // (This is a simplified approach - in production,
                        //  use a proper result structure)
                    },
                    // Callback (runs on main thread)
                    [this, &iconManager, iconName, path](
                        bool success, const std::string& error
                    ) {
                        isImportingIcon = false;
                        
                        if (success) {
                            // Process the icon data
                            // Note: We need to re-process on main thread
                            // because we can't easily pass data between threads
                            std::vector<uint8_t> pixels;
                            int width, height;
                            std::string errorMsg;
                            
                            if (IconManager::ProcessIconFromFile(
                                    path, pixels, width, height, errorMsg)) {
                                // Add to icon manager
                                if (iconManager.AddIconFromMemory(
                                        iconName, pixels.data(),
                                        width, height)) {
                                    // Rebuild atlas with new icon
                                    iconManager.BuildAtlas();
                                    
                                    ShowToast(
                                        "Icon imported: " + iconName,
                                        Toast::Type::Success
                                    );
                                } else {
                                    ShowToast(
                                        "Failed to add icon to atlas",
                                        Toast::Type::Error
                                    );
                                }
                            } else {
                                ShowToast(
                                    "Failed to process icon: " + errorMsg,
                                    Toast::Type::Error
                                );
                            }
                        } else {
                            ShowToast(
                                "Failed to import icon: " + error,
                                Toast::Type::Error
                            );
                        }
                    }
                );
                
                ImGui::CloseCurrentPopup();
                showInputDialog = false;
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            showInputDialog = false;
        }
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph

