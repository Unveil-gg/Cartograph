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

WelcomeScreen::WelcomeScreen(UI& ui) : m_ui(ui) {
    // Initialize welcome screen
}

WelcomeScreen::~WelcomeScreen() {
    UnloadThumbnailTextures();
}

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
            // User selected a file or folder - start async loading
            std::string filePath = *result;
            
            m_ui.m_modals.showLoadingModal = true;
            m_ui.m_modals.loadingFilePath = filePath;
            m_ui.m_modals.loadingCancelled = false;
            m_ui.m_modals.loadingStartTime = Platform::GetTime();
            
            // Extract filename for display
            size_t lastSlash = filePath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                m_ui.m_modals.loadingFileName = filePath.substr(lastSlash + 1);
            } else {
                m_ui.m_modals.loadingFileName = filePath;
            }
            
            // Create shared model to load into
            auto loadedModel = std::make_shared<Model>();
            auto tempIcons = std::make_shared<IconManager>();
            
            // Enqueue async loading job
            jobs.Enqueue(
                JobType::LoadProject,
                [filePath, loadedModel, tempIcons, this]() {
                    // Check for cancellation before starting
                    if (m_ui.m_modals.loadingCancelled) {
                        throw std::runtime_error("Cancelled by user");
                    }
                    
                    bool success = false;
                    bool isCartFile = (filePath.size() >= 5 && 
                                      filePath.substr(filePath.size() - 5) == ".cart");
                    if (isCartFile) {
                        success = Package::Load(filePath, *loadedModel, 
                                              tempIcons.get());
                    } else {
                        success = ProjectFolder::Load(filePath, *loadedModel,
                                                     tempIcons.get());
                    }
                    
                    if (!success) {
                        throw std::runtime_error("Failed to load project");
                    }
                },
                [this, &app, loadedModel, tempIcons, &icons, filePath](
                    bool success, const std::string& error) {
                    m_ui.m_modals.showLoadingModal = false;
                    
                    if (success && !m_ui.m_modals.loadingCancelled) {
                        // Swap loaded model into app (main thread safe)
                        app.OpenProject(filePath);
                        
                        // Merge loaded icons into main icon manager
                        icons.BuildAtlas();  // Rebuild atlas on main thread
                        
                        // Transition to editor
                        app.ShowEditor();
                        
                        m_ui.ShowToast("Project loaded", Toast::Type::Success);
                    } else if (m_ui.m_modals.loadingCancelled) {
                        m_ui.ShowToast("Loading cancelled", Toast::Type::Info);
                    } else {
                        m_ui.ShowToast("Failed to load project: " + error, 
                                 Toast::Type::Error);
                    }
                }
            );
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
        m_ui.m_canvasPanel.selectedMarker,
        m_ui.m_canvasPanel.selectedTileId
    );
    
    // Render project browser modal separately (needs recentProjects)
    if (m_ui.m_modals.showProjectBrowserModal) {
        m_ui.m_modals.RenderProjectBrowserModal(app, recentProjects);
    }
    
    // Render toasts
    m_ui.RenderToasts(0.016f);
}


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
        }
        
        ImGui::EndGroup();
        ImGui::PopID();
        
        // Add spacing between cards (but not after last card)
        if (i < maxDisplay - 1) {
            ImGui::SameLine(0.0f, cardSpacing);
        }
    }
}


/**
 * Extract thumbnail from a .cart file to cache directory.
 * @param cartPath Path to .cart file
 * @return Path to extracted thumbnail, or empty if extraction failed
 */
static std::string ExtractCartThumbnail(const std::string& cartPath) {
    namespace fs = std::filesystem;
    
    // Create cache directory for thumbnails
    std::string cacheDir = Platform::GetUserDataDir() + "thumbnail_cache/";
    Platform::EnsureDirectoryExists(cacheDir);
    
    // Generate cache filename based on cart file path hash
    // (Simple approach: use filename + modification time)
    fs::path cartFsPath(cartPath);
    std::string baseName = cartFsPath.stem().string();
    
    // Get file modification time for cache invalidation
    std::error_code ec;
    auto modTime = fs::last_write_time(cartPath, ec);
    if (ec) return "";
    
    auto modTimeVal = static_cast<long long>(
        modTime.time_since_epoch().count());
    std::string cacheFileName = baseName + "_" + 
        std::to_string(modTimeVal) + ".png";
    std::string cachePath = cacheDir + cacheFileName;
    
    // Check if cached thumbnail already exists and is valid
    if (fs::exists(cachePath)) {
        return cachePath;
    }
    
    // Extract thumb.png from .cart ZIP
    unzFile uf = unzOpen(cartPath.c_str());
    if (!uf) return "";
    
    // Try to locate thumb.png
    if (unzLocateFile(uf, "thumb.png", 0) != UNZ_OK) {
        unzClose(uf);
        return "";
    }
    
    // Get file info
    unz_file_info fileInfo;
    if (unzGetCurrentFileInfo(uf, &fileInfo, nullptr, 0, 
                              nullptr, 0, nullptr, 0) != UNZ_OK) {
        unzClose(uf);
        return "";
    }
    
    // Open and read the file
    if (unzOpenCurrentFile(uf) != UNZ_OK) {
        unzClose(uf);
        return "";
    }
    
    std::vector<uint8_t> buffer(fileInfo.uncompressed_size);
    int bytesRead = unzReadCurrentFile(uf, buffer.data(), buffer.size());
    unzCloseCurrentFile(uf);
    unzClose(uf);
    
    if (bytesRead <= 0) return "";
    
    // Write to cache file
    std::ofstream outFile(cachePath, std::ios::binary);
    if (!outFile.is_open()) return "";
    
    outFile.write(reinterpret_cast<const char*>(buffer.data()), bytesRead);
    outFile.close();
    
    return cachePath;
}

/**
 * Create RecentProject entry from a project folder path.
 * @param folderPath Path to project folder
 * @param lastModified Optional last modified string (empty = use file time)
 * @return Populated RecentProject entry
 */
static RecentProject CreateProjectEntryFromFolder(
    const std::string& folderPath,
    const std::string& lastModified = ""
) {
    namespace fs = std::filesystem;
            
            RecentProject project;
    project.path = folderPath;
    
    fs::path p(folderPath);
    project.name = p.filename().string();
            
            // Set thumbnail path if preview.png exists
    fs::path previewPath = p / "preview.png";
            if (fs::exists(previewPath)) {
                project.thumbnailPath = previewPath.string();
            }
            
            // Read description from project.json
    fs::path projectJsonPath = p / "project.json";
            if (fs::exists(projectJsonPath)) {
                try {
                    std::ifstream file(projectJsonPath);
                    if (file.is_open()) {
                        nlohmann::json j = nlohmann::json::parse(file);
                        if (j.contains("meta") && 
                            j["meta"].contains("description")) {
                            project.description = 
                                j["meta"]["description"].get<std::string>();
                        }
                    }
                } catch (...) {
            // Silently ignore parse errors
        }
    }
    
    // Set last modified time
    if (!lastModified.empty()) {
        project.lastModified = lastModified;
    } else {
        // Get from filesystem
        std::error_code ec;
        auto modTime = fs::last_write_time(folderPath, ec);
        if (!ec) {
            auto sctp = std::chrono::time_point_cast<
                std::chrono::system_clock::duration>(
                modTime - fs::file_time_type::clock::now() + 
                std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm = *std::localtime(&time_t);
            char timeStr[64];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tm);
            project.lastModified = timeStr;
        }
    }
    
    return project;
}

void WelcomeScreen::LoadRecentProjects() {
    recentProjects.clear();
    
    namespace fs = std::filesystem;
    
    // Track paths we've already added (for deduplication)
    std::unordered_set<std::string> addedPaths;
    
    // 1. Load from persistent recent projects list
    auto entries = RecentProjects::GetValidEntries();
    
    for (const auto& entry : entries) {
        RecentProject project;
        project.path = entry.path;
        
        bool isCartFile = (entry.type == "cart");
        
        if (isCartFile) {
            // .cart file - extract name from filename
            fs::path p(entry.path);
            project.name = p.stem().string();
            
            // Extract thumbnail from .cart to cache
            project.thumbnailPath = ExtractCartThumbnail(entry.path);
            
            project.description = "";
            
        } else {
            // Project folder
            project = CreateProjectEntryFromFolder(entry.path, entry.lastOpened);
        }
        
        project.lastModified = entry.lastOpened;
        recentProjects.push_back(project);
        addedPaths.insert(entry.path);
    }
    
    // 2. Scan default projects directory for additional projects
    std::string projectsDir = Platform::GetDefaultProjectsDir();
    
    if (fs::exists(projectsDir) && fs::is_directory(projectsDir)) {
        try {
            for (const auto& dirEntry : fs::directory_iterator(projectsDir)) {
                if (!dirEntry.is_directory()) continue;
                
                std::string folderPath = dirEntry.path().string();
                
                // Skip if already in recent list
                if (addedPaths.count(folderPath) > 0) continue;
                
                // Check if it's a valid project folder
                fs::path projectJson = dirEntry.path() / "project.json";
                if (!fs::exists(projectJson)) continue;
                
                // Add to list
                RecentProject project = CreateProjectEntryFromFolder(folderPath);
                recentProjects.push_back(project);
                addedPaths.insert(folderPath);
            }
        } catch (...) {
            // Silently ignore scan errors
        }
    }
    
    // 3. Sort by last modified (most recent first)
    std::sort(recentProjects.begin(), recentProjects.end(),
        [](const RecentProject& a, const RecentProject& b) {
            return a.lastModified > b.lastModified;
        });
}


void WelcomeScreen::AddRecentProject(const std::string& path) {
    // Add to persistent recent projects list
    RecentProjects::Add(path);
}


void WelcomeScreen::LoadThumbnailTexture(RecentProject& project) {
    // Skip if already loaded
    if (project.thumbnailLoaded) {
        return;
    }
    
    namespace fs = std::filesystem;
    
    // Check if thumbnail path exists
    if (project.thumbnailPath.empty() || 
        !fs::exists(project.thumbnailPath)) {
        // Use placeholder
        if (placeholderTexture == 0) {
            placeholderTexture = GeneratePlaceholderTexture();
        }
        project.thumbnailTextureId = placeholderTexture;
        project.thumbnailLoaded = true;
        return;
    }
    
    // Load image from file
    int width, height, channels;
    unsigned char* data = stbi_load(
        project.thumbnailPath.c_str(),
        &width, &height, &channels, 4  // Force RGBA
    );
    
    if (!data) {
        // Failed to load - use placeholder
        if (placeholderTexture == 0) {
            placeholderTexture = GeneratePlaceholderTexture();
        }
        project.thumbnailTextureId = placeholderTexture;
        project.thumbnailLoaded = true;
        return;
    }
    
    // Create OpenGL texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );
    
    // Unbind and cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    
    project.thumbnailTextureId = texId;
    project.thumbnailLoaded = true;
}


void WelcomeScreen::UnloadThumbnailTextures() {
    // Unload all thumbnail textures
    for (auto& project : recentProjects) {
        if (project.thumbnailLoaded && 
            project.thumbnailTextureId != 0 &&
            project.thumbnailTextureId != placeholderTexture) {
            GLuint texId = project.thumbnailTextureId;
            glDeleteTextures(1, &texId);
            project.thumbnailTextureId = 0;
            project.thumbnailLoaded = false;
        }
    }
    
    // Delete placeholder texture
    if (placeholderTexture != 0) {
        GLuint texId = placeholderTexture;
        glDeleteTextures(1, &texId);
        placeholderTexture = 0;
    }
}


unsigned int WelcomeScreen::GeneratePlaceholderTexture() {
    // Generate a gradient/grid pattern for missing thumbnails
    const int width = 384;
    const int height = 216;
    const int gridSize = 16;  // Grid cell size in pixels
    
    std::vector<uint8_t> pixels(width * height * 4);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            
            // Create a subtle grid pattern
            bool onGridLine = (x % gridSize == 0) || (y % gridSize == 0);
            
            // Gradient from dark gray to darker gray
            float gradientFactor = 1.0f - (y / static_cast<float>(height)) * 0.3f;
            uint8_t baseColor = static_cast<uint8_t>(40 * gradientFactor);
            uint8_t gridColor = static_cast<uint8_t>(60 * gradientFactor);
            
            if (onGridLine) {
                pixels[idx + 0] = gridColor;      // R
                pixels[idx + 1] = gridColor;      // G
                pixels[idx + 2] = gridColor;      // B
            } else {
                pixels[idx + 0] = baseColor;      // R
                pixels[idx + 1] = baseColor;      // G
                pixels[idx + 2] = baseColor;      // B
            }
            pixels[idx + 3] = 255;                // A
        }
    }
    
    // Create OpenGL texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );
    
    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texId;
}


} // namespace Cartograph
