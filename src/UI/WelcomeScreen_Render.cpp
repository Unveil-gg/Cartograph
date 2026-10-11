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


void WelcomeScreen::Render(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    JobQueue& jobs,
    IconManager& icons,
    KeymapManager& keymap
) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;
    
    ImGui::Begin("CartographWelcome", nullptr, windowFlags);
    
    // Center content horizontally and vertically
    ImVec2 windowSize = ImGui::GetWindowSize();
    float contentHeight = 480.0f;  // Height for treasure map parchment
    
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Monospace
    
    // Treasure map scroll with parchment texture
    const char* mapLine1 = 
        "####++++++++++++++++++++++++++++++++++++++++++++++++++++####";
    const char* mapLine2 = 
        "###++++++++++--------+--------++++++---------++++++++++++###";
    const char* mapLine3 = 
        "##+++++-------+----+----+----------+----+-------+--++++++##";
    const char* mapLine4 = 
        "##++++---+-------+----------+------+----------+------++++##";
    const char* mapLine5 = 
        "##++++------+----------++----------+------+----------++++##";
    const char* mapLine6 = 
        "##++++---+--------++----+------+------+--+------------+++##";
    const char* mapLine7 = 
        "##+++-------+----------+------+--------+------+-------+++##";
    
    // Cartograph ASCII integrated into map
    const char* titleLine1 = 
        "##+++   ___          _                             _  +++##";
    const char* titleLine2 = 
        "##++   / __\\__ _ _ _| |_ ___   __ _ _ __ __ _ _ __| |__++##";
    const char* titleLine3 = 
        "##++  / /  / _` | '__| __/ _ \\ / _` | '__/ _` | '_ \\ '_ ++##";
    const char* titleLine4 = 
        "##++ / /__| (_| | |  | || (_) | (_| | | | (_| | |_) | |+++##";
    const char* titleLine5 = 
        "##++ \\____/\\__,_|_|   \\__\\___/ \\__, |_|  \\__,_| .__/|_|+++##";
    const char* titleLine6 = 
        "##+++                          |___/          |_|      +++##";
    
    // Bottom map texture
    const char* mapLine8 = 
        "##++++------+----------+------+--------+------+-------+++##";
    const char* mapLine9 = 
        "##++++---+--------++----+------+------+--+------------+++##";
    const char* mapLine10 = 
        "##+++++------+----------++----------+------+----------+++##";
    const char* mapLine11 = 
        "##++++++---+-------+----------+------+----------+---+++++##";
    const char* mapLine12 = 
        "###+++++++++-------+----+----+----------+----+--+++++++++##";
    const char* mapLine13 = 
        "####++++++++++++++--------+--------++++++---------+++++####";
    
    // Calculate centering based on longest line
    float scrollWidth = ImGui::CalcTextSize(mapLine1).x;
    float scrollStartX = (windowSize.x - scrollWidth) * 0.5f;
    // Center vertically when no projects, top-bias when projects exist below
    float verticalFactor = recentProjects.empty() ? 0.5f : 0.2f;
    float scrollStartY = (windowSize.y - contentHeight) * verticalFactor;
    
    ImGui::SetCursorPos(ImVec2(scrollStartX, scrollStartY));
    ImGui::BeginGroup();
    
    // Top map texture (brown/parchment color)
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine1);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine2);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine3);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine4);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine5);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine6);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine7);
    
    // Cartograph title (blue text on parchment)
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine1);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine2);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine3);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine4);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine5);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", titleLine6);
    
    // Bottom map texture
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine8);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine9);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine10);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine11);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine12);
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.3f, 1.0f), "%s", mapLine13);
    
    ImGui::PopFont();
    
    ImGui::Spacing();
    
    // Subtitle - centered
    const char* subtitle = 
        "M e t r o i d v a n i a   M a p   E d i t o r";
    float subtitleWidth = ImGui::CalcTextSize(subtitle).x;
    float subtitleStartX = (windowSize.x - subtitleWidth) * 0.5f;
    
    ImGui::SetCursorPosX(subtitleStartX);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", subtitle);
    
    // Add spacing between ASCII art and buttons
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Main action buttons - side by side, centered
    float buttonWidth = 200.0f;
    float buttonHeight = 50.0f;
    float buttonSpacing = 16.0f;
    float buttonsStartX = (windowSize.x - (buttonWidth * 2 + 
                                           buttonSpacing)) * 0.5f;
    
    ImGui::SetCursorPosX(buttonsStartX);
    if (ImGui::Button("Create New Project", ImVec2(buttonWidth, 
                                                    buttonHeight))) {
        m_ui.m_modals.showNewProjectModal = true;
        // Reset config to defaults
        std::strncpy(m_ui.m_modals.newProjectConfig.projectName, "New Map",
                     sizeof(m_ui.m_modals.newProjectConfig.projectName) - 1);
        m_ui.m_modals.newProjectConfig.projectName[
            sizeof(m_ui.m_modals.newProjectConfig.projectName) - 1] = '\0';
        m_ui.m_modals.newProjectConfig.gridPreset = GridPreset::Square;
        m_ui.m_modals.newProjectConfig.mapWidth = 100;
        m_ui.m_modals.newProjectConfig.mapHeight = 100;
        
        // Initialize default save path
        m_ui.m_modals.newProjectConfig.saveDirectory = Platform::GetDefaultProjectsDir();
        m_ui.m_modals.UpdateNewProjectPath();
        
        // Ensure default directory exists
        Platform::EnsureDirectoryExists(m_ui.m_modals.newProjectConfig.saveDirectory);
    }
    
    ImGui::SameLine(0.0f, buttonSpacing);
    if (ImGui::Button("Import Project", ImVec2(buttonWidth, 
                                                buttonHeight))) {
        // Show native file picker
        auto result = Platform::ShowOpenDialogForImport(
            "Import Cartograph Project",
            true,  // Allow files
            true,  // Allow folders
            {"cart"},  // File extensions
            Platform::GetDefaultProjectsDir()  // Default path
        );
        
        if (result) {
            m_ui.StartAsyncProjectLoad(*result, app, jobs);
        }
        // If result is nullopt, user cancelled - no action needed
    }
    
    // Hover tooltip
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();
        ImGui::Text("Click to browse or drag & drop");
        ImGui::EndTooltip();
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Recent Projects Section
    if (!recentProjects.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        
        // "Recent Projects" header - centered
        const char* headerText = "Recent Projects";
        float headerWidth = ImGui::CalcTextSize(headerText).x;
        float headerStartX = (windowSize.x - headerWidth) * 0.5f;
        
        ImGui::SetCursorPosX(headerStartX);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", headerText);
        ImGui::Spacing();
        ImGui::Spacing();
        
        // 3-card horizontal display
        RenderRecentProjectsList(app);
        
        // "View more" button if there are more than 3 projects
        if (recentProjects.size() > 3) {
            ImGui::Spacing();
            ImGui::Spacing();
            
            float viewMoreWidth = 120.0f;
            float viewMoreStartX = (windowSize.x - viewMoreWidth) * 0.5f;
            ImGui::SetCursorPosX(viewMoreStartX);
            
            if (ImGui::Button("View more...", ImVec2(viewMoreWidth, 0))) {
                m_ui.m_modals.showProjectBrowserModal = true;
            }
        }
    }
    
    ImGui::EndGroup();
    
    // Version in bottom left corner
    ImGui::SetCursorPos(ImVec2(10, windowSize.y - 20));
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "v%s", CARTOGRAPH_VERSION);
    
    // What's New button in corner
    ImGui::SetCursorPos(ImVec2(windowSize.x - 140, windowSize.y - 40));
    if (ImGui::Button("What's New?", ImVec2(120, 30))) {
        m_ui.m_modals.showWhatsNew = !m_ui.m_modals.showWhatsNew;
    }
    
    // Drag & drop overlay (only when actively dragging)
    if (app.IsDragging()) {
        // Semi-transparent overlay covering entire window
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowEnd = ImVec2(
            windowPos.x + windowSize.x,
            windowPos.y + windowSize.y
        );
        
        // Dark overlay with 30% opacity
        drawList->AddRectFilled(
            windowPos,
            windowEnd,
            IM_COL32(20, 20, 25, 76)  // Dark grey, ~30% alpha
        );
        
        // Blue border glow (pulsing animation)
        float time = static_cast<float>(ImGui::GetTime());
        float pulseAlpha = 0.8f + 0.2f * sinf(time * 3.0f);  // 0.8-1.0
        int alphaValue = static_cast<int>(pulseAlpha * 255);
        
        drawList->AddRect(
            ImVec2(windowPos.x + 10, windowPos.y + 10),
            ImVec2(windowEnd.x - 10, windowEnd.y - 10),
            IM_COL32(74, 144, 226, alphaValue),  // Blue #4A90E2
            4.0f,   // Rounding
            0,      // Flags
            3.0f    // Thickness
        );
        
        // Centered text: "↓ Drop to import project"
        const char* dropText = "Drop to import project";
        ImVec2 textSize = ImGui::CalcTextSize(dropText);
        ImVec2 textPos = ImVec2(
            windowPos.x + (windowSize.x - textSize.x) * 0.5f,
            windowPos.y + (windowSize.y - textSize.y) * 0.5f
        );
        
        // Text shadow for better visibility
        drawList->AddText(
            ImVec2(textPos.x + 2, textPos.y + 2),
            IM_COL32(0, 0, 0, 180),
            dropText
        );
        
        // Main text (white, pulsing slightly)
        drawList->AddText(
            textPos,
            IM_COL32(255, 255, 255, alphaValue),
            dropText
        );
    }
    
    ImGui::End();
    
    // Render modal dialogs (most modals)
    m_ui.m_modals.RenderAll(
        app, model, canvas, history, icons, jobs, keymap,
        m_ui.m_canvasPanel.selectedIconName,
        m_ui.m_canvasPanel.selectedMarkerId,
        m_ui.m_canvasPanel.selectedTileId
    );
    
    // Render project browser modal separately (needs recentProjects)
    if (m_ui.m_modals.showProjectBrowserModal) {
        m_ui.m_modals.RenderProjectBrowserModal(app, recentProjects);
    }
    
    // Render project action modal (delete/remove)
    if (m_ui.m_modals.showProjectActionModal) {
        m_ui.m_modals.RenderProjectActionModal(recentProjects);
    }
    
    // Render toasts
    m_ui.RenderToasts(0.016f);
}

} // namespace Cartograph

