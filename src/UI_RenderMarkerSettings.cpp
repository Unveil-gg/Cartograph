#include "UI.h"
#include "Config.h"
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
#include "UI/ImGuiHelpers.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
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
#include <map>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// SDL_CursorDeleter implementation (outside namespace)


namespace Cartograph {

void UI::RenderMarkerSettings(
    Model& model,
    History& history
) {
        ImGui::Text("Marker Settings");
        ImGui::Separator();
        RenderMarkerKindFields(model, history);
        
        // Helper lambda to capture current marker properties
        auto captureMarkerProps = [](const Marker* m) -> MarkerPropertiesSnapshot {
            MarkerPropertiesSnapshot snap;
            if (m) {
                snap.label = m->label;
                snap.icon = m->icon;
                snap.color = m->color;
                snap.showLabel = m->showLabel;
            }
            return snap;
        };
        
        // Label input
        char labelBuf[128];
        std::strncpy(labelBuf, m_canvasPanel.markerLabel.c_str(), 
                    sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
        
        // Capture state when label input becomes active
        if (ImGui::IsItemActive() == false) {
            // Will check activation after InputText
        }
        
        // Look up selected marker by ID (safe across vector reallocation)
        Marker* selMarker = model.FindMarker(m_canvasPanel.selectedMarkerId);
        
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
            m_canvasPanel.markerLabel = labelBuf;
            
            // Update selected marker if editing
            if (selMarker) {
                selMarker->label = m_canvasPanel.markerLabel;
                selMarker->showLabel = !m_canvasPanel.markerLabel.empty();
                model.MarkDirty();
            }
        }
        
        // Capture state when starting to edit label
        if (ImGui::IsItemActivated()) {
            if (selMarker) {
                // Editing a specific marker - clear palette style selection
                m_selectedPaletteStyleKey.clear();
                m_paletteStyleMarkerCount = 0;
                m_markerEditStartState = captureMarkerProps(selMarker);
                m_editingMarkerId = selMarker->id;
            }
        }
        
        // Handle label edit completion
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            // Check if editing from a palette style with multiple markers
            if (!m_selectedPaletteStyleKey.empty() && 
                m_paletteStyleMarkerCount > 1) {
                // Show rename dialog - ask "Rename All" or "Create New"
                m_modals.showMarkerLabelRenameModal = true;
                m_modals.markerLabelRenameStyleKey = m_selectedPaletteStyleKey;
                m_modals.markerLabelRenameNewLabel = m_canvasPanel.markerLabel;
                m_modals.markerLabelRenameCount = m_paletteStyleMarkerCount;
            } else if (selMarker && m_editingMarkerId == selMarker->id) {
                // Single marker selected - create undo command
                auto newProps = captureMarkerProps(selMarker);
                if (newProps != m_markerEditStartState) {
                    auto cmd = std::make_unique<ModifyMarkerPropertiesCommand>(
                        selMarker->id,
                        m_markerEditStartState,
                        newProps
                    );
                    history.AddCommand(std::move(cmd), model, false);
                }
            }
            // Else: no marker selected, just updated template - nothing to save
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
            // Capture state when popup opens
            if (selMarker) {
                m_markerEditStartState = captureMarkerProps(selMarker);
                m_editingMarkerId = selMarker->id;
            }
        }
        
        // Color picker popup
        bool colorPickerIsOpen = ImGui::BeginPopup("ColorPicker");
        if (colorPickerIsOpen) {
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
                m_canvasPanel.markerColorHex[
                    sizeof(m_canvasPanel.markerColorHex) - 1] = '\0';
                
                // Update selected marker if editing
                if (selMarker) {
                    selMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
            ImGui::EndPopup();
        }
        
        // Create command when color picker closes
        if (m_colorPickerWasOpen && !colorPickerIsOpen && 
            selMarker && m_editingMarkerId == selMarker->id) {
            auto newProps = captureMarkerProps(selMarker);
            if (newProps != m_markerEditStartState) {
                auto cmd = std::make_unique<ModifyMarkerPropertiesCommand>(
                    selMarker->id,
                    m_markerEditStartState,
                    newProps
                );
                history.AddCommand(std::move(cmd), model, false);
            }
        }
        m_colorPickerWasOpen = colorPickerIsOpen;
        
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
                if (selMarker) {
                    selMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
        }
        
        // Capture state when starting to edit hex color
        if (ImGui::IsItemActivated() && selMarker) {
            m_markerEditStartState = captureMarkerProps(selMarker);
            m_editingMarkerId = selMarker->id;
        }
        
        // Create command when hex edit finishes
        if (ImGui::IsItemDeactivatedAfterEdit() && selMarker &&
            m_editingMarkerId == selMarker->id) {
            auto newProps = captureMarkerProps(selMarker);
            if (newProps != m_markerEditStartState) {
                auto cmd = std::make_unique<ModifyMarkerPropertiesCommand>(
                    selMarker->id,
                    m_markerEditStartState,
                    newProps
                );
                history.AddCommand(std::move(cmd), model, false);
            }
        }
        
        // Show hint text
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Hex color (e.g., #4dcc4d)");
        }
        
        ImGui::Separator();
        
}

}  // namespace Cartograph
