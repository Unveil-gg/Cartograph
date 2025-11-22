#include "UI.h"
#include "App.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "render/Renderer.h"
#include "platform/Paths.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Cartograph {

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
    RenderMenuBar(model, canvas, history);
    
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
    RenderCanvasPanel(renderer, model, canvas, history);
    RenderPropertiesPanel(model);
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
        newProjectConfig.cellWidth = 16;
        newProjectConfig.cellHeight = 16;
        newProjectConfig.mapWidth = 256;
        newProjectConfig.mapHeight = 256;
        selectedTemplate = ProjectTemplate::Medium;
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

void UI::RenderMenuBar(Model& model, Canvas& canvas, History& history) {
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
                canvas.SetZoom(1.0f);
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
        "Move", "Select", "Paint", "Erase", "Fill", "Rectangle", "Door", 
        "Marker", "Eyedropper"
    };
    
    const char* toolShortcuts[] = {
        "V", "S", "B", "", "", "", "", "", "I"
    };
    
    for (int i = 0; i < 9; ++i) {
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

void UI::RenderPropertiesPanel(Model& model) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Inspector", nullptr, flags);
    
    ImGui::Text("Inspector");
    ImGui::Separator();
    
    // Project metadata
    ImGui::Text("Project");
    ImGui::Separator();
    
    char titleBuf[256];
    strncpy(titleBuf, model.meta.title.c_str(), sizeof(titleBuf) - 1);
    if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
        model.meta.title = titleBuf;
        model.MarkDirty();
    }
    
    char authorBuf[256];
    strncpy(authorBuf, model.meta.author.c_str(), sizeof(authorBuf) - 1);
    if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
        model.meta.author = authorBuf;
        model.MarkDirty();
    }
    
    // TODO: Context-sensitive properties for selected room/door/marker/tile
    
    ImGui::End();
}

void UI::RenderCanvasPanel(
    IRenderer& renderer,
    Model& model, 
    Canvas& canvas, 
    History& history
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
            // Paint tool: Left mouse to paint with selected tile
            // Hold E key + left mouse to erase (alternative to right-click)
            bool shouldPaint = false;
            bool shouldErase = false;
            
            // Check for E key + left mouse (erase modifier)
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && 
                ImGui::IsKeyDown(ImGuiKey_E)) {
                shouldErase = true;
            }
            // Check for left mouse button (primary paint input)
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                shouldPaint = true;
            }
            
            if (shouldPaint) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
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
                    
                    // Find which room this tile belongs to
                    Room* targetRoom = nullptr;
                    for (auto& room : model.rooms) {
                        if (room.rect.Contains(tx, ty)) {
                            targetRoom = &room;
                            break;
                        }
                    }
                    
                    if (targetRoom) {
                        // Convert to room-relative coordinates
                        int roomX = tx - targetRoom->rect.x;
                        int roomY = ty - targetRoom->rect.y;
                        
                        // Get old tile value
                        int oldTileId = model.GetTileAt(
                            targetRoom->id, roomX, roomY
                        );
                        
                        // Only paint if the tile is different
                        if (oldTileId != selectedTileId) {
                            PaintTilesCommand::TileChange change;
                            change.roomId = targetRoom->id;
                            change.x = roomX;
                            change.y = roomY;
                            change.oldTileId = oldTileId;
                            change.newTileId = selectedTileId;
                            
                            currentPaintChanges.push_back(change);
                            
                            // Don't apply immediately - let History execute it
                            // This prevents double-execution and run-length 
                            // encoding issues
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
            }
            
            
            // Handle erase with E+Mouse1 modifier
            if (shouldErase) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
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
                    
                    // Find which room this tile belongs to
                    Room* targetRoom = nullptr;
                    for (auto& room : model.rooms) {
                        if (room.rect.Contains(tx, ty)) {
                            targetRoom = &room;
                            break;
                        }
                    }
                    
                    if (targetRoom) {
                        // Convert to room-relative coordinates
                        int roomX = tx - targetRoom->rect.x;
                        int roomY = ty - targetRoom->rect.y;
                        
                        // Get old tile value
                        int oldTileId = model.GetTileAt(
                            targetRoom->id, roomX, roomY
                        );
                        
                        // Only erase if there's something to erase
                        if (oldTileId != 0) {
                            PaintTilesCommand::TileChange change;
                            change.roomId = targetRoom->id;
                            change.x = roomX;
                            change.y = roomY;
                            change.oldTileId = oldTileId;
                            change.newTileId = 0;  // 0 = empty/erase
                            
                            currentPaintChanges.push_back(change);
                            
                            // Don't apply immediately - let History execute it
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
            }
            
            // When mouse is released, commit the paint command
            if (isPainting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (!currentPaintChanges.empty()) {
                    auto cmd = std::make_unique<PaintTilesCommand>(
                        currentPaintChanges
                    );
                    // Let History execute the command (applies all changes)
                    history.AddCommand(std::move(cmd), model);
                    currentPaintChanges.clear();
                }
                isPainting = false;
                lastPaintedTileX = -1;
                lastPaintedTileY = -1;
            }
        }
        else if (currentTool == Tool::Erase) {
            // Erase tool: Right mouse or two-finger touch
            bool shouldErase = false;
            
            // Check for right mouse button
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                shouldErase = true;
            }
            
            // Check for two-finger touch (SDL multi-touch)
            // Two-finger touch will be detected via SDL events
            // For now, we'll handle right-click as erase
            
            if (shouldErase) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
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
                    
                    // Find which room this tile belongs to
                    Room* targetRoom = nullptr;
                    for (auto& room : model.rooms) {
                        if (room.rect.Contains(tx, ty)) {
                            targetRoom = &room;
                            break;
                        }
                    }
                    
                    if (targetRoom) {
                        // Convert to room-relative coordinates
                        int roomX = tx - targetRoom->rect.x;
                        int roomY = ty - targetRoom->rect.y;
                        
                        // Get old tile value
                        int oldTileId = model.GetTileAt(
                            targetRoom->id, roomX, roomY
                        );
                        
                        // Only erase if there's something to erase
                        if (oldTileId != 0) {
                            PaintTilesCommand::TileChange change;
                            change.roomId = targetRoom->id;
                            change.x = roomX;
                            change.y = roomY;
                            change.oldTileId = oldTileId;
                            change.newTileId = 0;  // 0 = empty/erase
                            
                            currentPaintChanges.push_back(change);
                            
                            // Don't apply immediately - let History execute it
                        }
                        
                        lastPaintedTileX = tx;
                        lastPaintedTileY = ty;
                        isPainting = true;
                    }
                }
            }
            
            // When mouse is released, commit the erase command
            if (isPainting && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                if (!currentPaintChanges.empty()) {
                    auto cmd = std::make_unique<PaintTilesCommand>(
                        currentPaintChanges
                    );
                    // Note: Command already applied, so don't execute again
                    history.AddCommand(std::move(cmd), model);
                    currentPaintChanges.clear();
                }
                isPainting = false;
                lastPaintedTileX = -1;
                lastPaintedTileY = -1;
            }
        }
        // TODO: Add input handling for other tools (Fill, Rectangle, etc.)
    }
    
    // Clear selection if we click outside canvas
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
        !ImGui::IsItemHovered() && isSelecting) {
        isSelecting = false;
    }
    
    // Draw canvas content using ImGui DrawList
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Draw background
    ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    drawList->AddRectFilled(canvasPos, 
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
        bgColor);
    
    // Render the actual canvas content (grid, tiles, rooms, doors, markers)
    canvas.Render(
        renderer,
        model,
        static_cast<int>(canvasPos.x),
        static_cast<int>(canvasPos.y),
        static_cast<int>(canvasSize.x),
        static_cast<int>(canvasSize.y)
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
        ImU32 previewColor;
        if (currentTool == Tool::Erase || 
            (currentTool == Tool::Paint && ImGui::IsKeyDown(ImGuiKey_E))) {
            // Erase preview (red outline)
            previewColor = ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 0.6f));
        } else {
            // Paint preview (show selected tile color)
            Color tileColor(0.8f, 0.8f, 0.8f, 0.4f);
            for (const auto& tile : model.palette) {
                if (tile.id == selectedTileId) {
                    tileColor = tile.color;
                    tileColor.a = 0.5f;  // Semi-transparent
                    break;
                }
            }
            previewColor = tileColor.ToU32();
        }
        
        // Draw preview outline
        drawList->AddRect(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            previewColor,
            0.0f,
            0,
            2.0f
        );
        
        // Draw preview fill for paint mode
        if (currentTool == Tool::Paint && 
            !ImGui::IsKeyDown(ImGuiKey_E)) {
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                previewColor
            );
        }
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
                toolName = "Paint Tool";
                toolHint = "Click to paint | Right-click or E+Click to erase";
                break;
            case Tool::Erase:
                toolName = "Erase Tool";
                toolHint = "Click to erase tiles";
                break;
            case Tool::Fill:
                toolName = "Fill Tool";
                toolHint = "Click to fill area";
                break;
            case Tool::Rectangle:
                toolName = "Rectangle Tool";
                toolHint = "Drag to create room";
                break;
            case Tool::Door:
                toolName = "Door Tool";
                toolHint = "Click to place door";
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
        
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", toolName);
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
            "| Shortcuts: V=Move B=Paint E=Erase I=Eyedropper");
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
        
        // Grid Cell Dimensions
        ImGui::Text("Grid Cell Dimensions:");
        
        ImGui::SliderInt("Cell Width (px)", &model.grid.tileWidth, 
            4, 128);
        ImGui::SameLine();
        if (ImGui::Button("?##cellwidth_settings")) {
            ShowToast("Width of each grid cell in pixels", 
                Toast::Type::Info, 4.0f);
        }
        
        ImGui::SliderInt("Cell Height (px)", &model.grid.tileHeight, 
            4, 128);
        ImGui::SameLine();
        if (ImGui::Button("?##cellheight_settings")) {
            ShowToast("Height of each grid cell in pixels", 
                Toast::Type::Info, 4.0f);
        }
        
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
        
        // Template selection
        ImGui::Text("Template:");
        ImGui::BeginGroup();
        if (ImGui::RadioButton("Custom", 
            selectedTemplate == ProjectTemplate::Custom)) {
            selectedTemplate = ProjectTemplate::Custom;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Small (128x128)", 
            selectedTemplate == ProjectTemplate::Small)) {
            selectedTemplate = ProjectTemplate::Small;
            ApplyTemplate(ProjectTemplate::Small);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Medium (256x256)", 
            selectedTemplate == ProjectTemplate::Medium)) {
            selectedTemplate = ProjectTemplate::Medium;
            ApplyTemplate(ProjectTemplate::Medium);
        }
        
        if (ImGui::RadioButton("Large (512x512)", 
            selectedTemplate == ProjectTemplate::Large)) {
            selectedTemplate = ProjectTemplate::Large;
            ApplyTemplate(ProjectTemplate::Large);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Metroidvania (256x256, 8px)", 
            selectedTemplate == ProjectTemplate::Metroidvania)) {
            selectedTemplate = ProjectTemplate::Metroidvania;
            ApplyTemplate(ProjectTemplate::Metroidvania);
        }
        ImGui::EndGroup();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Configuration
        ImGui::Text("Configuration:");
        
        // Cell dimensions
        ImGui::SliderInt("Cell Width (px)", &newProjectConfig.cellWidth, 
            8, 64);
        ImGui::SameLine();
        if (ImGui::Button("?##cellwidth")) {
            ShowToast("Width of each grid cell in pixels", 
                Toast::Type::Info, 4.0f);
        }
        
        ImGui::SliderInt("Cell Height (px)", &newProjectConfig.cellHeight, 
            8, 64);
        ImGui::SameLine();
        if (ImGui::Button("?##cellheight")) {
            ShowToast("Height of each grid cell in pixels", 
                Toast::Type::Info, 4.0f);
        }
        
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
        int pixelWidth = newProjectConfig.mapWidth * 
            newProjectConfig.cellWidth;
        int pixelHeight = newProjectConfig.mapHeight * 
            newProjectConfig.cellHeight;
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Total cells: %d | Canvas size: %dx%d px",
            totalCells, pixelWidth, pixelHeight);
        
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Apply configuration to model
            model.meta.title = std::string(newProjectConfig.projectName);
            model.grid.tileWidth = newProjectConfig.cellWidth;
            model.grid.tileHeight = newProjectConfig.cellHeight;
            model.grid.cols = newProjectConfig.mapWidth;
            model.grid.rows = newProjectConfig.mapHeight;
            
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
            newProjectConfig.cellWidth = 16;
            newProjectConfig.cellHeight = 16;
            newProjectConfig.mapWidth = 128;
            newProjectConfig.mapHeight = 128;
            break;
            
        case ProjectTemplate::Medium:
            newProjectConfig.cellWidth = 16;
            newProjectConfig.cellHeight = 16;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Large:
            newProjectConfig.cellWidth = 16;
            newProjectConfig.cellHeight = 16;
            newProjectConfig.mapWidth = 512;
            newProjectConfig.mapHeight = 512;
            break;
            
        case ProjectTemplate::Metroidvania:
            newProjectConfig.cellWidth = 8;
            newProjectConfig.cellHeight = 8;
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

} // namespace Cartograph

