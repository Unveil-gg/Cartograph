#include "CanvasPanel.h"
#include "Modals.h"
#include "../App.h"
#include "../platform/Paths.h"
#include "../platform/System.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>
#include <stb/stb_image.h>
#include <filesystem>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <set>
#include <cstring>


namespace Cartograph {

void CanvasPanel::Render(
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
            
            // Convert to fractional tile coordinates (Y-up: negate world Y)
            float tileX = wx / model.grid.tileWidth;
            float tileY = -wy / model.grid.tileHeight;
            
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
        HandleToolShortcuts(keymap);
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
            HandleSelectInput(model, canvas, history);
        }
        else if (currentTool == Tool::Paint) {
            HandlePaintInput(model, canvas, history);
        }
        else if (currentTool == Tool::Erase) {
            HandleEraseInput(model, canvas);
        }
        else if (currentTool == Tool::Fill) {
            HandleFillInput(model, canvas, history);
        }
        else if (currentTool == Tool::Eyedropper) {
            HandleEyedropperInput(model, canvas);
        }
        else if (currentTool == Tool::Marker) {
            HandleMarkerInput(model, canvas, history);
        }
        else if (currentTool == Tool::Zoom) {
            HandleZoomInput(model, canvas, history);
        }
        else if (currentTool == Tool::RoomPaint) {
            HandleRoomPaintInput(model, canvas, history);
        }
        else if (currentTool == Tool::RoomErase) {
            HandleRoomEraseInput(model, canvas, history);
        }
        else if (currentTool == Tool::RoomFill) {
            HandleRoomFillInput(model, canvas, history);
        }
        else if (currentTool == Tool::RoomSelect) {
            HandleRoomSelectInput(model, canvas);
        }
    }
    
    // Handle RoomSelect drag-drop source (outside hover check)
    // This ensures the drag tooltip continues even when mouse is over hierarchy
    if (currentTool == Tool::RoomSelect) {
        HandleRoomSelectDrag(model, canvas, history);
    }
    
    // Handle mouse release for Paint/Erase tools (outside hover check)
    // This ensures releases are detected even if mouse drifts outside canvas
    if (currentTool == Tool::Paint || currentTool == Tool::Erase) {
        HandlePaintRelease(model, history);
    }
    
    // Clear selection if we click outside canvas
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
        !ImGui::IsItemHovered() && isSelecting) {
        isSelecting = false;
    }
    
    // Keyboard shortcuts
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        HandleCanvasKeys(model, canvas, history, keymap);
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
    
    // Update cursor based on current tool and hover state
    UpdateCursor();
    
    // Update hovered marker (if Marker tool is active)
    if (currentTool == Tool::Marker && ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float wx, wy;
        canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
        
        // Y-up: negate world Y for tile coordinates
        float tileX = wx / model.grid.tileWidth;
        float tileY = -wy / model.grid.tileHeight;
        
        Marker* hovered = model.FindMarkerNear(tileX, tileY, 0.5f);
        hoveredMarkerId = hovered ? hovered->id : "";
        
        // Show tooltip for hovered marker
        if (hovered && !isDraggingMarker) {
            ImGui::BeginTooltip();
            ImGui::Text("Marker: %s", hovered->label.empty() ? 
                       "(no label)" : hovered->label.c_str());
            ImGui::TextDisabled("Icon: %s", hovered->icon.c_str());
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               hovered->x, hovered->y);
            ImGui::Separator();
            ImGui::TextDisabled("Click: Select/Move");
            ImGui::TextDisabled("Shift+Click: Delete");
            ImGui::EndTooltip();
        }
    } else {
        hoveredMarkerId.clear();
    }
    
    // Render the actual canvas content (grid, tiles, rooms, doors, markers, overlays)
    // Look up markers by ID for safe pointer access during render
    const Marker* selMarkerPtr = model.FindMarker(selectedMarkerId);
    const Marker* hovMarkerPtr = model.FindMarker(hoveredMarkerId);
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
        selMarkerPtr,      // Pass selected marker for highlight
        hovMarkerPtr,      // Pass hovered marker for highlight
        nullptr,           // No special render context
        hoveredRoomId,     // Pass hovered room for highlight
        selectedRoomId,    // Pass selected room for stronger highlight
        selectedRegionGroupId  // Pass selected region for highlight + greyscale
    );
    
    // Clear hovered room at end of frame (will be set again if still hovering)
    hoveredRoomId.clear();
    
    // Note: Thumbnail capture moved to App::Render() after ImGui draw data
    // is rendered to ensure pixels are actually in the framebuffer
    
    // Draw selection rectangle while dragging
    if (currentTool == Tool::Select && isSelecting) {
        DrawSelectionDrag(model, drawList);
    }
    
    // Draw selection highlight for completed selection
    if (hasSelection && !currentSelection.IsEmpty()) {
        DrawSelectionHighlight(model, canvas, drawList);
    }
    
    // Draw paste preview if in paste mode
    if (currentTool == Tool::Select && isPasteMode && !clipboard.IsEmpty()) {
        DrawPastePreview(model, canvas, drawList);
    }
    
    // Draw paint cursor preview if Paint or Erase tool is active
    if ((currentTool == Tool::Paint || currentTool == Tool::Erase) && 
        ImGui::IsItemHovered()) {
        DrawPaintPreview(model, canvas, drawList);
    }
    
    // Draw fill cursor preview if Fill tool is active
    if (currentTool == Tool::Fill && ImGui::IsItemHovered()) {
        DrawFillPreview(model, canvas, drawList);
    }
    
    // Draw eyedropper hover highlight if Eyedropper tool is active
    if (currentTool == Tool::Eyedropper && ImGui::IsItemHovered()) {
        DrawEyedropperPreview(model, canvas, drawList);
    }
    
    // Room Select tool: show hover highlight and tooltip for room under cursor
    if (currentTool == Tool::RoomSelect && ImGui::IsItemHovered()) {
        DrawRoomSelectHover(model, canvas);
    }
    
    // Draw marker snap point preview if Marker tool is active
    if (currentTool == Tool::Marker && ImGui::IsItemHovered()) {
        DrawMarkerSnapPreview(model, canvas, drawList);
    }
    
    // Draw drag-drop preview when dragging icon over canvas
    if (ImGui::IsDragDropActive() && ImGui::IsItemHovered()) {
        DrawMarkerDropPreview(model, canvas, drawList);
    }
    
    // Pop clip rect before ending window
    drawList->PopClipRect();
    
    ImGui::End();
}

}  // namespace Cartograph
