#include "UI.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "render/Renderer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace Cartograph {

UI::UI() {
}

void UI::SetupDockspace() {
    // Docking setup happens in first frame of Render()
}

void UI::Render(
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
    RenderToolsPanel();
    RenderCanvasPanel(model, canvas);
    RenderPropertiesPanel(model);
    RenderStatusBar(model, canvas);
    
    // Render toasts
    RenderToasts(deltaTime);
    
    // Render modal dialogs
    if (showExportModal) {
        RenderExportModal(model, canvas);
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

void UI::RenderToolsPanel() {
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
    
    for (int i = 0; i < 9; ++i) {
        bool selected = (static_cast<int>(currentTool) == i);
        if (ImGui::Selectable(toolNames[i], selected)) {
            currentTool = static_cast<Tool>(i);
        }
    }
    
    // TODO: Add palette section here
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

void UI::RenderCanvasPanel(Model& model, Canvas& canvas) {
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
        // TODO: Add input handling for other tools (Paint, Erase, etc.)
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
    
    // Draw grid if enabled
    if (canvas.showGrid) {
        int tileSize = model.grid.tileSize;
        float scaledTileSize = tileSize * canvas.zoom;
        
        // Calculate visible grid range
        float startX = canvasPos.x - fmod(canvas.offsetX * canvas.zoom, scaledTileSize);
        float startY = canvasPos.y - fmod(canvas.offsetY * canvas.zoom, scaledTileSize);
        
        ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        // Draw vertical lines
        for (float x = startX; x < canvasPos.x + canvasSize.x; x += scaledTileSize) {
            drawList->AddLine(
                ImVec2(x, canvasPos.y),
                ImVec2(x, canvasPos.y + canvasSize.y),
                gridColor
            );
        }
        
        // Draw horizontal lines
        for (float y = startY; y < canvasPos.y + canvasSize.y; y += scaledTileSize) {
            drawList->AddLine(
                ImVec2(canvasPos.x, y),
                ImVec2(canvasPos.x + canvasSize.x, y),
                gridColor
            );
        }
    }
    
    // TODO: Draw rooms, tiles, doors, markers
    // For now, just show the grid
    
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
    
    ImGui::End();
}

void UI::RenderStatusBar(Model& model, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::Begin("Cartograph/Console", nullptr, flags);
    ImGui::PopStyleVar();
    
    // Thin horizontal status bar layout
    // Left section: Zoom
    ImGui::Text("Zoom: %.0f%%", canvas.zoom * 100.0f);
    
    // Console output section
    ImGui::SameLine(0, 20);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "Console: Ready");
    
    // Error/Warning section (if any)
    if (model.dirty) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
            "⚠ Modified");
    }
    
    // Right section: Reserved space for future icons and info
    ImGui::SameLine(0, 20);
    
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

