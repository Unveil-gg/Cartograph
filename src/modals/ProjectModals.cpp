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

void Modals::RenderNewProjectModal(App& app, Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!newProjectModalOpened) {
        ImGui::OpenPopup("New Project");
        newProjectModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New Project", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Project name
        ImGui::Text("Project Name:");
        if (ImGui::InputText("##projectname", newProjectConfig.projectName, 
            sizeof(newProjectConfig.projectName))) {
            // Update path when project name changes
            UpdateNewProjectPath();
        }
        
        ImGui::Spacing();
        
        // Save location
        ImGui::Text("Save Location:");
        
        // Display path in a framed box (styled like input field)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, 
                             ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("##savelocationdisplay", 
                         ImVec2(450, 30), 
                         true, 
                         ImGuiWindowFlags_NoScrollbar);
        
        ImGui::PushStyleColor(ImGuiCol_Text, 
                             ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::TextWrapped("%s", newProjectConfig.fullSavePath.c_str());
        ImGui::PopStyleColor();
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Choose Different Location...", 
                         ImVec2(240, 0))) {
            ShowNewProjectFolderPicker();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map Style Selection
        ImGui::Text("Choose your map style:");
        ImGui::Spacing();
        
        // Grid preset cards - side by side
        ImGui::BeginGroup();
        
        // Square preset card
        bool isSquareSelected = 
            (newProjectConfig.gridPreset == GridPreset::Square);
        if (isSquareSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                                 ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                                 ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Square\n16×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Square;
        }
        ImGui::PopStyleVar();
        
        if (isSquareSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 20);
        
        // Rectangle preset card
        bool isRectangleSelected = 
            (newProjectConfig.gridPreset == GridPreset::Rectangle);
        if (isRectangleSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                                 ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                                 ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Rectangle\n32×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Rectangle;
        }
        ImGui::PopStyleVar();
        
        if (isRectangleSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::EndGroup();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map dimensions with validation
        ImGui::PushItemWidth(150.0f);
        ImGui::InputInt("Map Width (cells)", &newProjectConfig.mapWidth);
        if (newProjectConfig.mapWidth < 16) newProjectConfig.mapWidth = 16;
        if (newProjectConfig.mapWidth > 1024) 
            newProjectConfig.mapWidth = 1024;
        
        ImGui::InputInt("Map Height (cells)", &newProjectConfig.mapHeight);
        if (newProjectConfig.mapHeight < 16) 
            newProjectConfig.mapHeight = 16;
        if (newProjectConfig.mapHeight > 1024) 
            newProjectConfig.mapHeight = 1024;
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        
        // Preview info
        int totalCells = newProjectConfig.mapWidth * 
            newProjectConfig.mapHeight;
        
        // Get cell dimensions from preset
        int cellWidth = 
            (newProjectConfig.gridPreset == GridPreset::Square) ? 16 : 32;
        int cellHeight = 16;
        
        int pixelWidth = newProjectConfig.mapWidth * cellWidth;
        int pixelHeight = newProjectConfig.mapHeight * cellHeight;
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Total cells: %d | Canvas size: %dx%d px",
            totalCells, pixelWidth, pixelHeight);
        
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Validate path
            if (newProjectConfig.fullSavePath.empty()) {
                m_ui.ShowToast("Please select a save location", 
                         Toast::Type::Error);
            } else {
                showNewProjectModal = false;
                newProjectModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                // Create project with configuration
                // NewProject() handles InitDefaults() + applies config
                app.NewProject(
                    newProjectConfig.fullSavePath,
                    std::string(newProjectConfig.projectName),
                    newProjectConfig.gridPreset,
                    newProjectConfig.mapWidth,
                    newProjectConfig.mapHeight
                );
                
                // Transition to editor
                app.ShowEditor();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProjectModal = false;
            newProjectModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

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

void Modals::RenderProjectActionModal(
    std::vector<RecentProject>& recentProjects) {
    // Only call OpenPopup once when modal is first shown
    if (!projectActionModalOpened) {
        ImGui::OpenPopup("Project Options");
        projectActionModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Project Options", &showProjectActionModal,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        
        // Project name (prominent)
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                          "Project: %s", projectActionName.c_str());
        
        // Project path (dimmed, wrapped for long paths)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("%s", projectActionPath.c_str());
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Prompt
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                          "Choose an action:");
        ImGui::Spacing();
        
        // Delete Project button (destructive)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        
        if (ImGui::Button("Delete Project", ImVec2(-1, 0))) {
            // Show confirmation popup
            ImGui::OpenPopup("Confirm Delete");
        }
        
        ImGui::PopStyleColor(3);
        
        // Confirmation popup for delete (centered)
        ImVec2 confirmCenter = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(confirmCenter, ImGuiCond_Always, 
                               ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Are you sure you want to delete this project?");
            ImGui::Spacing();
            ImGui::TextWrapped("This will permanently delete the project "
                             "files from disk. This cannot be undone.");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "%s", projectActionPath.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Button, 
                                 ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                 ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                 ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete Permanently", ImVec2(140, 0))) {
                // Delete from filesystem
                namespace fs = std::filesystem;
                std::error_code ec;
                
                if (fs::exists(projectActionPath, ec)) {
                    if (fs::is_directory(projectActionPath, ec)) {
                        fs::remove_all(projectActionPath, ec);
                    } else {
                        fs::remove(projectActionPath, ec);
                    }
                }
                
                // Remove from recent projects
                RecentProjects::Remove(projectActionPath);
                
                // Remove from in-memory list
                auto it = std::remove_if(recentProjects.begin(), 
                    recentProjects.end(),
                    [this](const RecentProject& p) {
                        return p.path == projectActionPath;
                    });
                recentProjects.erase(it, recentProjects.end());
                
                m_ui.ShowToast("Project deleted", Toast::Type::Info);
                
                showProjectActionModal = false;
                projectActionModalOpened = false;
                ImGui::CloseCurrentPopup();
                ImGui::CloseCurrentPopup();  // Close both popups
            }
            
            ImGui::PopStyleColor(3);
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel##delete", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::Spacing();
        
        // Remove from list button (non-destructive)
        if (ImGui::Button("Remove from List", ImVec2(-1, 0))) {
            // Remove from recent projects only (don't delete files)
            RecentProjects::Remove(projectActionPath);
            
            // Remove from in-memory list
            auto it = std::remove_if(recentProjects.begin(), 
                recentProjects.end(),
                [this](const RecentProject& p) {
                    return p.path == projectActionPath;
                });
            recentProjects.erase(it, recentProjects.end());
            
            m_ui.ShowToast("Project removed from list", Toast::Type::Info);
            
            showProjectActionModal = false;
            projectActionModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Remove from recent projects list\n"
                            "(project files are not deleted)");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            showProjectActionModal = false;
            projectActionModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Reset flag if modal was closed
    if (!showProjectActionModal) {
        projectActionModalOpened = false;
    }
}

void Modals::RenderSaveBeforeActionModal(App& app, Model& model) {
    const char* actionName = "";
    switch (pendingAction) {
        case PendingAction::NewProject:
            actionName = "creating a new project";
            break;
        case PendingAction::OpenProject:
            actionName = "opening a project";
            break;
        default:
            actionName = "continuing";
            break;
    }
    
    // Only call OpenPopup once when modal is first shown
    if (!saveBeforeActionModalOpened) {
        ImGui::OpenPopup("Unsaved Changes");
        saveBeforeActionModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Warning: Unsaved Changes");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "You have unsaved changes. Do you want to save your work "
            "before %s?", actionName);
        ImGui::Spacing();
        
        // Show current project name if available
        if (!app.GetCurrentFilePath().empty()) {
            // Extract filename from path
            std::string filename = app.GetCurrentFilePath();
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            ImGui::TextDisabled("Current project: %s", filename.c_str());
        } else {
            ImGui::TextDisabled("Current project: Untitled");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            pendingAction = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            // Execute the pending action without saving
            if (pendingAction == PendingAction::NewProject) {
                app.ShowNewProjectDialog();
            } else if (pendingAction == PendingAction::OpenProject) {
                app.ShowOpenProjectDialog();
            }
            
            pendingAction = PendingAction::None;
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.15f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            // Save then execute the pending action
            app.SaveProject();
            
            // Only proceed if save succeeded (model should be clean now)
            if (!model.dirty) {
                if (pendingAction == PendingAction::NewProject) {
                    app.ShowNewProjectDialog();
                } else if (pendingAction == PendingAction::OpenProject) {
                    app.ShowOpenProjectDialog();
                }
            }
            
            pendingAction = PendingAction::None;
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}

void Modals::ApplyTemplate(ProjectTemplate tmpl) {
    switch (tmpl) {
        case ProjectTemplate::Small:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 128;
            newProjectConfig.mapHeight = 128;
            break;
            
        case ProjectTemplate::Medium:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Large:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 512;
            newProjectConfig.mapHeight = 512;
            break;
            
        case ProjectTemplate::Metroidvania:
            newProjectConfig.gridPreset = GridPreset::Rectangle;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Custom:
        default:
            // Keep current settings
            break;
    }
}

void Modals::UpdateNewProjectPath() {
    // Use centralized sanitization helper
    std::string sanitized = ProjectFolder::SanitizeProjectName(
        newProjectConfig.projectName);
    
    // Build full path with .cartproj extension
    if (!newProjectConfig.saveDirectory.empty()) {
        // Ensure directory path ends with separator
        std::string dir = newProjectConfig.saveDirectory;
        if (dir.back() != '/' && dir.back() != '\\') {
#ifdef _WIN32
            dir += '\\';
#else
            dir += '/';
#endif
        }
        
        // Append .cartproj extension for new projects
        newProjectConfig.fullSavePath = dir + sanitized + CARTPROJ_EXTENSION;
    }
}

void Modals::ShowNewProjectFolderPicker() {
    // Callback struct to capture Modals reference
    struct CallbackData {
        Modals* modals;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->modals = this;
    
    // Show native folder picker dialog
    SDL_ShowOpenFolderDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->modals->m_ui.ShowToast("Failed to open folder dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled - keep existing path
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a folder
            std::string folderPath = filelist[0];
            
            // Update save directory and regenerate full path
            data->modals->newProjectConfig.saveDirectory = folderPath;
            data->modals->UpdateNewProjectPath();
            
            // unique_ptr cleans up automatically at end of scope
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Default location - start with current directory
        newProjectConfig.saveDirectory.empty() 
            ? nullptr 
            : newProjectConfig.saveDirectory.c_str(),
        // Allow multiple folders
        false
    );
}

} // namespace Cartograph
