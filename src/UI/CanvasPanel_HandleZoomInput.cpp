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

void CanvasPanel::HandleZoomInput(
    Model& model,
    Canvas& canvas,
    History& history
) {
            // Zoom tool: 
            // - Left-click: Zoom in centered on click point
            // - Right-click: Zoom out centered on click point
            
            // Zoom presets (display percentages)
            static const int ZOOM_PRESETS[] = {
                10, 25, 50, 75, 100, 150, 200, 400, 800, 1000
            };
            static const int ZOOM_PRESET_COUNT = 
                sizeof(ZOOM_PRESETS) / sizeof(ZOOM_PRESETS[0]);
            
            // Helper lambdas for zoom calculations
            auto displayToInternal = [](int pct) {
                return (pct / 100.0f) * Canvas::DEFAULT_ZOOM;
            };
            auto internalToDisplay = [](float zoom) {
                return static_cast<int>(
                    (zoom / Canvas::DEFAULT_ZOOM) * 100.0f
                );
            };
            auto getNextPreset = [&](int currentPct, bool zoomIn) {
                if (zoomIn) {
                    for (int i = 0; i < ZOOM_PRESET_COUNT; ++i) {
                        if (ZOOM_PRESETS[i] > currentPct) {
                            return ZOOM_PRESETS[i];
                        }
                    }
                    return ZOOM_PRESETS[ZOOM_PRESET_COUNT - 1];
                } else {
                    for (int i = ZOOM_PRESET_COUNT - 1; i >= 0; --i) {
                        if (ZOOM_PRESETS[i] < currentPct) {
                            return ZOOM_PRESETS[i];
                        }
                    }
                    return ZOOM_PRESETS[0];
                }
            };
            
            ImVec2 mousePos = ImGui::GetMousePos();
            int currentPercent = internalToDisplay(canvas.zoom);
            float oldZoom = canvas.zoom;
            
            // Left-click: Zoom in
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                int newPercent = getNextPreset(currentPercent, true);
                if (newPercent != currentPercent) {
                    float newZoom = displayToInternal(newPercent);
                    
                    // Zoom centered on click point
                    canvas.ZoomToPoint(newZoom, mousePos.x, mousePos.y);
                    
                    // Add to history
                    auto cmd = std::make_unique<SetZoomCommand>(
                        canvas, oldZoom, newZoom, newPercent
                    );
                    history.AddCommand(std::move(cmd), model, false);
                }
            }
            // Right-click: Zoom out
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                int newPercent = getNextPreset(currentPercent, false);
                if (newPercent != currentPercent) {
                    float newZoom = displayToInternal(newPercent);
                    
                    // Zoom centered on click point
                    canvas.ZoomToPoint(newZoom, mousePos.x, mousePos.y);
                    
                    // Add to history
                    auto cmd = std::make_unique<SetZoomCommand>(
                        canvas, oldZoom, newZoom, newPercent
                    );
                    history.AddCommand(std::move(cmd), model, false);
                }
            }
}

}  // namespace Cartograph
