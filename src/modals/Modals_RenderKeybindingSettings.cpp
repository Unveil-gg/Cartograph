#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../Canvas.h"
#include "../Config.h"
#include "../Preferences.h"
#include "../Package.h"
#include "../ProjectFolder.h"
#include "../Theme/Themes.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>


namespace Cartograph {

void Modals::RenderKeybindingSettings(
    Model& model,
    KeymapManager& keymap
) {
                settingsModalSelectedTab = 2;
                ImGui::Spacing();
                
                ImGui::Text("Keyboard Shortcuts");
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset All to Defaults")) {
                    model.InitDefaultKeymap();
                    keymap.LoadBindings(model.keymap);
                    m_ui.ShowToast("Keybindings reset to defaults", 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                ImGui::Spacing();
                
                if (ImGui::BeginTable("KeybindingsTable", 3, 
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                    ImGuiTableFlags_ScrollY, 
                    ImVec2(0, 450))) {
                    
                    ImGui::TableSetupColumn("Action", 
                        ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Binding", 
                        ImGuiTableColumnFlags_WidthFixed, 160.0f);
                    ImGui::TableSetupColumn("Actions", 
                        ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    
                    // Helper lambda to render a keybinding row
                    auto renderBinding = [&](const char* displayName, 
                                             const char* action) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", displayName);
                        
                        ImGui::TableNextColumn();
                        auto it = model.keymap.find(action);
                        std::string bindingStr = 
                            (it != model.keymap.end()) ? 
                            keymap.GetBindingDisplayName(it->second) : 
                            "(Not bound)";
                        ImGui::TextColored(
                            (it != model.keymap.end()) ? 
                                ImVec4(0.7f, 0.9f, 1.0f, 1.0f) : 
                                ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
                            "%s", bindingStr.c_str());
                        
                        ImGui::TableNextColumn();
                        ImGui::PushID(action);
                        if (ImGui::SmallButton("Rebind")) {
                            rebindAction = action;
                            rebindActionDisplayName = displayName;
                            capturedBinding = "";
                            isCapturing = false;
                            showRebindModal = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Clear")) {
                            model.keymap[action] = "";
                            keymap.SetBinding(action, "");
                        }
                        ImGui::PopID();
                    };
                    
                    // TOOLS CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "TOOLS");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Tool: Move", "toolMove");
                    renderBinding("Tool: Select", "toolSelect");
                    renderBinding("Tool: Paint", "toolPaint");
                    renderBinding("Tool: Erase", "toolErase");
                    renderBinding("Tool: Fill", "toolFill");
                    renderBinding("Tool: Marker", "toolMarker");
                    renderBinding("Tool: Eyedropper", "toolEyedropper");
                    renderBinding("Tool: Zoom", "toolZoom");
                    renderBinding("Tool: Room Select", "toolRoomSelect");
                    renderBinding("Tool: Room Paint", "toolRoomPaint");
                    renderBinding("Tool: Room Fill", "toolRoomFill");
                    renderBinding("Tool: Room Erase", "toolRoomErase");
                    
                    // VIEW CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "VIEW");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Zoom In", "zoomIn");
                    renderBinding("Zoom Out", "zoomOut");
                    renderBinding("Toggle Grid", "toggleGrid");
                    renderBinding("Toggle Room Overlays", "toggleRoomOverlays");
                    renderBinding("Toggle Hierarchy Panel", 
                                 "togglePropertiesPanel");
                    
                    // EDIT CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "EDIT");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Undo", "undo");
                    renderBinding("Redo", "redo");
                    renderBinding("Copy", "copy");
                    renderBinding("Paste", "paste");
                    renderBinding("Delete", "delete");
                    renderBinding("Delete (Alt)", "deleteAlt");
                    
                    // ACTIONS CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "ACTIONS");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Place Wall", "placeWall");
                    renderBinding("Place Door", "placeDoor");
                    renderBinding("Detect Rooms", "detectRooms");
                    
                    // FILE CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "FILE");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("New Project", "new");
                    renderBinding("Open Project", "open");
                    renderBinding("Save", "save");
                    renderBinding("Save As", "saveAs");
                    renderBinding("Export PNG", "export");
                    renderBinding("Export Package", "exportPackage");
                    
                    ImGui::EndTable();
                }
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
}

}  // namespace Cartograph
