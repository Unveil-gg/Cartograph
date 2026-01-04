#include "Modals.h"
#include "../UI.h"
#include "../Keymap.h"
#include <imgui.h>

namespace Cartograph {

void Modals::RenderRebindModal(Model& model, KeymapManager& keymap) {
    // Only call OpenPopup once when modal is first shown
    if (!rebindModalOpened) {
        ImGui::OpenPopup("Rebind Key");
        rebindModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Rebind Key", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Rebind: %s", rebindActionDisplayName.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        
        if (!isCapturing) {
            ImGui::TextWrapped(
                "Press any key combination to bind it to this action.");
            ImGui::Spacing();
            
            if (ImGui::Button("Start Capturing", ImVec2(-1, 0))) {
                isCapturing = true;
                capturedBinding = "";
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                "Press a key combination...");
            ImGui::Spacing();
            
            // Capture key press
            ImGuiIO& io = ImGui::GetIO();
            
            // Build binding string based on current input
            std::string binding = "";
            bool hasModifier = false;
            
            if (io.KeyCtrl) {
                binding += "Ctrl+";
                hasModifier = true;
            }
            if (io.KeyAlt) {
                binding += "Alt+";
                hasModifier = true;
            }
            if (io.KeyShift) {
                binding += "Shift+";
                hasModifier = true;
            }
            if (io.KeySuper) {
#ifdef __APPLE__
                binding += "Cmd+";
#else
                binding += "Super+";
#endif
                hasModifier = true;
            }
            
            // Check for key press (A-Z, 0-9, special keys)
            for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
                if (ImGui::IsKeyPressed((ImGuiKey)key, false)) {
                    char c = 'A' + (key - ImGuiKey_A);
                    binding += std::string(1, c);
                    capturedBinding = binding;
                    break;
                }
            }
            for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
                if (ImGui::IsKeyPressed((ImGuiKey)key, false)) {
                    char c = '0' + (key - ImGuiKey_0);
                    binding += std::string(1, c);
                    capturedBinding = binding;
                    break;
                }
            }
            // Function keys
            const ImGuiKey functionKeys[] = {
                ImGuiKey_F1, ImGuiKey_F2, ImGuiKey_F3, ImGuiKey_F4,
                ImGuiKey_F5, ImGuiKey_F6, ImGuiKey_F7, ImGuiKey_F8,
                ImGuiKey_F9, ImGuiKey_F10, ImGuiKey_F11, ImGuiKey_F12
            };
            const char* functionKeyNames[] = {
                "F1", "F2", "F3", "F4", "F5", "F6", 
                "F7", "F8", "F9", "F10", "F11", "F12"
            };
            for (int i = 0; i < 12; i++) {
                if (ImGui::IsKeyPressed(functionKeys[i], false)) {
                    binding += functionKeyNames[i];
                    capturedBinding = binding;
                    break;
                }
            }
            // Special keys
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                binding += "Space";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
                binding += "Enter";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                // Cancel on Escape
                isCapturing = false;
                capturedBinding = "";
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                binding += "Delete";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
                binding += "Backspace";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Equal, false)) {
                binding += "=";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus, false)) {
                binding += "-";
                capturedBinding = binding;
            }
            
            // Show what's being pressed
            if (!capturedBinding.empty()) {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                    "Captured: %s", capturedBinding.c_str());
            }
            
            ImGui::Spacing();
            if (ImGui::Button("Cancel Capture", ImVec2(-1, 0))) {
                isCapturing = false;
                capturedBinding = "";
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Show current binding
        auto it = model.keymap.find(rebindAction);
        if (it != model.keymap.end() && !it->second.empty()) {
            ImGui::Text("Current: %s", 
                keymap.GetBindingDisplayName(it->second).c_str());
        } else {
            ImGui::TextDisabled("Current: (Not bound)");
        }
        
        ImGui::Spacing();
        
        // Check for conflicts
        std::string conflictAction = "";
        bool hasConflict = false;
        if (!capturedBinding.empty()) {
            conflictAction = keymap.FindConflict(capturedBinding, rebindAction);
            hasConflict = !conflictAction.empty();
        }
        
        // Show conflict warning
        if (hasConflict) {
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
            ImGui::TextWrapped(
                "Warning: '%s' is already bound to '%s'", 
                capturedBinding.c_str(), 
                conflictAction.c_str());
            ImGui::PopStyleColor();
            
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "Applying will remove the existing binding.");
            ImGui::Spacing();
        }
        
        // Action buttons
        bool canApply = !capturedBinding.empty() && 
                       keymap.IsBindingValid(capturedBinding);
        
        if (!canApply) {
            ImGui::BeginDisabled();
        }
        
        std::string applyLabel = hasConflict ? "Apply (Override)" : "Apply";
        if (ImGui::Button(applyLabel.c_str(), ImVec2(140, 0))) {
            // If there's a conflict, clear the conflicting binding
            if (hasConflict) {
                model.keymap[conflictAction] = "";
                keymap.SetBinding(conflictAction, "");
            }
            
            // Apply the new binding
            model.keymap[rebindAction] = capturedBinding;
            keymap.SetBinding(rebindAction, capturedBinding);
            showRebindModal = false;
            rebindModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            if (hasConflict) {
                m_ui.ShowToast("Keybinding updated (conflict resolved)", 
                         Toast::Type::Warning);
            } else {
                m_ui.ShowToast("Keybinding updated", Toast::Type::Success);
            }
        }
        if (!canApply) {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRebindModal = false;
            rebindModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph
