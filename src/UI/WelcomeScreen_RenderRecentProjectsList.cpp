#include "WelcomeScreen.h"
#include "../UI.h"
#include "../App.h"
#include "../Model.h"
#include "../Canvas.h"
#include "../History.h"
#include "../Jobs.h"
#include "../Icons.h"
#include "../Keymap.h"
#include "../Package.h"
#include "../ProjectFolder.h"
#include "../Preferences.h"
#include "../platform/Paths.h"
#include "../platform/Fs.h"
#include "../platform/Time.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

// minizip-ng for reading .cart thumbnails
#include "unzip.h"

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// STB image for loading thumbnails (implementation in Icons.cpp)
#include "../../third_party/stb/stb_image.h"

namespace Cartograph {

void WelcomeScreen::RenderRecentProjectsList(App& app) {
    // Show max 3 most recent projects side-by-side
    const size_t maxDisplay = std::min(recentProjects.size(), size_t(3));
    if (maxDisplay == 0) {
        return;  // No projects to show
    }
    
    // Card dimensions (16:9 aspect ratio thumbnails)
    const float cardWidth = 260.0f;
    const float cardHeight = 174.0f;
    const float thumbnailHeight = 146.0f;  // 260*9/16 = 146.25
    const float cardSpacing = 16.0f;
    const float titleHeight = 28.0f;
    
    // Load textures for visible projects
    for (size_t i = 0; i < maxDisplay; ++i) {
        LoadThumbnailTexture(recentProjects[i]);
    }
    
    // Center the cards horizontally
    ImVec2 windowSize = ImGui::GetWindowSize();
    float totalWidth = maxDisplay * cardWidth + (maxDisplay - 1) * cardSpacing;
    float startX = (windowSize.x - totalWidth) * 0.5f;
    
    ImGui::SetCursorPosX(startX);
    
    // Render each card
    for (size_t i = 0; i < maxDisplay; ++i) {
        auto& project = recentProjects[i];
        
        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();
        
        // Get current cursor position for the card
        ImVec2 cardPos = ImGui::GetCursorScreenPos();
        
        // Thumbnail image
        if (project.thumbnailTextureId != 0) {
            ImVec2 thumbSize(cardWidth, thumbnailHeight);
            
            // Make thumbnail clickable
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                 ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                 ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
            
            // Allow X button to capture clicks over this ImageButton
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::ImageButton(
                    ("##thumb" + std::to_string(i)).c_str(),
                    (ImTextureID)(intptr_t)project.thumbnailTextureId,
                    thumbSize)) {
                app.OpenProject(project.path);
            app.ShowEditor();
        }
            
            ImGui::PopStyleColor(3);
            
            // Tooltip on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", project.path.c_str());
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Last modified: %s", project.lastModified.c_str());
                ImGui::EndTooltip();
            }
            
            // Draw title overlay at bottom-left of thumbnail
            ImVec2 overlayMin(cardPos.x, cardPos.y + thumbnailHeight - 
                             titleHeight);
            ImVec2 overlayMax(cardPos.x + cardWidth, 
                             cardPos.y + thumbnailHeight);
            
            // Semi-transparent dark background for title
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(overlayMin, overlayMax, 
                IM_COL32(0, 0, 0, 180));
            
            // Title text - use ImGui widget for proper font handling
            ImVec2 textPos(cardPos.x + 10, cardPos.y + thumbnailHeight - 
                          titleHeight + 5);
            ImGui::SetCursorScreenPos(textPos);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                              "%s", project.name.c_str());
            
            // Check if mouse is hovering over the card area
            ImVec2 mousePos = ImGui::GetMousePos();
            bool isCardHovered = (mousePos.x >= cardPos.x && 
                                 mousePos.x <= cardPos.x + cardWidth &&
                                 mousePos.y >= cardPos.y && 
                                 mousePos.y <= cardPos.y + thumbnailHeight);
            
            // X button in top-right corner (only show on hover)
            if (isCardHovered) {
                const float xButtonSize = 18.0f;
                const float xButtonMarginX = -2.0f;  // Negative to push further right
                const float xButtonMarginY = 6.0f;   // Slightly lower
                ImVec2 xButtonPos(cardPos.x + cardWidth - xButtonSize - 
                                 xButtonMarginX,
                                 cardPos.y + xButtonMarginY);
                ImGui::SetCursorScreenPos(xButtonPos);
                
                // Style for X button (semi-transparent dark background)
                ImGui::PushStyleColor(ImGuiCol_Button, 
                                     ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                     ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                     ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
                
                std::string xButtonId = "##xclose" + std::to_string(i);
                if (ImGui::Button(xButtonId.c_str(), 
                                 ImVec2(xButtonSize, xButtonSize))) {
                    // Open project action modal
                    m_ui.m_modals.showProjectActionModal = true;
                    m_ui.m_modals.projectActionPath = project.path;
                    m_ui.m_modals.projectActionName = project.name;
                }
                
                // Draw X icon manually (centered in button)
                ImVec2 xCenter(xButtonPos.x + xButtonSize * 0.5f,
                              xButtonPos.y + xButtonSize * 0.5f);
                float xSize = 4.0f;
                ImU32 xColor = IM_COL32(255, 255, 255, 220);
                drawList->AddLine(
                    ImVec2(xCenter.x - xSize, xCenter.y - xSize),
                    ImVec2(xCenter.x + xSize, xCenter.y + xSize),
                    xColor, 2.0f);
                drawList->AddLine(
                    ImVec2(xCenter.x + xSize, xCenter.y - xSize),
                    ImVec2(xCenter.x - xSize, xCenter.y + xSize),
                    xColor, 2.0f);
                
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Remove or delete project");
                }
            }
        }
        
        ImGui::EndGroup();
        ImGui::PopID();
        
        // Add spacing between cards (but not after last card)
        if (i < maxDisplay - 1) {
            ImGui::SameLine(0.0f, cardSpacing);
        }
    }
}



} // namespace Cartograph
