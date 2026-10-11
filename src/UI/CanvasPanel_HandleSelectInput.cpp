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

void CanvasPanel::HandleSelectInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Door spawn placement owns the click while it is active
            if (HandleDoorSpawnInput(model, canvas, history)) {
                return;
            }
            
            // Update paste preview position
            if (isPasteMode) {
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &pastePreviewX, &pastePreviewY
                );
                
                // Click to commit paste
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    PasteClipboard(model, history, pastePreviewX, pastePreviewY);
                    ExitPasteMode();
                }
            }
            // Check if clicking inside existing selection (keep selection active)
            // or outside (start new selection)
            else if (hasSelection && 
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                int clickTileX, clickTileY;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &clickTileX, &clickTileY
                );
                
                // Check if click is inside selection bounds
                bool insideBounds = 
                    currentSelection.bounds.Contains(clickTileX, clickTileY);
                
                if (!insideBounds) {
                    // Clicking outside: clear selection and start new
                    ClearSelection();
                    selectionStartX = mousePos.x;
                    selectionStartY = mousePos.y;
                    selectionEndX = mousePos.x;
                    selectionEndY = mousePos.y;
                    isSelecting = true;
                }
                // Clicking inside: do nothing, keep selection active
                // (use Cut/Copy/Paste to move content)
            }
            // Start new selection (no existing selection)
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ClearSelection();
                selectionStartX = mousePos.x;
                selectionStartY = mousePos.y;
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                isSelecting = true;
            }
            
            // Update selection rectangle while dragging
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && isSelecting) {
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
            }
            
            // Finish selection rectangle
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isSelecting) {
                selectionEndX = mousePos.x;
                selectionEndY = mousePos.y;
                isSelecting = false;
                
                // A click (not a drag) on a door edge inspects that door
                float dragX = selectionEndX - selectionStartX;
                float dragY = selectionEndY - selectionStartY;
                bool isClick = dragX * dragX + dragY * dragY < 9.0f;
                hasSelectedDoor = false;
                if (isClick && TrySelectDoorAt(model, canvas,
                                               mousePos.x, mousePos.y)) {
                    currentSelection.Clear();
                    hasSelection = false;
                    return;
                }
                
                // Populate selection with content from the rectangle
                PopulateSelectionFromRect(model, canvas);
            }
            
            // Right-click context menu for selection operations
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && 
                !isSelecting && !isPasteMode) {
                ImGui::OpenPopup("SelectionContextMenu");
            }
            
            if (ImGui::BeginPopup("SelectionContextMenu")) {
                bool selectionActive = hasSelection && 
                                       !currentSelection.IsEmpty();
                bool clipboardHasContent = !clipboard.IsEmpty();
                
                // Cut (copy + delete)
                if (ImGui::MenuItem("Cut", Platform::FormatShortcut("X").c_str(),
                                   false, selectionActive)) {
                    CopySelection(model);
                    DeleteSelection(model, history);
                }
                
                // Copy
                if (ImGui::MenuItem("Copy", Platform::FormatShortcut("C").c_str(),
                                   false, selectionActive)) {
                    CopySelection(model);
                }
                
                // Paste
                if (ImGui::MenuItem("Paste", 
                                   Platform::FormatShortcut("V").c_str(),
                                   false, clipboardHasContent)) {
                    EnterPasteMode();
                }
                
                ImGui::Separator();
                
                // Delete
                if (ImGui::MenuItem("Delete", "Del", false, selectionActive)) {
                    DeleteSelection(model, history);
                }
                
                ImGui::Separator();
                
                // Select All
                if (ImGui::MenuItem("Select All", 
                                   Platform::FormatShortcut("A").c_str())) {
                    SelectAll(model);
                }
                
                // Deselect
                if (ImGui::MenuItem("Deselect", "Esc", false, selectionActive)) {
                    ClearSelection();
                }
                
                ImGui::EndPopup();
            }
}

}  // namespace Cartograph
