#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../ProjectFolder.h"
#include "../Preferences.h"
#include "../platform/Paths.h"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace Cartograph {


void Modals::RenderProjectBrowserModal(App& app, 
                                       std::vector<RecentProject>& recentProjects) {
    // Only call OpenPopup once when modal is first shown
    if (!projectBrowserModalOpened) {
        ImGui::OpenPopup("Recent Projects");
        projectBrowserModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(880, 600), ImGuiCond_FirstUseEver);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Recent Projects", &showProjectBrowserModal, 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        
        ImGui::Text("All Recent Projects");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Search bar and sorting dropdown on same line
        static char searchFilter[256] = "";
        float comboWidth = 130.0f;
        float searchWidth = ImGui::GetContentRegionAvail().x - comboWidth - 10.0f;
        
        ImGui::SetNextItemWidth(searchWidth);
        if (ImGui::InputTextWithHint("##projectsearch", 
                                     "Search projects...", 
                                     searchFilter, 
                                     sizeof(searchFilter))) {
            // Filter updates on every keystroke
        }
        
        // Sorting dropdown
        ImGui::SameLine();
        ImGui::SetNextItemWidth(comboWidth);
        const char* sortLabels[] = { 
            "Most Recent", "Oldest First", "A -> Z", "Z -> A" 
        };
        int currentSort = static_cast<int>(Preferences::projectBrowserSortOrder);
        if (ImGui::Combo("##sort", &currentSort, sortLabels, 4)) {
            Preferences::projectBrowserSortOrder = 
                static_cast<ProjectSortOrder>(currentSort);
            Preferences::Save();
        }
        
        // Show count of filtered results
        if (searchFilter[0] != '\0') {
            int visibleCount = 0;
            for (const auto& project : recentProjects) {
                std::string lowerName = project.name;
                std::string lowerFilter = searchFilter;
                
                // Convert to lowercase for case-insensitive search
                std::transform(lowerName.begin(), lowerName.end(), 
                              lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), 
                              lowerFilter.begin(), ::tolower);
                
                if (lowerName.find(lowerFilter) != std::string::npos) {
                    visibleCount++;
                }
            }
            
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Showing %d of %zu projects", visibleCount, 
                recentProjects.size());
        }
        
        ImGui::Spacing();
        
        // Scrollable content area
        ImGui::BeginChild("ProjectList", ImVec2(0, -40), true);
        
        // Card dimensions for modal view (sized to fit 3 per row snugly)
        const float cardWidth = 265.0f;
        const float cardHeight = 190.0f;
        const float thumbnailHeight = 149.0f;  // 265*9/16
        const float titleHeight = 25.0f;
        const float cardSpacing = 12.0f;
        const int cardsPerRow = 3;
        
        // Load all thumbnails
        for (auto& project : recentProjects) {
            m_ui.m_welcomeScreen.LoadThumbnailTexture(project);
        }
        
        // Build sorted list of project pointers
        std::vector<RecentProject*> sortedProjects;
        for (auto& p : recentProjects) {
            sortedProjects.push_back(&p);
        }
        
        // Apply sort order
        switch (Preferences::projectBrowserSortOrder) {
            case ProjectSortOrder::MostRecent:
                // Already sorted by LoadRecentProjects()
                break;
            case ProjectSortOrder::OldestFirst:
                std::reverse(sortedProjects.begin(), sortedProjects.end());
                break;
            case ProjectSortOrder::AtoZ:
                std::sort(sortedProjects.begin(), sortedProjects.end(),
                    [](const RecentProject* a, const RecentProject* b) {
                        // Case-insensitive comparison
                        std::string nameA = a->name;
                        std::string nameB = b->name;
                        std::transform(nameA.begin(), nameA.end(), 
                                      nameA.begin(), ::tolower);
                        std::transform(nameB.begin(), nameB.end(), 
                                      nameB.begin(), ::tolower);
                        return nameA < nameB;
                    });
                break;
            case ProjectSortOrder::ZtoA:
                std::sort(sortedProjects.begin(), sortedProjects.end(),
                    [](const RecentProject* a, const RecentProject* b) {
                        // Case-insensitive comparison
                        std::string nameA = a->name;
                        std::string nameB = b->name;
                        std::transform(nameA.begin(), nameA.end(), 
                                      nameA.begin(), ::tolower);
                        std::transform(nameB.begin(), nameB.end(), 
                                      nameB.begin(), ::tolower);
                        return nameA > nameB;
                    });
                break;
        }
        
        // Render projects in a grid (with search filtering)
        int visibleIndex = 0;
        for (size_t i = 0; i < sortedProjects.size(); ++i) {
            auto& project = *sortedProjects[i];
            
            // Apply search filter
            if (searchFilter[0] != '\0') {
                std::string lowerName = project.name;
                std::string lowerFilter = searchFilter;
                
                // Convert to lowercase for case-insensitive search
                std::transform(lowerName.begin(), lowerName.end(), 
                              lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), 
                              lowerFilter.begin(), ::tolower);
                
                // Skip if doesn't match search
                if (lowerName.find(lowerFilter) == std::string::npos) {
                    continue;
                }
            }
            
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
                    showProjectBrowserModal = false;
                    app.OpenProject(project.path);
                    app.ShowEditor();
                }
                
                ImGui::PopStyleColor(3);
                
                // Tooltip on hover
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    
                    // Truncated description (50 chars max)
                    if (!project.description.empty()) {
                        std::string desc = project.description;
                        if (desc.length() > 50) {
                            desc = desc.substr(0, 47) + "...";
                        }
                        ImGui::TextWrapped("%s", desc.c_str());
                        ImGui::Spacing();
                    }
                    
                    // Path (dimmed)
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                        "%s", project.path.c_str());
                    
                    // Last modified
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
                ImVec2 textPos(cardPos.x + 8, cardPos.y + thumbnailHeight - 
                              titleHeight + 4);
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
                    const float xButtonSize = 16.0f;
                    const float xButtonMarginX = -2.0f;
                    const float xButtonMarginY = 5.0f;
                    ImVec2 xButtonPos(cardPos.x + cardWidth - xButtonSize - 
                                     xButtonMarginX,
                                     cardPos.y + xButtonMarginY);
                    ImGui::SetCursorScreenPos(xButtonPos);
                    
                    // Style for X button
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
                        showProjectActionModal = true;
                        projectActionPath = project.path;
                        projectActionName = project.name;
                    }
                    
                    // Draw X icon manually
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
            
            // Grid layout: add spacing or new line
            // Use visibleIndex for grid layout (not original index i)
            visibleIndex++;
            if (visibleIndex % cardsPerRow != 0) {
                ImGui::SameLine(0.0f, cardSpacing);
            } else {
                ImGui::Spacing();
            }
        }
        
        // Show "no results" message if search yielded nothing
        if (searchFilter[0] != '\0' && visibleIndex == 0) {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "No projects found matching \"%s\"", searchFilter);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "Try a different search term");
        }
        
        ImGui::EndChild();
        
        // Close button at bottom
        ImGui::Spacing();
        float closeButtonWidth = 120.0f;
        float closeButtonX = (ImGui::GetWindowWidth() - closeButtonWidth) * 0.5f;
        ImGui::SetCursorPosX(closeButtonX);
        
        if (ImGui::Button("Close", ImVec2(closeButtonWidth, 0))) {
            showProjectBrowserModal = false;
            projectBrowserModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Reset flag if modal was closed (via X button)
    if (!showProjectBrowserModal) {
        projectBrowserModalOpened = false;
    }
    
    // Render project action modal if opened from browser
    if (showProjectActionModal) {
        RenderProjectActionModal(recentProjects);
    }
}


} // namespace Cartograph
