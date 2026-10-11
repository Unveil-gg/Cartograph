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

void UI::RenderRoomFields(
    Model& model,
    History& history,
    Room* room
) {
                // Room name
                char nameBuf[256];
                strncpy(nameBuf, room->name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name##Room", nameBuf, sizeof(nameBuf))) {
                    room->name = nameBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit name
                if (ImGui::IsItemActivated()) {
                    m_roomEditStartState = CaptureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                // Create command when name edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRoomId == room->id) {
                    auto newProps = CaptureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Room color (hex input like palette)
                static char roomColorHex[16] = "";
                if (roomColorHex[0] == '\0') {
                    // Initialize hex from current color
                    strncpy(roomColorHex, room->color.ToHex(false).c_str(), 
                           sizeof(roomColorHex) - 1);
                }
                
                ImGui::Text("Color:");
                ImGui::SameLine();
                
                // Color preview button
                ImVec4 colorPreview = room->color.ToImVec4();
                if (ImGui::ColorButton("##roomColorPreview", colorPreview,
                                      ImGuiColorEditFlags_NoTooltip,
                                      ImVec2(24, 24))) {
                    // Click opens full color picker - capture state
                    ImGui::OpenPopup("##roomColorPicker");
                    m_roomEditStartState = CaptureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                ImGui::SameLine();
                
                // Hex input field
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("##roomColorHex", roomColorHex, 
                                    sizeof(roomColorHex))) {
                    // Parse hex and update color
                    Color newColor = Color::FromHex(roomColorHex);
                    room->color = newColor;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit hex color
                if (ImGui::IsItemActivated()) {
                    m_roomEditStartState = CaptureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                // Create command when hex edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRoomId == room->id) {
                    auto newProps = CaptureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Full color picker popup
                bool roomColorPickerIsOpen = ImGui::BeginPopup("##roomColorPicker");
                if (roomColorPickerIsOpen) {
                    float colorArray[4] = {
                        room->color.r, room->color.g,
                        room->color.b, room->color.a
                    };
                    if (ImGui::ColorPicker4("##picker", colorArray)) {
                        room->color = Color(colorArray[0], colorArray[1],
                                          colorArray[2], colorArray[3]);
                        // Update hex display
                        strncpy(roomColorHex, room->color.ToHex(false).c_str(),
                               sizeof(roomColorHex) - 1);
                        model.MarkDirty();
                    }
                    ImGui::EndPopup();
                }
                
                // Create command when color picker closes
                if (m_roomColorPickerWasOpen && !roomColorPickerIsOpen && 
                    m_editingRoomId == room->id) {
                    auto newProps = CaptureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                m_roomColorPickerWasOpen = roomColorPickerIsOpen;
                
                // Room description
                ImGui::Spacing();
                ImGui::Text("Description:");
                char notesBuf[1024];
                strncpy(notesBuf, room->notes.c_str(), sizeof(notesBuf) - 1);
                if (ImGui::InputTextMultiline("##description", notesBuf, 
                                             sizeof(notesBuf), 
                                             ImVec2(-1, 80),
                                             ImGuiInputTextFlags_WordWrap)) {
                    room->notes = notesBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit notes
                if (ImGui::IsItemActivated()) {
                    m_roomEditStartState = CaptureRoomProps(room);
                    m_editingRoomId = room->id;
                }
                
                // Create command when notes edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRoomId == room->id) {
                    auto newProps = CaptureRoomProps(room);
                    if (newProps != m_roomEditStartState) {
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, m_roomEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Tags
                ImGui::Spacing();
                ImGui::Text("Tags:");
                
                // Display existing tags as chips
                bool tagRemoved = false;
                std::string tagToRemove;
                for (const auto& tag : room->tags) {
                    ImGui::PushID(tag.c_str());
                    
                    // Tag chip with X button
                    ImVec4 chipColor(0.3f, 0.5f, 0.8f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, chipColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                        ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                        ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
                    
                    std::string chipLabel = tag + " X";
                    if (ImGui::SmallButton(chipLabel.c_str())) {
                        tagToRemove = tag;
                        tagRemoved = true;
                    }
                    
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                    ImGui::PopID();
                }
                
                if (tagRemoved) {
                    // Capture old state before removing tag
                    auto oldProps = CaptureRoomProps(room);
                    
                    room->tags.erase(
                        std::remove(room->tags.begin(), room->tags.end(), 
                                   tagToRemove),
                        room->tags.end()
                    );
                    model.MarkDirty();
                    
                    // Create command for tag removal
                    auto newProps = CaptureRoomProps(room);
                    auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                        room->id, oldProps, newProps);
                    history.AddCommand(std::move(cmd), model, false);
                }
                
                // If tags exist, add newline; otherwise keep tight
                if (!room->tags.empty()) {
                    ImGui::NewLine();
                }
                
                // Add new tag (inline layout)
                static char newTagBuf[64] = "";
                ImGui::SetNextItemWidth(-70);
                bool enterPressed = ImGui::InputTextWithHint(
                    "##newtag", "Add tag...", newTagBuf, sizeof(newTagBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );
                
                ImGui::SameLine();
                bool addClicked = ImGui::Button("Add");
                
                if ((enterPressed || addClicked) && strlen(newTagBuf) > 0) {
                    std::string newTag(newTagBuf);
                    // Check if tag already exists
                    if (std::find(room->tags.begin(), room->tags.end(), newTag) == 
                        room->tags.end()) {
                        // Capture old state before adding tag
                        auto oldProps = CaptureRoomProps(room);
                        
                        room->tags.push_back(newTag);
                        model.MarkDirty();
                        
                        // Create command for tag addition
                        auto newProps = CaptureRoomProps(room);
                        auto cmd = std::make_unique<ModifyRoomPropertiesCommand>(
                            room->id, oldProps, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                    newTagBuf[0] = '\0';  // Clear input
                }
                
}

}  // namespace Cartograph
