#include "ImGuiHelpers.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace Cartograph {
namespace ImGuiHelpers {

void HandleDragDropAutoScroll(float edgeZone, float baseSpeed) {
    // Only scroll when a drag-drop operation is active
    if (!ImGui::IsDragDropActive()) {
        return;
    }
    
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 winMin = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 winMax = ImVec2(winMin.x + winSize.x, winMin.y + winSize.y);
    
    float scrollY = ImGui::GetScrollY();
    float scrollMaxY = ImGui::GetScrollMaxY();
    float deltaTime = ImGui::GetIO().DeltaTime;
    
    // Calculate relative Y position within window
    float relativeY = mousePos.y - winMin.y;
    
    // Scroll up when mouse is near top edge
    if (relativeY < edgeZone && scrollY > 0) {
        // Speed proportional to how close to edge (closer = faster)
        float proximity = 1.0f - (relativeY / edgeZone);
        float speed = baseSpeed * proximity * deltaTime;
        ImGui::SetScrollY(scrollY - speed);
    }
    // Scroll down when mouse is near bottom edge
    else if (relativeY > winSize.y - edgeZone && scrollY < scrollMaxY) {
        float distFromBottom = relativeY - (winSize.y - edgeZone);
        float proximity = distFromBottom / edgeZone;
        float speed = baseSpeed * proximity * deltaTime;
        ImGui::SetScrollY(scrollY + speed);
    }
}

}  // namespace ImGuiHelpers
}  // namespace Cartograph

