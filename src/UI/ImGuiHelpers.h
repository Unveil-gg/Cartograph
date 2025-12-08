#pragma once

namespace Cartograph {
namespace ImGuiHelpers {

/**
 * Auto-scroll a child window when mouse is near edges during drag-drop.
 * Call this inside a BeginChild()/EndChild() block while a drag-drop
 * operation is active. Uses smooth, framerate-independent scrolling.
 * 
 * @param edgeZone Pixels from edge to trigger scrolling (default 40)
 * @param baseSpeed Scroll speed in pixels per second (default 400)
 */
void HandleDragDropAutoScroll(float edgeZone = 40.0f, 
                               float baseSpeed = 400.0f);

}  // namespace ImGuiHelpers
}  // namespace Cartograph

