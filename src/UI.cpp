#include "UI.h"
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
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
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
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>

// OpenGL for texture loading
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

// SDL_CursorDeleter implementation (outside namespace)
void SDL_CursorDeleter::operator()(SDL_Cursor* cursor) const {
    if (cursor) {
        SDL_DestroyCursor(cursor);
    }
}

namespace Cartograph {

UI::UI() {
    // Connect canvas panel to UI state for shared members
    m_canvasPanel.showPropertiesPanel = &showPropertiesPanel;
    m_canvasPanel.layoutInitialized = &m_layoutInitialized;
}

UI::~UI() {
    UnloadThumbnailTextures();
}

void UI::SetupDockspace() {
    // Docking setup happens in first frame of Render()
}

void UI::Render(
    App& app,
    IRenderer& renderer,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs,
    KeymapManager& keymap,
    float deltaTime
) {
    // Render menu bar outside dockspace (ensures it's on top)
    RenderMenuBar(app, model, canvas, history, icons, jobs);
    
    // Global keyboard shortcuts (work even when menus are closed)
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        // Save
        if (keymap.IsActionTriggered("save")) {
            app.SaveProject();
        }
        
        // Save As
        if (keymap.IsActionTriggered("saveAs")) {
            ShowSaveProjectDialog(app);
        }
        
        // Export PNG
        if (keymap.IsActionTriggered("export")) {
            showExportModal = true;
        }
        
        // Export Package
        if (keymap.IsActionTriggered("exportPackage")) {
            ShowExportPackageDialog(app);
        }
    }
    
    // Create fullscreen dockspace window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // WorkPos/WorkSize already account for main menu bar
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("CartographDockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);
    
    // Create dockspace
    ImGuiID dockspaceId = ImGui::GetID("CartographDockSpaceID");
    ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
    
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
    
    // Build fixed layout on first run
    if (!m_layoutInitialized) {
        BuildFixedLayout(dockspaceId);
        m_layoutInitialized = true;
    }
    
    ImGui::End();
    
    // Render all panels (they will dock into the dockspace)
    // Canvas first (background layer), then side panels (on top for tooltips)
    m_canvasPanel.Render(renderer, model, canvas, history, icons, keymap);
    RenderStatusBar(model, canvas);
    RenderToolsPanel(model, history, icons, jobs);
    
    if (showPropertiesPanel) {
        RenderPropertiesPanel(model, icons, jobs);
    }
    
    // Render toasts
    RenderToasts(deltaTime);
    
    // Render modal dialogs
    if (showExportModal) {
        RenderExportModal(model, canvas);
    }
    if (shouldShowExportPngDialog) {
        shouldShowExportPngDialog = false;
        ShowExportPngDialog(app);
    }
    if (showSettingsModal) {
        RenderSettingsModal(model, keymap);
    }
    if (showRenameIconModal) {
        RenderRenameIconModal(model, icons);
    }
    if (showDeleteIconModal) {
        RenderDeleteIconModal(model, icons, history);
    }
    if (showRebindModal) {
        RenderRebindModal(model, keymap);
    }
    if (showColorPickerModal) {
        RenderColorPickerModal(model, history);
    }
    
    // Quit confirmation modal (when closing with unsaved changes)
    if (showQuitConfirmationModal) {
        RenderQuitConfirmationModal(app, model);
    }
    
    // Note: Cursor updates are now handled by CanvasPanel
}

void UI::ShowToast(
    const std::string& message, 
    Toast::Type type,
    float duration
) {
    // Convert Toast::Type to MessageType and add to console
    MessageType msgType;
    switch (type) {
        case Toast::Type::Info:    msgType = MessageType::Info; break;
        case Toast::Type::Success: msgType = MessageType::Success; break;
        case Toast::Type::Warning: msgType = MessageType::Warning; break;
        case Toast::Type::Error:   msgType = MessageType::Error; break;
        default:                   msgType = MessageType::Info; break;
    }
    AddConsoleMessage(message, msgType);
    
    // Legacy toast system (deprecated, will be removed)
    Toast toast;
    toast.message = message;
    toast.type = type;
    toast.remainingTime = duration;
    m_toasts.push_back(toast);
}

void UI::AddConsoleMessage(const std::string& message, MessageType type) {
    double timestamp = Platform::GetTime();
    m_consoleMessages.emplace_back(message, type, timestamp);
    
    // Keep only last MAX_CONSOLE_MESSAGES
    if (m_consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
        m_consoleMessages.erase(m_consoleMessages.begin());
    }
}

void UI::RenderWelcomeScreen(App& app, Model& model, JobQueue& jobs,
                             IconManager& icons) {
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
    float scrollStartY = (windowSize.y - contentHeight) * 0.2f;
    
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
        showNewProjectModal = true;
        // Reset config to defaults
        std::strcpy(newProjectConfig.projectName, "New Map");
        newProjectConfig.gridPreset = GridPreset::Square;
        newProjectConfig.mapWidth = 100;
        newProjectConfig.mapHeight = 100;
        
        // Initialize default save path
        newProjectConfig.saveDirectory = Platform::GetDefaultProjectsDir();
        UpdateNewProjectPath();
        
        // Ensure default directory exists
        Platform::EnsureDirectoryExists(newProjectConfig.saveDirectory);
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
            
            showLoadingModal = true;
            loadingFilePath = filePath;
            loadingCancelled = false;
            loadingStartTime = Platform::GetTime();
            
            // Extract filename for display
            size_t lastSlash = filePath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                loadingFileName = filePath.substr(lastSlash + 1);
            } else {
                loadingFileName = filePath;
            }
            
            // Create shared model to load into
            auto loadedModel = std::make_shared<Model>();
            auto tempIcons = std::make_shared<IconManager>();
            
            // Enqueue async loading job
            jobs.Enqueue(
                JobType::LoadProject,
                [filePath, loadedModel, tempIcons, this]() {
                    // Check for cancellation before starting
                    if (loadingCancelled) {
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
                    showLoadingModal = false;
                    
                    if (success && !loadingCancelled) {
                        // Swap loaded model into app (main thread safe)
                        app.OpenProject(filePath);
                        
                        // Merge loaded icons into main icon manager
                        icons.BuildAtlas();  // Rebuild atlas on main thread
                        
                        // Transition to editor
                        app.ShowEditor();
                        
                        ShowToast("Project loaded", Toast::Type::Success);
                    } else if (loadingCancelled) {
                        ShowToast("Loading cancelled", Toast::Type::Info);
                    } else {
                        ShowToast("Failed to load project: " + error, 
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
                showProjectBrowserModal = true;
            }
        }
    }
    
    ImGui::EndGroup();
    
    // What's New button in corner
    ImGui::SetCursorPos(ImVec2(windowSize.x - 140, windowSize.y - 40));
    if (ImGui::Button("What's New?", ImVec2(120, 30))) {
        showWhatsNew = !showWhatsNew;
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
    
    // Render modal dialogs
    if (showNewProjectModal) {
        RenderNewProjectModal(app, model);
    }
    
    if (showProjectBrowserModal) {
        RenderProjectBrowserModal(app);
    }
    
    if (showWhatsNew) {
        RenderWhatsNewPanel();
    }
    
    if (showAutosaveRecoveryModal) {
        RenderAutosaveRecoveryModal(app, model);
    }
    
    if (showLoadingModal) {
        RenderLoadingModal(app, model, jobs, icons);
    }
    
    // Render toasts
    RenderToasts(0.016f);
}

void UI::RenderMenuBar(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Cmd+N")) {
                // TODO: New project
            }
            if (ImGui::MenuItem("Open...", "Cmd+O")) {
                // TODO: Open project
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Project", "Cmd+S")) {
                app.SaveProject();
            }
            if (ImGui::MenuItem("Save Project As...", "Cmd+Shift+S")) {
                // Save as project folder (git-friendly)
                ShowSaveProjectDialog(app);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Package (.cart)...", 
                               "Cmd+Shift+E")) {
                // Export as .cart package (single file distribution)
                ShowExportPackageDialog(app);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export PNG...", "Cmd+E")) {
                showExportModal = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Cmd+Q")) {
                app.RequestQuit();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = history.CanUndo();
            bool canRedo = history.CanRedo();
            
            if (ImGui::MenuItem("Undo", "Cmd+Z", false, canUndo)) {
                history.Undo(model);
            }
            if (ImGui::MenuItem("Redo", "Cmd+Y", false, canRedo)) {
                history.Redo(model);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Settings...")) {
                showSettingsModal = true;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Assets")) {
            if (ImGui::MenuItem("Import Icon...")) {
                ImportIcon(icons, jobs);
            }
            
            // Show loading indicator if importing
            if (isImportingIcon) {
                ImGui::Separator();
                ImGui::TextDisabled("Importing: %s...", 
                                   importingIconName.c_str());
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Show Properties Panel", "Cmd+P", 
                               showPropertiesPanel)) {
                showPropertiesPanel = !showPropertiesPanel;
                m_layoutInitialized = false;  // Trigger layout rebuild
            }
            ImGui::Separator();
            ImGui::MenuItem("Show Grid", "G", &canvas.showGrid);
            ImGui::Separator();
            if (ImGui::MenuItem("Zoom In", "=")) {
                canvas.SetZoom(canvas.zoom * 1.2f);
            }
            if (ImGui::MenuItem("Zoom Out", "-")) {
                canvas.SetZoom(canvas.zoom / 1.2f);
            }
            if (ImGui::MenuItem("Reset Zoom", "0")) {
                canvas.SetZoom(2.5f);
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void UI::RenderPalettePanel(Model& model) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;
    
    ImGui::Begin("Cartograph/Palette", nullptr, flags);
    
    ImGui::Text("Tile Types");
    ImGui::Separator();
    
    for (const auto& tile : model.palette) {
        ImGui::PushID(tile.id);
        
        bool selected = (m_canvasPanel.selectedTileId == tile.id);
        ImVec4 color = tile.color.ToImVec4();
        
        // Color button
        if (ImGui::ColorButton("##color", color, 0, ImVec2(24, 24))) {
            m_canvasPanel.selectedTileId = tile.id;
        }
        
        ImGui::SameLine();
        
        // Selectable name
        if (ImGui::Selectable(tile.name.c_str(), selected)) {
            m_canvasPanel.selectedTileId = tile.id;
        }
        
        ImGui::PopID();
    }
    
    ImGui::End();
}

void UI::RenderToolsPanel(Model& model, History& history, IconManager& icons, 
                          JobQueue& jobs) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Tools", nullptr, flags);
    
    ImGui::Text("Tools");
    ImGui::Separator();
    
    const char* toolNames[] = {
        "Move", "Select", "Paint", "Erase", "Fill", 
        "Marker", "Eyedropper"
    };
    
    const char* toolIconNames[] = {
        "move", "square-dashed", "paintbrush", "paint-bucket", "eraser",
        "map-pinned", "pipette"
    };
    
    const char* toolShortcuts[] = {
        "V", "S", "B", "E", "F", "M", "I"
    };
    
    // Icon button size and spacing
    const ImVec2 iconButtonSize(36.0f, 36.0f);
    const float iconSpacing = 6.0f;
    const float panelPadding = 8.0f;
    const int TOOLS_PER_ROW = 4;  // Fixed 4-column grid
    
    // Add padding
    ImGui::Dummy(ImVec2(0, panelPadding * 0.5f));
    
    for (int i = 0; i < 7; ++i) {
        bool selected = (static_cast<int>(m_canvasPanel.currentTool) == i);
        
        ImGui::PushID(i);
        
        // Get icon for this tool
        const Icon* icon = icons.GetIcon(toolIconNames[i]);
        
        // Prominent selection highlight
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.36f, 0.69f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.16f, 0.49f, 0.88f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        bool clicked = false;
        
        if (icon) {
            // Draw icon button with UV coordinates
            ImTextureID texId = icons.GetAtlasTexture();
            ImVec2 uv0(icon->u0, icon->v0);
            ImVec2 uv1(icon->u1, icon->v1);
            
            clicked = ImGui::ImageButton(toolIconNames[i], texId, 
                                         iconButtonSize, uv0, uv1);
        } else {
            // Fallback to text button if icon not found
            clicked = ImGui::Button(toolNames[i], iconButtonSize);
        }
        
        // Tooltip on button hover (force to main viewport to avoid clipping)
        if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::SetNextWindowPos(ImVec2(mousePos.x + 15, mousePos.y + 10));
            ImGui::BeginTooltip();
            switch (i) {
                case 0: // Move
                    ImGui::Text("Move Tool [V]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Drag to pan the canvas\nWheel to zoom in/out");
                    break;
                case 1: // Select
                    ImGui::Text("Select Tool [S]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Drag to create selection rectangle");
                    break;
                case 2: // Paint
                    ImGui::Text("Paint Tool [B]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Left-click: Paint with selected color\n"
                        "Right-click: Erase\n"
                        "E+Click: Erase (alternative)");
                    break;
                case 3: // Fill
                    ImGui::Text("Fill Tool [F]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to fill area");
                    break;
                case 4: // Erase
                    ImGui::Text("Erase Tool [E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to erase tiles");
                    break;
                case 5: // Marker
                    ImGui::Text("Marker Tool [M]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to place marker");
                    break;
                case 6: // Eyedropper
                    ImGui::Text("Eyedropper Tool [I]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to pick tile color");
                    break;
                default:
                    ImGui::Text("%s", toolNames[i]);
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Tool not yet implemented");
                    break;
            }
            ImGui::EndTooltip();
        }
        
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        
        if (clicked) {
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(i);
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 6) {
            ImGui::SameLine(0, iconSpacing);
        }
        
        ImGui::PopID();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Show eyedropper preview when Eyedropper tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Eyedropper) {
        ImGui::Text("Eyedropper Tool");
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Hover to preview");
        
        ImGui::Spacing();
        
        // Get hover preview color (what WILL be picked)
        Color hoverColor(0.5f, 0.5f, 0.5f, 1.0f);
        std::string hoverName = "No tile";
        bool isHoveringTile = false;
        
        if (m_canvasPanel.isHoveringCanvas && m_canvasPanel.hoveredTileX >= 0 && m_canvasPanel.hoveredTileY >= 0) {
            const std::string globalRoomId = "";
            int hoveredTileId = model.GetTileAt(
                globalRoomId, m_canvasPanel.hoveredTileX, m_canvasPanel.hoveredTileY
            );
            
            if (hoveredTileId != 0) {
                for (const auto& tile : model.palette) {
                    if (tile.id == hoveredTileId) {
                        hoverColor = tile.color;
                        hoverName = tile.name;
                        isHoveringTile = true;
                        break;
                    }
                }
            }
        }
        
        // If not hovering, show selected color as fallback
        if (!isHoveringTile) {
            for (const auto& tile : model.palette) {
                if (tile.id == m_canvasPanel.selectedTileId) {
                    hoverColor = tile.color;
                    hoverName = tile.name;
                    break;
                }
            }
        }
        
        // Large hover preview
        float previewWidth = ImGui::GetContentRegionAvail().x;
        float hoverHeight = 60.0f;
        
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 hoverMin = cursorPos;
        ImVec2 hoverMax = ImVec2(
            cursorPos.x + previewWidth, 
            cursorPos.y + hoverHeight
        );
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw hover preview
        drawList->AddRectFilled(hoverMin, hoverMax, hoverColor.ToU32());
        
        // Draw border (highlight if hovering tile)
        ImU32 hoverBorderColor = isHoveringTile ? 
            ImGui::GetColorU32(ImVec4(0.0f, 0.8f, 1.0f, 1.0f)) :  // Cyan
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));   // Dark
        float hoverBorderThickness = isHoveringTile ? 2.5f : 1.5f;
        drawList->AddRect(
            hoverMin, hoverMax, hoverBorderColor, 0.0f, 0, 
            hoverBorderThickness
        );
        
        ImGui::Dummy(ImVec2(previewWidth, hoverHeight));
        
        // Display hover color name (centered)
        ImGui::Spacing();
        float hoverTextWidth = ImGui::CalcTextSize(hoverName.c_str()).x;
        float hoverCenterX = 
            (ImGui::GetContentRegionAvail().x - hoverTextWidth) * 0.5f;
        if (hoverCenterX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + hoverCenterX);
        }
        ImGui::Text("%s", hoverName.c_str());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Current selection (smaller preview)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Current Selection:");
        ImGui::Spacing();
        
        Color selectedColor(0.8f, 0.8f, 0.8f, 1.0f);
        std::string selectedName = "Empty";
        for (const auto& tile : model.palette) {
            if (tile.id == m_canvasPanel.selectedTileId) {
                selectedColor = tile.color;
                selectedName = tile.name;
                break;
            }
        }
        
        // Smaller selected preview
        float selectedHeight = 40.0f;
        cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 selectedMin = cursorPos;
        ImVec2 selectedMax = ImVec2(
            cursorPos.x + previewWidth, 
            cursorPos.y + selectedHeight
        );
        
        drawList->AddRectFilled(
            selectedMin, selectedMax, selectedColor.ToU32()
        );
        drawList->AddRect(
            selectedMin, selectedMax,
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)),
            0.0f, 0, 1.5f
        );
        
        ImGui::Dummy(ImVec2(previewWidth, selectedHeight));
        
        // Display selected name (centered)
        float selectedTextWidth = ImGui::CalcTextSize(selectedName.c_str()).x;
        float selectedCenterX = 
            (ImGui::GetContentRegionAvail().x - selectedTextWidth) * 0.5f;
        if (selectedCenterX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + selectedCenterX);
        }
        ImGui::TextColored(
            ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", selectedName.c_str()
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Auto-switch to Paint toggle
        ImGui::Checkbox("Auto-switch to Paint", &m_canvasPanel.eyedropperAutoSwitchToPaint);
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Automatically switch to Paint tool\n"
                "after picking a color"
            );
        }
    }
    
    // Show palette when Paint tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Paint || m_canvasPanel.currentTool == CanvasPanel::Tool::Fill) {
        ImGui::Text("Paint Color");
        ImGui::Separator();
        
        // Display palette tiles inline
        for (const auto& tile : model.palette) {
            if (tile.id == 0) continue;  // Skip empty tile
            
            ImGui::PushID(tile.id);
            
            bool selected = (m_canvasPanel.selectedTileId == tile.id);
            ImVec4 color = tile.color.ToImVec4();
            bool inUse = model.IsPaletteColorInUse(tile.id);
            
            // Color button with visual indicator if in use
            ImGuiColorEditFlags colorBtnFlags = 0;
            if (ImGui::ColorButton("##color", color, colorBtnFlags, 
                                  ImVec2(24, 24))) {
                m_canvasPanel.selectedTileId = tile.id;
            }
            
            // Double-click to edit
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                // Open edit modal
                colorPickerEditingTileId = tile.id;
                strncpy(colorPickerName, tile.name.c_str(), 
                       sizeof(colorPickerName) - 1);
                colorPickerName[sizeof(colorPickerName) - 1] = '\0';
                
                colorPickerColor[0] = tile.color.r;
                colorPickerColor[1] = tile.color.g;
                colorPickerColor[2] = tile.color.b;
                colorPickerColor[3] = tile.color.a;
                
                // Save original for preview
                colorPickerOriginalColor[0] = tile.color.r;
                colorPickerOriginalColor[1] = tile.color.g;
                colorPickerOriginalColor[2] = tile.color.b;
                colorPickerOriginalColor[3] = tile.color.a;
                
                showColorPickerModal = true;
            }
            
            // Tooltip showing hex value and usage
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", tile.color.ToHex(false).c_str());
                if (inUse) {
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), 
                                      "In use");
                }
                ImGui::Text("Double-click to edit");
                ImGui::Text("Right-click for options");
                ImGui::EndTooltip();
            }
            
            // Right-click context menu
            if (ImGui::BeginPopupContextItem("color_context")) {
                ImGui::TextDisabled("%s", tile.name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Edit...")) {
                    // Open edit modal
                    colorPickerEditingTileId = tile.id;
                    strncpy(colorPickerName, tile.name.c_str(), 
                           sizeof(colorPickerName) - 1);
                    colorPickerName[sizeof(colorPickerName) - 1] = '\0';
                    
                    colorPickerColor[0] = tile.color.r;
                    colorPickerColor[1] = tile.color.g;
                    colorPickerColor[2] = tile.color.b;
                    colorPickerColor[3] = tile.color.a;
                    
                    colorPickerOriginalColor[0] = tile.color.r;
                    colorPickerOriginalColor[1] = tile.color.g;
                    colorPickerOriginalColor[2] = tile.color.b;
                    colorPickerOriginalColor[3] = tile.color.a;
                    
                    showColorPickerModal = true;
                }
                
                if (ImGui::MenuItem("Duplicate")) {
                    // Create a copy with "(Copy)" suffix
                    std::string newName = tile.name + " (Copy)";
                    auto cmd = std::make_unique<AddPaletteColorCommand>(
                        newName, tile.color);
                    history.AddCommand(std::move(cmd), model, true);
                    
                    ShowToast("Color duplicated: " + newName, 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                
                // Delete option with warning if in use
                if (inUse) {
                    ImGui::PushStyleColor(ImGuiCol_Text, 
                        ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                    if (ImGui::MenuItem("Delete (in use)...")) {
                        // Open delete confirmation
                        colorPickerEditingTileId = tile.id;
                        colorPickerDeleteRequested = true;
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::MenuItem("Delete")) {
                        // Direct delete without confirmation
                        auto cmd = std::make_unique<RemovePaletteColorCommand>(
                            tile.id, 0);
                        history.AddCommand(std::move(cmd), model, true);
                        
                        // If this was selected, switch to default
                        if (m_canvasPanel.selectedTileId == tile.id) {
                            m_canvasPanel.selectedTileId = 1;  // Default to "Solid"
                        }
                        
                        ShowToast("Color deleted: " + tile.name, 
                                 Toast::Type::Info);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::SameLine();
            
            // Selectable name
            if (ImGui::Selectable(tile.name.c_str(), selected, 0, 
                                  ImVec2(0, 24))) {
                m_canvasPanel.selectedTileId = tile.id;
            }
            
            // Same context menu on name
            if (ImGui::BeginPopupContextItem("name_context")) {
                ImGui::TextDisabled("%s", tile.name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Edit...")) {
                    colorPickerEditingTileId = tile.id;
                    strncpy(colorPickerName, tile.name.c_str(), 
                           sizeof(colorPickerName) - 1);
                    colorPickerName[sizeof(colorPickerName) - 1] = '\0';
                    
                    colorPickerColor[0] = tile.color.r;
                    colorPickerColor[1] = tile.color.g;
                    colorPickerColor[2] = tile.color.b;
                    colorPickerColor[3] = tile.color.a;
                    
                    colorPickerOriginalColor[0] = tile.color.r;
                    colorPickerOriginalColor[1] = tile.color.g;
                    colorPickerOriginalColor[2] = tile.color.b;
                    colorPickerOriginalColor[3] = tile.color.a;
                    
                    showColorPickerModal = true;
                }
                
                if (ImGui::MenuItem("Duplicate")) {
                    std::string newName = tile.name + " (Copy)";
                    auto cmd = std::make_unique<AddPaletteColorCommand>(
                        newName, tile.color);
                    history.AddCommand(std::move(cmd), model, true);
                    
                    ShowToast("Color duplicated: " + newName, 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                
                if (inUse) {
                    ImGui::PushStyleColor(ImGuiCol_Text, 
                        ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                    if (ImGui::MenuItem("Delete (in use)...")) {
                        colorPickerEditingTileId = tile.id;
                        colorPickerDeleteRequested = true;
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::MenuItem("Delete")) {
                        auto cmd = std::make_unique<RemovePaletteColorCommand>(
                            tile.id, 0);
                        history.AddCommand(std::move(cmd), model, true);
                        
                        if (m_canvasPanel.selectedTileId == tile.id) {
                            m_canvasPanel.selectedTileId = 1;
                        }
                        
                        ShowToast("Color deleted: " + tile.name, 
                                 Toast::Type::Info);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::PopID();
        }
        
        ImGui::Spacing();
        
        // Add Color button
        bool canAddMore = model.palette.size() < 32;
        
        if (!canAddMore) {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("+ Add Color", ImVec2(-1, 0))) {
            // Open modal in "new color" mode
            colorPickerEditingTileId = -1;
            
            // Generate a smart default name
            int colorNum = static_cast<int>(model.palette.size());
            std::string defaultName = "Color " + std::to_string(colorNum);
            strncpy(colorPickerName, defaultName.c_str(), 
                   sizeof(colorPickerName) - 1);
            colorPickerName[sizeof(colorPickerName) - 1] = '\0';
            
            // Start with white color
            colorPickerColor[0] = 1.0f;
            colorPickerColor[1] = 1.0f;
            colorPickerColor[2] = 1.0f;
            colorPickerColor[3] = 1.0f;
            
            showColorPickerModal = true;
        }
        
        if (!canAddMore) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Palette is full (max 32 colors)");
            }
        }
    }
    
    // Show eraser options when Erase tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Erase) {
        ImGui::Text("Eraser Options");
        ImGui::Separator();
        
        // Label on its own line
        ImGui::Text("Eraser Size");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of eraser brush in tiles\n"
                            "1 = single tile (precise)\n"
                            "5 = 5x5 area (fast erase)");
        }
        
        // Slider below label
        ImGui::SetNextItemWidth(-1);  // Full width
        ImGui::SliderInt("##eraserSize", &m_canvasPanel.eraserBrushSize, 1, 5);
        
        // Visual preview of eraser size
        ImGui::Spacing();
        ImGui::Text("Preview:");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        float previewSize = 80.0f;
        float cellSize = previewSize / 5.0f;  // 5x5 grid
        
        // Draw grid background
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                ImVec2 p0(cursorPos.x + x * cellSize, 
                         cursorPos.y + y * cellSize);
                ImVec2 p1(p0.x + cellSize, p0.y + cellSize);
                
                // Calculate if this cell is within brush
                int centerOffset = 2;  // Center of 5x5 grid
                int halfBrush = m_canvasPanel.eraserBrushSize / 2;
                bool inBrush = (x >= centerOffset - halfBrush && 
                               x <= centerOffset + halfBrush &&
                               y >= centerOffset - halfBrush && 
                               y <= centerOffset + halfBrush);
                
                // Draw cell
                ImU32 fillColor = inBrush ? 
                    ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 0.4f)) :
                    ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
                drawList->AddRectFilled(p0, p1, fillColor);
                
                // Draw grid lines
                ImU32 lineColor = ImGui::GetColorU32(
                    ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                drawList->AddRect(p0, p1, lineColor);
            }
        }
        
        ImGui::Dummy(ImVec2(previewSize, previewSize));
    }
    
    // Show marker options when Marker tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Marker) {
        ImGui::Text("Marker Settings");
        ImGui::Separator();
        
        // Label input
        char labelBuf[128];
        std::strncpy(labelBuf, m_canvasPanel.markerLabel.c_str(), sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
        
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
            m_canvasPanel.markerLabel = labelBuf;
            
            // Update selected marker if editing
            if (m_canvasPanel.selectedMarker) {
                m_canvasPanel.selectedMarker->label = m_canvasPanel.markerLabel;
                m_canvasPanel.selectedMarker->showLabel = !m_canvasPanel.markerLabel.empty();
                model.MarkDirty();
            }
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
        }
        
        // Color picker popup
        if (ImGui::BeginPopup("ColorPicker")) {
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
                m_canvasPanel.markerColorHex[sizeof(m_canvasPanel.markerColorHex) - 1] = '\0';
                
                // Update selected marker if editing
                if (m_canvasPanel.selectedMarker) {
                    m_canvasPanel.selectedMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
            ImGui::EndPopup();
        }
        
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
                if (m_canvasPanel.selectedMarker) {
                    m_canvasPanel.selectedMarker->color = m_canvasPanel.markerColor;
                    model.MarkDirty();
                }
            }
        }
        
        // Show hint text
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Hex color (e.g., #4dcc4d)");
        }
        
        ImGui::Separator();
        
        // Import Icon button at the top
        if (ImGui::Button("Import Icon...", ImVec2(-1, 0))) {
            ImportIcon(icons, jobs);
        }
        
        // Show loading indicator if importing
        if (isImportingIcon) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "Loading...");
        }
        
        ImGui::Spacing();
        
        // Handle dropped file (OS-level file drop)
        if (hasDroppedFile) {
            // Check if it's an image file
            std::string ext = droppedFilePath.substr(
                droppedFilePath.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || 
                ext == "bmp" || ext == "gif" || ext == "tga" || ext == "webp") {
                // Extract filename without extension for icon name
                size_t lastSlash = droppedFilePath.find_last_of("/\\");
                size_t lastDot = droppedFilePath.find_last_of(".");
                std::string baseName = droppedFilePath.substr(
                    lastSlash + 1, 
                    lastDot - lastSlash - 1
                );
                
                // Generate unique name
                std::string iconName = icons.GenerateUniqueName(baseName);
                
                // Start async import
                importingIconName = iconName;
                isImportingIcon = true;
                
                // Capture variables for async processing
                std::string capturedIconName = iconName;
                std::string capturedFilePath = droppedFilePath;
                
                jobs.Enqueue(
                    JobType::ProcessIcon,
                    [capturedIconName, capturedFilePath, &icons]() {
                        std::vector<uint8_t> pixels;
                        int width, height;
                        std::string errorMsg;
                        
                        if (!IconManager::ProcessIconFromFile(
                                capturedFilePath, pixels, 
                                width, height, errorMsg)) {
                            throw std::runtime_error(errorMsg);
                        }
                        
                        if (!icons.AddIconFromMemory(capturedIconName, 
                                                     pixels.data(), 
                                                     width, height, "marker")) {
                            throw std::runtime_error(
                                "Failed to add icon to memory");
                        }
                    },
                    [this, &icons, capturedIconName](
                        bool success, const std::string& error) {
                        isImportingIcon = false;
                        if (success) {
                            // Build atlas on main thread (OpenGL required)
                            icons.BuildAtlas();
                            
                            ShowToast("Icon imported: " + capturedIconName, 
                                     Toast::Type::Success, 2.0f);
                            // Auto-select the imported icon
                            m_canvasPanel.selectedIconName = capturedIconName;
                        } else {
                            ShowToast("Failed to import: " + error, 
                                     Toast::Type::Error, 3.0f);
                        }
                    }
                );
            } else {
                ShowToast("Unsupported format. Use PNG, JPEG, BMP, GIF, "
                         "TGA, or WebP", 
                         Toast::Type::Warning, 3.0f);
            }
            
            hasDroppedFile = false;
            droppedFilePath.clear();
        }
        
        // Icon picker grid
        ImGui::Text("Select Icon");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Drag & drop image files here to import");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
            " | Right-click or Del key to delete");
        if (ImGui::BeginChild("IconPicker", ImVec2(0, 280), true)) {
            // Show loading overlay if importing
            if (isImportingIcon) {
                ImVec2 childSize = ImGui::GetWindowSize();
                ImVec2 textSize = ImGui::CalcTextSize(
                    ("Importing: " + importingIconName + "...").c_str());
                ImGui::SetCursorPos(ImVec2(
                    (childSize.x - textSize.x) * 0.5f,
                    (childSize.y - textSize.y) * 0.5f
                ));
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), 
                    "Importing: %s...", importingIconName.c_str());
            } else if (icons.GetIconCount() == 0) {
                ImGui::TextDisabled("No icons loaded");
                ImGui::TextDisabled("Click 'Import Icon...' to add icons");
            } else {
                // Get marker icon names (exclude tool icons)
                auto iconNames = icons.GetIconNamesByCategory("marker");
                
                // Responsive grid layout
                float buttonSize = 80.0f;  // Size of each icon button
                float spacing = 8.0f;
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                // Calculate responsive columns (min 2, max 4)
                int columns = std::max(2, 
                    static_cast<int>((availWidth + spacing) / 
                                    (buttonSize + spacing)));
                columns = std::min(columns, 4);
                
                // Calculate centering offset
                float totalWidth = columns * buttonSize + 
                                  (columns - 1) * spacing;
                float leftPadding = std::max(0.0f, 
                    (availWidth - totalWidth) * 0.5f);
                
                for (size_t i = 0; i < iconNames.size(); ++i) {
                    const std::string& iconName = iconNames[i];
                    const Icon* icon = icons.GetIcon(iconName);
                    
                    if (!icon) continue;
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    // Begin new row with left padding for centering
                    if (i % columns == 0) {
                        ImGui::SetCursorPosX(
                            ImGui::GetCursorPosX() + leftPadding);
                    } else {
                        ImGui::SameLine(0, spacing);
                    }
                    
                    ImGui::BeginGroup();
                    
                    // Draw icon button
                    bool isSelected = (m_canvasPanel.selectedIconName == iconName);
                    
                    // Highlight selected icon
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, 
                            ImVec4(0.2f, 0.4f, 0.8f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                            ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                            ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
                    }
                    
                    // Icon image button
                    if (icons.GetAtlasTexture()) {
                        ImVec2 uvMin(icon->u0, icon->v0);
                        ImVec2 uvMax(icon->u1, icon->v1);
                        
                        if (ImGui::ImageButton(
                            iconName.c_str(),
                            icons.GetAtlasTexture(),
                            ImVec2(buttonSize, buttonSize),
                            uvMin, uvMax)) {
                            m_canvasPanel.selectedIconName = iconName;
                            
                            // Update selected marker if editing
                            if (m_canvasPanel.selectedMarker) {
                                m_canvasPanel.selectedMarker->icon = m_canvasPanel.selectedIconName;
                                model.MarkDirty();
                            }
                        }
                    } else {
                        // Fallback if no texture
                        if (ImGui::Button("##icon", 
                                         ImVec2(buttonSize, buttonSize))) {
                            m_canvasPanel.selectedIconName = iconName;
                            
                            if (m_canvasPanel.selectedMarker) {
                                m_canvasPanel.selectedMarker->icon = m_canvasPanel.selectedIconName;
                                model.MarkDirty();
                            }
                        }
                    }
                    
                    if (isSelected) {
                        ImGui::PopStyleColor(3);
                    }
                    
                    // Enable drag-drop from icon button
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        // Set payload with icon name
                        ImGui::SetDragDropPayload("MARKER_ICON", 
                            iconName.c_str(), 
                            iconName.length() + 1);
                        
                        // Show preview while dragging
                        if (icons.GetAtlasTexture()) {
                            ImVec2 uvMin(icon->u0, icon->v0);
                            ImVec2 uvMax(icon->u1, icon->v1);
                            ImGui::Image(icons.GetAtlasTexture(), 
                                ImVec2(32, 32), uvMin, uvMax);
                        }
                        ImGui::TextUnformatted(iconName.c_str());
                        
                        ImGui::EndDragDropSource();
                    }
                    
                    // Right-click context menu for custom icons
                    if (icon->category == "marker" && 
                        ImGui::BeginPopupContextItem(
                            ("icon_ctx_" + iconName).c_str())) {
                        ImGui::TextDisabled("Icon: %s", iconName.c_str());
                        ImGui::Separator();
                        
                        if (ImGui::MenuItem("Rename...")) {
                            showRenameIconModal = true;
                            renameIconOldName = iconName;
                            std::strncpy(renameIconNewName, 
                                        iconName.c_str(), 
                                        sizeof(renameIconNewName) - 1);
                            renameIconNewName[sizeof(renameIconNewName) - 1] 
                                = '\0';
                        }
                        
                        if (ImGui::MenuItem("Delete...")) {
                            showDeleteIconModal = true;
                            deleteIconName = iconName;
                            // Count markers using this icon
                            deleteIconMarkerCount = 
                                model.CountMarkersUsingIcon(iconName);
                            // Get affected marker IDs (for display)
                            deleteIconAffectedMarkers = 
                                model.GetMarkersUsingIcon(iconName);
                        }
                        
                        ImGui::EndPopup();
                    }
                    
                    // Icon name label below button (centered)
                    float textWidth = ImGui::CalcTextSize(iconName.c_str()).x;
                    float offset = (buttonSize - textWidth) * 0.5f;
                    if (offset > 0) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
                    }
                    ImGui::TextWrapped("%s", iconName.c_str());
                    
                    // Handle Delete key for selected icon
                    if (isSelected && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                        showDeleteIconModal = true;
                        deleteIconName = iconName;
                        // Count markers using this icon
                        deleteIconMarkerCount = 
                            model.CountMarkersUsingIcon(iconName);
                        // Get affected marker IDs (for display)
                        deleteIconAffectedMarkers = 
                            model.GetMarkersUsingIcon(iconName);
                    }
                    
                    ImGui::EndGroup();
                    
                    ImGui::PopID();
                }
            }
            
            // Show import progress
            if (isImportingIcon) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f),
                    "Importing: %s...", importingIconName.c_str());
            }
            
            ImGui::EndChild();
        }
        
        // Actions
        ImGui::Separator();
        
        if (m_canvasPanel.selectedMarker) {
            ImGui::Text("Editing marker:");
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               m_canvasPanel.selectedMarker->x, m_canvasPanel.selectedMarker->y);
            
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                m_canvasPanel.selectedMarker = nullptr;
            }
        }
    }
    
    // TODO: Add layers toggles here
    
    ImGui::End();
}

void UI::RenderPropertiesPanel(Model& model, IconManager& icons, JobQueue& jobs) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Inspector", nullptr, flags);
    
    ImGui::Text("Inspector");
    ImGui::Separator();
    
    // Rooms management
    if (ImGui::CollapsingHeader("Rooms", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Room overlay toggle
        if (ImGui::Checkbox("Show Overlays", &m_canvasPanel.showRoomOverlays)) {
            // Visual change only, no model change
        }
        
        // New room button
        if (ImGui::Button("Create Room")) {
            showNewRoomDialog = true;
        }
        
        ImGui::Separator();
        
        // List existing rooms
        if (model.rooms.empty()) {
            ImGui::TextDisabled("No rooms created yet");
        } else {
            for (auto& room : model.rooms) {
                bool isSelected = (m_canvasPanel.selectedRoomId == room.id);
                ImGuiTreeNodeFlags nodeFlags = 
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    (isSelected ? ImGuiTreeNodeFlags_Selected : 0);
                
                // Color indicator
                ImVec4 roomColor(room.color.r, room.color.g, 
                                room.color.b, room.color.a);
                ImGui::ColorButton("##colorIndicator", roomColor, 
                                  ImGuiColorEditFlags_NoTooltip, 
                                  ImVec2(16, 16));
                ImGui::SameLine();
                
                // Room name (selectable)
                ImGui::TreeNodeEx(room.name.c_str(), nodeFlags);
                if (ImGui::IsItemClicked()) {
                    m_canvasPanel.selectedRoomId = room.id;
                    m_canvasPanel.roomPaintMode = false;  // Reset paint mode on select
                }
            }
        }
    }
    
    ImGui::Spacing();
    
    // Selected room details
    if (!m_canvasPanel.selectedRoomId.empty()) {
        Room* room = model.FindRoom(m_canvasPanel.selectedRoomId);
        if (room) {
            if (ImGui::CollapsingHeader("Selected Room", 
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                // Room name
                char nameBuf[256];
                strncpy(nameBuf, room->name.c_str(), sizeof(nameBuf) - 1);
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                    room->name = nameBuf;
                    model.MarkDirty();
                }
                
                // Room color
                float colorArray[4] = {
                    room->color.r, room->color.g, 
                    room->color.b, room->color.a
                };
                if (ImGui::ColorEdit4("Color", colorArray)) {
                    room->color = Color(colorArray[0], colorArray[1], 
                                       colorArray[2], colorArray[3]);
                    model.MarkDirty();
                }
                
                // Room notes
                char notesBuf[1024];
                strncpy(notesBuf, room->notes.c_str(), sizeof(notesBuf) - 1);
                if (ImGui::InputTextMultiline("Notes", notesBuf, 
                                             sizeof(notesBuf), 
                                             ImVec2(-1, 80))) {
                    room->notes = notesBuf;
                    model.MarkDirty();
                }
                
                ImGui::Separator();
                
                // Room paint mode toggle
                if (m_canvasPanel.roomPaintMode) {
                    if (ImGui::Button("Exit Room Paint Mode")) {
                        m_canvasPanel.roomPaintMode = false;
                    }
                    ImGui::TextWrapped(
                        "Paint cells to assign them to this room. "
                        "Right-click to remove cells."
                    );
                } else {
                    if (ImGui::Button("Paint Room Cells")) {
                        m_canvasPanel.roomPaintMode = true;
                    }
                }
                
                ImGui::Separator();
                
                // Room actions
                if (ImGui::Button("Demote Room")) {
                    ImGui::OpenPopup("Demote Room?");
                }
                
                // Demote confirmation popup
                if (ImGui::BeginPopupModal("Demote Room?", nullptr, 
                                          ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("What would you like to do?");
                    ImGui::Separator();
                    
                    if (ImGui::Button("Remove room metadata only", 
                                     ImVec2(-1, 0))) {
                        // Remove room but keep cells/tiles
                        model.ClearAllCellsForRoom(room->id);
                        auto it = std::find_if(
                            model.rooms.begin(), 
                            model.rooms.end(),
                            [&](const Room& r) { return r.id == m_canvasPanel.selectedRoomId; }
                        );
                        if (it != model.rooms.end()) {
                            model.rooms.erase(it);
                        }
                        m_canvasPanel.selectedRoomId.clear();
                        m_canvasPanel.roomPaintMode = false;
                        model.MarkDirty();
                        ImGui::CloseCurrentPopup();
                    }
                    
                    if (ImGui::Button("Remove room AND clear all cells", 
                                     ImVec2(-1, 0))) {
                        // Clear all cells assigned to this room
                        for (auto it = model.cellRoomAssignments.begin(); 
                             it != model.cellRoomAssignments.end(); ) {
                            if (it->second == room->id) {
                                // Also clear the tile at this position
                                model.SetTileAt("", it->first.first, 
                                              it->first.second, 0);
                                it = model.cellRoomAssignments.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        // Remove room
                        auto it = std::find_if(
                            model.rooms.begin(), 
                            model.rooms.end(),
                            [&](const Room& r) { return r.id == m_canvasPanel.selectedRoomId; }
                        );
                        if (it != model.rooms.end()) {
                            model.rooms.erase(it);
                        }
                        m_canvasPanel.selectedRoomId.clear();
                        m_canvasPanel.roomPaintMode = false;
                        model.MarkDirty();
                        ImGui::CloseCurrentPopup();
                    }
                    
                    if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    
                    ImGui::EndPopup();
                }
            }
        }
    }
    
    // New room dialog
    if (showNewRoomDialog) {
        ImGui::OpenPopup("Create New Room");
        showNewRoomDialog = false;
    }
    
    if (ImGui::BeginPopupModal("Create New Room", nullptr, 
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Room Name", newRoomName, sizeof(newRoomName));
        ImGui::ColorEdit3("Room Color", newRoomColor);
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Generate unique room ID
            std::string roomId = "room_" + std::to_string(model.rooms.size());
            
            Room newRoom;
            newRoom.id = roomId;
            newRoom.name = newRoomName;
            newRoom.color = Color(newRoomColor[0], newRoomColor[1], 
                                 newRoomColor[2], 1.0f);
            newRoom.regionId = -1;
            
            model.rooms.push_back(newRoom);
            m_canvasPanel.selectedRoomId = roomId;
            model.MarkDirty();
            
            // Reset form
            strcpy(newRoomName, "New Room");
            newRoomColor[0] = 1.0f;
            newRoomColor[1] = 0.5f;
            newRoomColor[2] = 0.5f;
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Note: Marker tool settings moved to Tools panel (shown when Marker 
    // tool is selected)
    
    ImGui::End();
}



void UI::RenderStatusBar(Model& model, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    
    // Check if we have an active error to display
    if (!m_statusError.empty() && m_statusErrorTime > 0.0f) {
        // Red background for errors
        ImGui::PushStyleColor(ImGuiCol_WindowBg, 
                             ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
        ImGui::Begin("Cartograph/Console", nullptr, flags);
        ImGui::PopStyleColor();
        
        // Display error prominently
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                          "⚠ ERROR: %s", m_statusError.c_str());
        
        // Countdown the error display time
        m_statusErrorTime -= 0.016f;  // Approximate frame time
        if (m_statusErrorTime <= 0.0f) {
            m_statusError.clear();
        }
    } else {
        // Normal status bar
        ImGui::Begin("Cartograph/Console", nullptr, flags);
        
        // Left section: Tile coordinates (if hovering canvas)
        if (m_canvasPanel.isHoveringCanvas && m_canvasPanel.hoveredTileX >= 0 && m_canvasPanel.hoveredTileY >= 0) {
            ImGui::Text("Tile: %d, %d", m_canvasPanel.hoveredTileX, m_canvasPanel.hoveredTileY);
        } else {
            ImGui::TextDisabled("Tile: --, --");
        }
        
        // Separator
        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("|");
        
        // Middle section: Zoom
        ImGui::SameLine(0, 10);
        // Display zoom relative to default (2.5 internal = 100% display)
        float displayZoom = (canvas.zoom / Canvas::DEFAULT_ZOOM) * 100.0f;
        ImGui::Text("Zoom: %.0f%%", displayZoom);
        
        // Separator
        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("|");
        
        // Console message section (remaining space)
        ImGui::SameLine(0, 10);
        
        if (!m_consoleMessages.empty()) {
            // Get the most recent message
            const ConsoleMessage& lastMsg = m_consoleMessages.back();
            
            // Choose icon and color based on message type
            const char* icon = "";
            ImVec4 color;
            switch (lastMsg.type) {
                case MessageType::Info:
                    icon = "ℹ";
                    color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);  // Light blue
                    break;
                case MessageType::Success:
                    icon = "✓";
                    color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);  // Green
                    break;
                case MessageType::Warning:
                    icon = "⚠";
                    color = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);  // Orange
                    break;
                case MessageType::Error:
                    icon = "✖";
                    color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
                    break;
            }
            
            // Display message with icon
            ImGui::TextColored(color, "%s", icon);
            ImGui::SameLine(0, 5);
            
            // Truncate long messages to single line (max 120 chars)
            std::string displayMsg = lastMsg.message;
            
            // Replace newlines with spaces
            for (char& c : displayMsg) {
                if (c == '\n' || c == '\r') {
                    c = ' ';
                }
            }
            
            // Truncate if too long
            const size_t maxLen = 120;
            if (displayMsg.length() > maxLen) {
                displayMsg = displayMsg.substr(0, maxLen - 3) + "...";
            }
            
            // Calculate time since message (for fading effect)
            double age = Platform::GetTime() - lastMsg.timestamp;
            if (age > 5.0) {
                // Fade out old messages
                ImGui::TextDisabled("%s", displayMsg.c_str());
            } else {
                ImGui::Text("%s", displayMsg.c_str());
            }
        } else {
            // No messages yet
            ImGui::TextDisabled("Ready");
        }
    }
    
    ImGui::PopStyleVar();
    ImGui::End();
}

void UI::RenderToasts(float deltaTime) {
    // Update and render toasts
    float yOffset = 100.0f;
    
    for (auto it = m_toasts.begin(); it != m_toasts.end(); ) {
        it->remainingTime -= deltaTime;
        
        if (it->remainingTime <= 0.0f) {
            it = m_toasts.erase(it);
            continue;
        }
        
        // Render toast
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;
        
        ImGui::SetNextWindowPos(
            ImVec2(workPos.x + workSize.x - 320, workPos.y + yOffset)
        );
        ImGui::SetNextWindowSize(ImVec2(300, 0));
        
        ImGuiWindowFlags flags = 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoInputs;
        
        ImGui::Begin(("##toast" + std::to_string((size_t)&(*it))).c_str(), 
            nullptr, flags);
        ImGui::Text("%s", it->message.c_str());
        ImGui::End();
        
        yOffset += 60.0f;
        ++it;
    }
}

void UI::RenderExportModal(Model& model, Canvas& canvas) {
    ImGui::OpenPopup("Export PNG");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
                           ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Export PNG", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Calculate content dimensions for display
        ContentBounds bounds = model.CalculateContentBounds();
        
        // Check if there's content to export
        if (bounds.isEmpty) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                "Warning: No content to export!");
            ImGui::Text("Draw some tiles, walls, or markers first.");
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                showExportModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }
        
        int contentWidthTiles = bounds.maxX - bounds.minX + 1;
        int contentHeightTiles = bounds.maxY - bounds.minY + 1;
        int contentWidthPx = contentWidthTiles * model.grid.tileWidth;
        int contentHeightPx = contentHeightTiles * model.grid.tileHeight;
        
        // Display content info
        ImGui::Text("Content Area: %d × %d pixels (%d × %d tiles)",
                   contentWidthPx, contentHeightPx,
                   contentWidthTiles, contentHeightTiles);
        ImGui::Separator();
        
        // Size mode selection
        ImGui::Text("Size Mode:");
        
        int mode = (int)exportOptions.sizeMode;
        if (ImGui::RadioButton("Scale", &mode, 0)) {
            exportOptions.sizeMode = ExportOptions::SizeMode::Scale;
        }
        
        if (exportOptions.sizeMode == ExportOptions::SizeMode::Scale) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderInt("##scale", &exportOptions.scale, 1, 4);
            
            int outW = (contentWidthPx + exportOptions.padding * 2) * 
                      exportOptions.scale;
            int outH = (contentHeightPx + exportOptions.padding * 2) * 
                      exportOptions.scale;
            ImGui::SameLine();
            ImGui::Text("→ %d × %d px", outW, outH);
        }
        
        if (ImGui::RadioButton("Custom Dimensions", &mode, 1)) {
            exportOptions.sizeMode = 
                ExportOptions::SizeMode::CustomDimensions;
        }
        
        if (exportOptions.sizeMode == 
            ExportOptions::SizeMode::CustomDimensions) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Width", &exportOptions.customWidth);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Height", &exportOptions.customHeight);
            
            // Clamp to reasonable range
            exportOptions.customWidth = std::clamp(
                exportOptions.customWidth, 64, 
                ExportOptions::MAX_DIMENSION);
            exportOptions.customHeight = std::clamp(
                exportOptions.customHeight, 64, 
                ExportOptions::MAX_DIMENSION);
            
            ImGui::Text("(scales to fit, maintains aspect ratio)");
            ImGui::Unindent();
        }
        
        ImGui::Separator();
        
        // Padding control
        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("Padding (px)", &exportOptions.padding, 0, 64);
        
        ImGui::Separator();
        
        // Transparency
        ImGui::Checkbox("Transparency", &exportOptions.transparency);
        
        if (!exportOptions.transparency) {
            ImGui::SameLine();
            ImGui::ColorEdit3("Background", &exportOptions.bgColorR);
        }
        
        ImGui::Separator();
        ImGui::Text("Layers");
        ImGui::Checkbox("Grid", &exportOptions.layerGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Tiles", &exportOptions.layerTiles);
        ImGui::Checkbox("Walls & Doors", &exportOptions.layerDoors);
        ImGui::SameLine();
        ImGui::Checkbox("Markers", &exportOptions.layerMarkers);
        
        ImGui::Separator();
        
        // Export button - triggers file dialog
        if (ImGui::Button("Export...", ImVec2(120, 0))) {
            showExportModal = false;
            ImGui::CloseCurrentPopup();
            // Trigger the export dialog (will be called after modal closes)
            shouldShowExportPngDialog = true;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showExportModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderSettingsModal(Model& model, KeymapManager& keymap) {
    ImGui::OpenPopup("Settings");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Settings", nullptr, 
        ImGuiWindowFlags_NoResize)) {
        
        // Apply increased padding for better spacing
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        
        // Style the tab bar for better visibility
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, 
            ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, 
            ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
        
        if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
            
            // ============================================================
            // TAB 1: PROJECT
            // ============================================================
            if (ImGui::BeginTabItem("Project")) {
                settingsModalSelectedTab = 0;
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::Text("Project Information");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Project metadata
        char titleBuf[256];
                strncpy(titleBuf, model.meta.title.c_str(), 
                    sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            model.meta.title = titleBuf;
            model.MarkDirty();
        }
        
        char authorBuf[256];
                strncpy(authorBuf, model.meta.author.c_str(), 
                    sizeof(authorBuf) - 1);
        authorBuf[sizeof(authorBuf) - 1] = '\0';
        if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
            model.meta.author = authorBuf;
            model.MarkDirty();
        }
        
        ImGui::Spacing();
        
        // Project description (multi-line)
        char descBuf[2048];
                strncpy(descBuf, model.meta.description.c_str(), 
                    sizeof(descBuf) - 1);
        descBuf[sizeof(descBuf) - 1] = '\0';
        ImGui::Text("Description");
        if (ImGui::InputTextMultiline("##description", descBuf, 
                                      sizeof(descBuf),
                                      ImVec2(-1, 120.0f),
                                      ImGuiInputTextFlags_AllowTabInput)) {
            model.meta.description = descBuf;
            model.MarkDirty();
        }
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Brief description of your project (optional)");
        
        ImGui::Spacing();
        ImGui::Separator();
                ImGui::Spacing();
                
                // Preview info
                ImGui::Text("Canvas Information");
                ImGui::Separator();
                ImGui::Spacing();
                
                int totalCells = model.grid.cols * model.grid.rows;
                int pixelWidth = model.grid.cols * model.grid.tileWidth;
                int pixelHeight = model.grid.rows * model.grid.tileHeight;
                
                ImGui::Text("Total cells: %d", totalCells);
                ImGui::Text("Canvas size: %d × %d pixels", 
                    pixelWidth, pixelHeight);
                ImGui::Text("Cell size: %d × %d pixels", 
                    model.grid.tileWidth, model.grid.tileHeight);
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 2: GRID & CANVAS
            // ============================================================
            if (ImGui::BeginTabItem("Grid & Canvas")) {
                settingsModalSelectedTab = 1;
                ImGui::Spacing();
                ImGui::Spacing();
        
        // Grid Cell Type
                ImGui::Text("Grid Cell Type");
                ImGui::Separator();
                ImGui::Spacing();
        
        bool canChangePreset = model.CanChangeGridPreset();
        
        if (!canChangePreset) {
            ImGui::BeginDisabled();
        }
        
        bool isSquare = (model.grid.preset == GridPreset::Square);
        if (ImGui::RadioButton("Square (16×16)", isSquare)) {
            if (canChangePreset) {
                model.ApplyGridPreset(GridPreset::Square);
            } else {
                        ShowToast(
                            "Cannot change cell type - delete all markers first",
                    Toast::Type::Warning, 3.0f);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Square cells for top-down games. "
                            "Markers snap to center only.");
        }
        
                bool isRectangle = 
                    (model.grid.preset == GridPreset::Rectangle);
        if (ImGui::RadioButton("Rectangle (32×16)", isRectangle)) {
            if (canChangePreset) {
                model.ApplyGridPreset(GridPreset::Rectangle);
            } else {
                        ShowToast(
                            "Cannot change cell type - delete all markers first",
                    Toast::Type::Warning, 3.0f);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Rectangular cells for side-scrollers. "
                            "Markers snap to left/right positions.");
        }
        
        if (!canChangePreset) {
            ImGui::EndDisabled();
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f),
                "🔒 Locked (%zu marker%s placed)",
                model.markers.size(),
                model.markers.size() == 1 ? "" : "s");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Delete all markers to change cell type");
            }
        }
        
        // Show current dimensions (read-only info)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Cell Dimensions: %d×%d px",
            model.grid.tileWidth, model.grid.tileHeight);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
                // Edge Configuration
                ImGui::Text("Edge/Wall Configuration");
        ImGui::Separator();
        ImGui::Spacing();
        
                ImGui::Checkbox("Auto-expand grid", 
                    &model.grid.autoExpandGrid);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically expand grid when placing edges "
                            "near boundaries");
        }
        
        ImGui::SetNextItemWidth(250.0f);
        ImGui::SliderInt("Expansion threshold (cells)", 
                        &model.grid.expansionThreshold, 1, 20);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Distance from grid boundary to trigger "
                            "expansion");
        }
        
        ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Expansion factor", 
                    &model.grid.expansionFactor, 
                          1.1f, 3.0f, "%.1fx");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Grid growth multiplier "
                            "(e.g., 1.5x = 50%% growth)");
        }
        
        ImGui::SetNextItemWidth(250.0f);
        ImGui::SliderFloat("Edge hover threshold", 
                          &model.grid.edgeHoverThreshold, 
                          0.1f, 0.5f, "%.2f");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Distance from cell edge to activate edge mode "
                            "(0.2 = 20%% of cell size)");
        }
        
        ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 3: KEYBINDINGS
            // ============================================================
            if (ImGui::BeginTabItem("Keybindings")) {
                settingsModalSelectedTab = 2;
                ImGui::Spacing();
                
                ImGui::Text("Keyboard Shortcuts");
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset All to Defaults")) {
                    model.InitDefaultKeymap();
                    keymap.LoadBindings(model.keymap);
                    ShowToast("Keybindings reset to defaults", 
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
                std::string bindingStr = (it != model.keymap.end()) ? 
                    keymap.GetBindingDisplayName(it->second) : "(Not bound)";
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
            renderBinding("Tool: Eyedropper", "toolEyedropper");
            
            // VIEW CATEGORY
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "VIEW");
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            
            renderBinding("Zoom In", "zoomIn");
            renderBinding("Zoom Out", "zoomOut");
            renderBinding("Toggle Grid", "toggleGrid");
            renderBinding("Toggle Properties", "togglePropertiesPanel");
            
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
            
            ImGui::EndTabBar();
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons (outside tab content, always visible)
        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            model.MarkDirty();
            showSettingsModal = false;
            ImGui::CloseCurrentPopup();
            ShowToast("Settings applied", Toast::Type::Success);
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showSettingsModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderRenameIconModal(Model& model, IconManager& icons) {
    ImGui::OpenPopup("Rename Icon");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Rename Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Current name: %s", renameIconOldName.c_str());
        ImGui::Spacing();
        
        // New name input
        ImGui::InputText("New name", renameIconNewName, 
                        sizeof(renameIconNewName));
        
        ImGui::Spacing();
        ImGui::Separator();
        
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            std::string newName(renameIconNewName);
            std::string errorMsg;
            
            // Validate and rename
            if (icons.RenameIcon(renameIconOldName, newName, errorMsg)) {
                // Update all markers using this icon
                int count = model.UpdateMarkerIconNames(
                    renameIconOldName, newName);
                
                // Update selected icon if it was the renamed one
                if (m_canvasPanel.selectedIconName == renameIconOldName) {
                    m_canvasPanel.selectedIconName = newName;
                }
                
                showRenameIconModal = false;
                ImGui::CloseCurrentPopup();
                
                std::string msg = "Icon renamed";
                if (count > 0) {
                    msg += " (" + std::to_string(count) + " marker";
                    if (count > 1) msg += "s";
                    msg += " updated)";
                }
                ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                ShowToast("Rename failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRenameIconModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderDeleteIconModal(Model& model, IconManager& icons, 
                                History& history) {
    ImGui::OpenPopup("Delete Icon");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Delete Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Delete Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Icon: %s", deleteIconName.c_str());
        ImGui::Spacing();
        
        // Show warning if markers are using this icon
        if (deleteIconMarkerCount > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.7f, 0.2f, 1.0f));  // Orange warning
            ImGui::TextWrapped(
                "Warning: This icon is used by %d marker%s.", 
                deleteIconMarkerCount,
                deleteIconMarkerCount == 1 ? "" : "s"
            );
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            
            // Show affected markers (limit to first 10)
            ImGui::TextWrapped("Affected markers:");
            ImGui::Indent();
            
            int displayCount = std::min(
                static_cast<int>(deleteIconAffectedMarkers.size()), 10
            );
            for (int i = 0; i < displayCount; ++i) {
                const std::string& markerId = deleteIconAffectedMarkers[i];
                const Marker* marker = model.FindMarker(markerId);
                if (marker) {
                    ImGui::BulletText("%s at (%.1f, %.1f)", 
                        marker->label.empty() ? 
                            marker->id.c_str() : marker->label.c_str(),
                        marker->x, marker->y
                    );
                }
            }
            
            if (deleteIconAffectedMarkers.size() > 10) {
                ImGui::BulletText("... and %zu more", 
                    deleteIconAffectedMarkers.size() - 10);
            }
            
            ImGui::Unindent();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.5f, 0.5f, 1.0f));  // Red warning
            ImGui::TextWrapped(
                "Deleting this icon will also remove all markers using it."
            );
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
        } else {
            // No markers using this icon - safe to delete
            ImGui::TextWrapped(
                "Are you sure you want to delete this icon?"
            );
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), 
                "No markers are using this icon.");
            ImGui::Spacing();
        }
        
        // Remind user that deletion is undoable
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), 
            "Tip: You can undo this with Cmd+Z");
        ImGui::Spacing();
        
        ImGui::Separator();
        
        // Delete button (destructive action, so place on left)
        bool confirmDelete = false;
        if (deleteIconMarkerCount > 0) {
            // Destructive action - use warning color
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.3f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(1.0f, 0.4f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.9f, 0.2f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete Icon & Markers", ImVec2(200, 0))) {
                confirmDelete = true;
            }
            
            ImGui::PopStyleColor(3);
        } else {
            // Safe deletion
            if (ImGui::Button("Delete Icon", ImVec2(120, 0))) {
                confirmDelete = true;
            }
        }
        
        if (confirmDelete) {
            // Create delete command with icon manager reference
            auto cmd = std::make_unique<DeleteIconCommand>(
                deleteIconName, 
                deleteIconMarkerCount > 0,  // Remove markers if any exist
                icons                       // Icon manager for undo restoration
            );
            
            // Capture icon state before deletion
            cmd->CaptureIconState();
            
            // Delete icon from IconManager
            std::string errorMsg;
            if (icons.DeleteIcon(deleteIconName, errorMsg)) {
                // Rebuild atlas
                icons.BuildAtlas();
                
                // Execute command (handles marker deletion)
                history.AddCommand(std::move(cmd), model, true);
                
                // Update selected icon if it was the deleted one
                if (m_canvasPanel.selectedIconName == deleteIconName) {
                    // Try to select another icon if available
                    auto remainingIcons = 
                        icons.GetIconNamesByCategory("marker");
                    if (!remainingIcons.empty()) {
                        m_canvasPanel.selectedIconName = remainingIcons[0];
                    } else {
                        m_canvasPanel.selectedIconName = "";
                    }
                }
                
                // Deselect marker if it was using this icon
                if (m_canvasPanel.selectedMarker && m_canvasPanel.selectedMarker->icon == deleteIconName) {
                    m_canvasPanel.selectedMarker = nullptr;
                }
                
                showDeleteIconModal = false;
                ImGui::CloseCurrentPopup();
                
                // Show success message
                std::string msg = "Icon deleted";
                if (deleteIconMarkerCount > 0) {
                    msg += " (" + std::to_string(deleteIconMarkerCount) + 
                           " marker";
                    if (deleteIconMarkerCount > 1) msg += "s";
                    msg += " removed)";
                }
                ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                ShowToast("Delete failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        // Cancel button (safe action, default)
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || 
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showDeleteIconModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderRebindModal(Model& model, KeymapManager& keymap) {
    ImGui::OpenPopup("Rebind Key");
    
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
            ImGui::CloseCurrentPopup();
            
            if (hasConflict) {
                ShowToast("Keybinding updated (conflict resolved)", 
                         Toast::Type::Warning);
            } else {
                ShowToast("Keybinding updated", Toast::Type::Success);
            }
        }
        if (!canApply) {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRebindModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderColorPickerModal(Model& model, History& history) {
    if (!showColorPickerModal) {
        return;
    }
    
    ImGui::OpenPopup("Color Picker");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    // Set fixed size to prevent resize animation
    ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_Always);
    
    bool modalOpen = true;
    if (ImGui::BeginPopupModal("Color Picker", &modalOpen, 
        ImGuiWindowFlags_NoResize)) {
        
        // Modal title/header
        if (colorPickerEditingTileId == -1) {
            ImGui::Text("Add New Color");
        } else {
            ImGui::Text("Edit Color");
        }
        ImGui::Separator();
        ImGui::Spacing();
        
        // Name input with auto-focus on first open
        ImGui::Text("Name:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        bool nameEnterPressed = ImGui::InputText(
            "##colorname", 
            colorPickerName, 
            sizeof(colorPickerName),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Color picker widget
        ImGui::Text("Color:");
        ImGuiColorEditFlags flags = 
            ImGuiColorEditFlags_AlphaBar | 
            ImGuiColorEditFlags_AlphaPreview |
            ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_DisplayHex;
        
        ImGui::ColorPicker4("##colorpicker", colorPickerColor, flags);
        
        ImGui::Spacing();
        
        // Preview: Original vs New (only when editing)
        if (colorPickerEditingTileId != -1) {
            ImGui::Text("Preview:");
            
            ImGui::BeginGroup();
            ImGui::Text("Original");
            ImGui::ColorButton("##original", 
                ImVec4(colorPickerOriginalColor[0], 
                      colorPickerOriginalColor[1],
                      colorPickerOriginalColor[2], 
                      colorPickerOriginalColor[3]),
                0, ImVec2(60, 60));
            ImGui::EndGroup();
            
            ImGui::SameLine(0, 20);
            
            ImGui::BeginGroup();
            ImGui::Text("New");
            ImGui::ColorButton("##new", 
                ImVec4(colorPickerColor[0], colorPickerColor[1],
                      colorPickerColor[2], colorPickerColor[3]),
                0, ImVec2(60, 60));
            ImGui::EndGroup();
            
            ImGui::Spacing();
        }
        
        ImGui::Separator();
        ImGui::Spacing();
        
        // Validation messages
        bool nameValid = strlen(colorPickerName) > 0 && 
                        strlen(colorPickerName) < 64;
        bool canSave = nameValid;
        
        // Check for empty name
        if (strlen(colorPickerName) == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Please enter a color name");
            ImGui::Spacing();
        }
        // Check for overly long name
        else if (strlen(colorPickerName) >= 64) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Color name is too long (max 63 characters)");
            canSave = false;
            ImGui::Spacing();
        }
        // Optional: Warn about duplicate names (non-blocking)
        else {
            bool duplicateFound = false;
            for (const auto& tile : model.palette) {
                if (tile.id != colorPickerEditingTileId &&
                    tile.name == std::string(colorPickerName)) {
                    duplicateFound = true;
                    break;
                }
            }
            if (duplicateFound) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                    "Note: A color with this name already exists");
                ImGui::Spacing();
            }
        }
        
        // Check palette size limit
        if (colorPickerEditingTileId == -1 && model.palette.size() >= 32) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "Palette is full (max 32 colors)");
            canSave = false;
            ImGui::Spacing();
        }
        
        // Keyboard shortcuts (Enter to save, Escape to cancel)
        bool shouldSave = false;
        bool shouldCancel = false;
        
        if (canSave && (nameEnterPressed || 
            ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
            shouldSave = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            shouldCancel = true;
        }
        
        // Action buttons
        if (!canSave) {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("Save", ImVec2(120, 0)) || shouldSave) {
            // Create color from picker
            Color color(colorPickerColor[0], colorPickerColor[1],
                       colorPickerColor[2], colorPickerColor[3]);
            std::string name = colorPickerName;
            
            if (colorPickerEditingTileId == -1) {
                // Add new color
                auto cmd = std::make_unique<AddPaletteColorCommand>(
                    name, color);
                history.AddCommand(std::move(cmd), model, true);
                
                // Select the newly added color
                m_canvasPanel.selectedTileId = model.palette.back().id;
                
                ShowToast("Color added: " + name, Toast::Type::Success);
            } else {
                // Update existing color
                auto cmd = std::make_unique<UpdatePaletteColorCommand>(
                    colorPickerEditingTileId, name, color);
                history.AddCommand(std::move(cmd), model, true);
                
                ShowToast("Color updated: " + name, Toast::Type::Success);
            }
            
            showColorPickerModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        if (!canSave) {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || shouldCancel) {
            showColorPickerModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        // Hint text for keyboard shortcuts
        ImGui::Spacing();
        ImGui::TextDisabled("Tip: Press Enter to save, Escape to cancel");
        
        // Delete button (only for editing existing colors, not Empty)
        if (colorPickerEditingTileId > 0) {
            ImGui::SameLine();
            
            // Check if color is in use
            bool inUse = model.IsPaletteColorInUse(colorPickerEditingTileId);
            
            if (inUse) {
                ImGui::PushStyleColor(ImGuiCol_Button, 
                    ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, 
                    ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            }
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                colorPickerDeleteRequested = true;
            }
            
            ImGui::PopStyleColor();
            
            // Show warning if in use
            if (inUse) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                    "Warning: This color is currently in use!");
                ImGui::TextWrapped(
                    "Deleting will replace all tiles with Empty (id=0)");
            }
        }
        
        ImGui::EndPopup();
    }
    
    // Handle delete confirmation (separate popup)
    if (colorPickerDeleteRequested) {
        ImGui::OpenPopup("Delete Color?");
        colorPickerDeleteRequested = false;
    }
    
    if (ImGui::BeginPopupModal("Delete Color?", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Are you sure you want to delete this color?");
        
        bool inUse = model.IsPaletteColorInUse(colorPickerEditingTileId);
        if (inUse) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "This color is in use.");
            ImGui::TextWrapped(
                "All tiles using this color will be replaced with Empty.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            // Delete the color (replace with Empty id=0)
            auto cmd = std::make_unique<RemovePaletteColorCommand>(
                colorPickerEditingTileId, 0);
            history.AddCommand(std::move(cmd), model, true);
            
            // If this was the selected tile, switch to Empty
            if (m_canvasPanel.selectedTileId == colorPickerEditingTileId) {
                m_canvasPanel.selectedTileId = 0;
            }
            
            ShowToast("Color deleted", Toast::Type::Info);
            
            showColorPickerModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Close modal if user clicked X
    if (!modalOpen) {
        showColorPickerModal = false;
    }
}

void UI::RenderNewProjectModal(App& app, Model& model) {
    ImGui::OpenPopup("New Project");
    
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
        bool isSquareSelected = (newProjectConfig.gridPreset == GridPreset::Square);
        if (isSquareSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
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
        bool isRectangleSelected = (newProjectConfig.gridPreset == GridPreset::Rectangle);
        if (isRectangleSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
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
        int cellWidth = (newProjectConfig.gridPreset == GridPreset::Square) ? 16 : 32;
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
                ShowToast("Please select a save location", 
                         Toast::Type::Error);
            } else {
                // Apply configuration to model
                model.meta.title = std::string(newProjectConfig.projectName);
                
                // Apply grid preset (sets tileWidth/tileHeight automatically)
                model.ApplyGridPreset(newProjectConfig.gridPreset);
                model.grid.cols = newProjectConfig.mapWidth;
                model.grid.rows = newProjectConfig.mapHeight;
                model.grid.locked = false;  // New project starts unlocked
                
                // Initialize other defaults
                model.InitDefaultPalette();
                model.InitDefaultKeymap();
                model.InitDefaultTheme("Dark");
                
                showNewProjectModal = false;
                ImGui::CloseCurrentPopup();
                
                // Create project with specified path
                app.NewProject(newProjectConfig.fullSavePath);
                
                // Transition to editor
                app.ShowEditor();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProjectModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderRecentProjectsList(App& app) {
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

void UI::RenderProjectTemplates() {
    // This is handled inline in RenderNewProjectModal
    // Could be extracted if we want a separate template browser
}

void UI::RenderProjectBrowserModal(App& app) {
    ImGui::OpenPopup("Recent Projects");
    
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Recent Projects", &showProjectBrowserModal, 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        
        ImGui::Text("All Recent Projects");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "Sorted by last modified");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Search bar
        static char searchFilter[256] = "";
        ImGui::SetNextItemWidth(-1.0f);  // Full width
        if (ImGui::InputTextWithHint("##projectsearch", 
                                     "🔍 Search projects...", 
                                     searchFilter, 
                                     sizeof(searchFilter))) {
            // Filter updates on every keystroke
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
        
        // Card dimensions for modal view (smaller than welcome screen)
        const float cardWidth = 250.0f;
        const float cardHeight = 180.0f;
        const float thumbnailHeight = 141.0f;  // 250*9/16
        const float titleHeight = 25.0f;
        const float cardSpacing = 15.0f;
        const int cardsPerRow = 3;
        
        // Load all thumbnails
        for (auto& project : recentProjects) {
            LoadThumbnailTexture(project);
        }
        
        // Render projects in a grid (with search filtering)
        int visibleIndex = 0;
        for (size_t i = 0; i < recentProjects.size(); ++i) {
            auto& project = recentProjects[i];
            
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
                ImVec2 textPos(cardPos.x + 8, cardPos.y + thumbnailHeight - 
                              titleHeight + 4);
                ImGui::SetCursorScreenPos(textPos);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                                  "%s", project.name.c_str());
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
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderWhatsNewPanel() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("What's New in Cartograph", &showWhatsNew)) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), 
            "Version 0.1.0 - Initial Release");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::BulletText("Welcome screen with project templates");
        ImGui::BulletText("Pan/zoom canvas with grid");
        ImGui::BulletText("Room and tile painting tools");
        ImGui::BulletText("Door and marker placement");
        ImGui::BulletText("PNG export with layers");
        ImGui::BulletText("Undo/redo support");
        ImGui::BulletText("Autosave functionality");
        ImGui::BulletText("Theme support (Dark/Light)");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
            "Coming Soon:");
        ImGui::BulletText("Reachability analysis");
        ImGui::BulletText("Minimap panel");
        ImGui::BulletText("Legend generation");
        ImGui::BulletText("SVG icon support");
        ImGui::BulletText("Web build");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showWhatsNew = false;
        }
    }
    ImGui::End();
}

void UI::RenderAutosaveRecoveryModal(App& app, Model& model) {
    ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Always);
    ImGui::OpenPopup("Autosave Recovery");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Autosave Recovery", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Unsaved Work Detected");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "Cartograph detected unsaved work from a previous session. "
            "Would you like to recover it?");
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Note: Recovering will load the autosaved data. You can "
            "manually save it when ready.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        if (ImGui::Button("Recover", ImVec2(buttonWidth, 0))) {
            // Load autosave
            std::string autosavePath = Platform::GetAutosaveDir() + "autosave.json";
            Model recoveredModel;
            if (IOJson::LoadFromFile(autosavePath, recoveredModel)) {
                model = recoveredModel;
                model.MarkDirty();  // Mark as dirty so user must save
                ShowToast("Recovered from autosave", Toast::Type::Success);
                app.ShowEditor();
            } else {
                ShowToast("Failed to load autosave", Toast::Type::Error);
            }
            showAutosaveRecoveryModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        if (ImGui::Button("Discard", ImVec2(buttonWidth, 0))) {
            // Clean up autosave and continue
            std::string autosaveDir = Platform::GetAutosaveDir();
            try {
                std::filesystem::remove(autosaveDir + "autosave.json");
                std::filesystem::remove(autosaveDir + "metadata.json");
            } catch (...) {
                // Ignore errors
            }
            showAutosaveRecoveryModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderLoadingModal(App& app, Model& model, JobQueue& jobs, 
                            IconManager& icons) {
    if (!showLoadingModal) return;
    
    ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_Always);
    ImGui::OpenPopup("Loading Project");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Loading Project", nullptr,
                               ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoCollapse)) {
        ImGui::Spacing();
        
        // Title
        ImGui::TextColored(
            ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
            "Opening Project"
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // File name (truncated if too long)
        std::string displayName = loadingFileName;
        if (displayName.length() > 45) {
            displayName = "..." + displayName.substr(
                displayName.length() - 42
            );
        }
        ImGui::Text("%s", displayName.c_str());
        
        ImGui::Spacing();
        
        // Indeterminate progress bar (pulsing animation)
        float time = static_cast<float>(ImGui::GetTime());
        float progress = (sinf(time * 3.0f) + 1.0f) * 0.5f;  // 0.0-1.0 pulse
        
        ImGui::ProgressBar(
            progress,
            ImVec2(-1, 0),
            ""  // No text overlay
        );
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Cancel button
        float buttonWidth = 120.0f;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - buttonWidth) * 0.5f
        );
        
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            loadingCancelled = true;
        }
        
        ImGui::EndPopup();
    }
}

void UI::RenderQuitConfirmationModal(App& app, Model& model) {
    ImGui::SetNextWindowSize(ImVec2(450, 180), ImGuiCond_Always);
    ImGui::OpenPopup("Unsaved Changes");
    
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
            "before quitting?");
        ImGui::Spacing();
        
        if (app.GetState() != AppState::Editor) {
            ImGui::TextDisabled("Current project has not been saved.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showQuitConfirmationModal = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            // Quit without saving
            showQuitConfirmationModal = false;
            ImGui::CloseCurrentPopup();
            app.ForceQuit();
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            // Save then quit
            app.SaveProject();
            
            // Check if save was successful (dirty flag should be cleared)
            if (!model.dirty) {
                showQuitConfirmationModal = false;
                ImGui::CloseCurrentPopup();
                app.ForceQuit();
            } else {
                // Save failed or was canceled, stay in modal
                ShowToast("Please save the project before quitting", 
                         Toast::Type::Warning);
            }
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}

void UI::ApplyTemplate(ProjectTemplate tmpl) {
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

void UI::LoadRecentProjects() {
    recentProjects.clear();
    
    namespace fs = std::filesystem;
    
    // Get default projects directory
    std::string projectsDir = Platform::GetDefaultProjectsDir();
    
    // Check if directory exists
    if (!fs::exists(projectsDir) || !fs::is_directory(projectsDir)) {
        return;
    }
    
    try {
        // Scan directory for project folders
        std::vector<std::pair<fs::file_time_type, fs::path>> projectPaths;
        
        for (const auto& entry : fs::directory_iterator(projectsDir)) {
            if (!entry.is_directory()) continue;
            
            // Check if it has a project.json file
            fs::path projectJson = entry.path() / "project.json";
            if (!fs::exists(projectJson)) continue;
            
            // Get last modified time
            auto modTime = fs::last_write_time(entry.path());
            projectPaths.push_back({modTime, entry.path()});
        }
        
        // Sort by last modified time (newest first)
        std::sort(projectPaths.begin(), projectPaths.end(),
            [](const auto& a, const auto& b) {
                return a.first > b.first;
            });
        
        // Convert to RecentProject entries (limit to 10)
        size_t count = std::min(projectPaths.size(), size_t(10));
        for (size_t i = 0; i < count; ++i) {
            const auto& [modTime, path] = projectPaths[i];
            
            RecentProject project;
            project.path = path.string();
            project.name = path.filename().string();
            
            // Format last modified time
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                modTime - fs::file_time_type::clock::now() + 
                std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm = *std::localtime(&time_t);
            char timeStr[64];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &tm);
            project.lastModified = timeStr;
            
            // Set thumbnail path if preview.png exists
            fs::path previewPath = path / "preview.png";
            if (fs::exists(previewPath)) {
                project.thumbnailPath = previewPath.string();
            }
            
            recentProjects.push_back(project);
        }
        
    } catch (const std::exception& e) {
        // Failed to scan directory - clear list and continue
        recentProjects.clear();
    }
}

void UI::AddRecentProject(const std::string& path) {
    // Note: This function no longer saves to JSON.
    // Recent projects are now loaded dynamically by scanning the directory.
    // We keep this function as a no-op for now in case it's called elsewhere,
    // but it doesn't need to do anything since LoadRecentProjects() handles
    // everything.
    
    // The directory scan approach means:
    // - No file I/O failures
    // - Always shows current state
    // - Self-healing (deleted projects auto-remove)
    // - Simpler and more reliable
}

void UI::LoadThumbnailTexture(RecentProject& project) {
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

void UI::UnloadThumbnailTextures() {
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

unsigned int UI::GeneratePlaceholderTexture() {
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

void UI::UpdateNewProjectPath() {
    // Sanitize project name for filesystem
    std::string sanitized = newProjectConfig.projectName;
    
    // Replace invalid characters with underscores
    for (char& c : sanitized) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    
    // Build full path
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
        
        newProjectConfig.fullSavePath = dir + sanitized + 
#ifdef _WIN32
            "\\";
#else
            "/";
#endif
    }
}

void UI::ShowNewProjectFolderPicker() {
    // Callback struct to capture UI reference
    struct CallbackData {
        UI* ui;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    
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
                data->ui->ShowToast("Failed to open folder dialog", 
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
            data->ui->newProjectConfig.saveDirectory = folderPath;
            data->ui->UpdateNewProjectPath();
            
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

void UI::BuildFixedLayout(ImGuiID dockspaceId) {
    // Clear any existing layout
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, 
        ImGuiDockNodeFlags_DockSpace);
    
    // Set dockspace size to fill the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);
    
    // Split bottom: 28px from bottom for status bar
    ImGuiID bottomId = 0;
    ImGuiID topRestId = 0;
    ImGui::DockBuilderSplitNode(
        dockspaceId, ImGuiDir_Down, 28.0f / viewport->WorkSize.y, 
        &bottomId, &topRestId
    );
    
    // Split left: 220px from left for tools (fixed width, non-resizable)
    ImGuiID leftId = 0;
    ImGuiID remainingId = 0;
    ImGui::DockBuilderSplitNode(
        topRestId, ImGuiDir_Left, 220.0f / viewport->WorkSize.x, 
        &leftId, &remainingId
    );
    
    ImGuiID centerId = remainingId;  // Default: center takes all remaining space
    ImGuiID rightId = 0;
    
    if (showPropertiesPanel) {
        // 3-column mode: Split right for properties panel (320px)
        float rightWidth = 320.0f / (viewport->WorkSize.x - 220.0f);
        ImGui::DockBuilderSplitNode(
            remainingId, ImGuiDir_Right, rightWidth,
            &rightId, &centerId
        );
    }
    
    // Dock windows into their respective nodes
    ImGui::DockBuilderDockWindow("Cartograph/Tools", leftId);
    ImGui::DockBuilderDockWindow("Cartograph/Canvas", centerId);
    ImGui::DockBuilderDockWindow("Cartograph/Console", bottomId);
    
    if (showPropertiesPanel) {
        ImGui::DockBuilderDockWindow("Cartograph/Inspector", rightId);
    }
    
    // Configure node flags
    ImGuiDockNode* leftNode = ImGui::DockBuilderGetNode(leftId);
    ImGuiDockNode* centerNode = ImGui::DockBuilderGetNode(centerId);
    ImGuiDockNode* bottomNode = ImGui::DockBuilderGetNode(bottomId);
    
    if (leftNode) {
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
        leftNode->LocalFlags |= ImGuiDockNodeFlags_NoResize;  // Non-resizable
    }
    
    if (centerNode) {
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        centerNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
    }
    
    if (showPropertiesPanel) {
        ImGuiDockNode* rightNode = ImGui::DockBuilderGetNode(rightId);
        if (rightNode) {
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
        }
    }
    
    if (bottomNode) {
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton;
        bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoResize;  // Non-resizable
    }
    
    // Finish building
    ImGui::DockBuilderFinish(dockspaceId);
}

void UI::ImportIcon(IconManager& iconManager, JobQueue& jobs) {
    // Use SDL3 native file dialog
    
    // Setup filters for image files
    static SDL_DialogFileFilter filters[] = {
        { "All Images", "png;jpg;jpeg;bmp;gif;tga;webp" },
        { "PNG Files", "png" },
        { "JPEG Files", "jpg;jpeg" },
        { "BMP Files", "bmp" },
        { "GIF Files", "gif" },
        { "TGA Files", "tga" },
        { "WebP Files", "webp" }
    };
    
    // Callback struct to capture references
    struct CallbackData {
        UI* ui;
        IconManager* iconManager;
        JobQueue* jobs;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->iconManager = &iconManager;
    dataPtr->jobs = &jobs;
    
    // Show native file dialog
    SDL_ShowOpenFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open file dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a file
            std::string path = filelist[0];
            
            // Extract base name from path
            namespace fs = std::filesystem;
            std::string baseName = fs::path(path).stem().string();
            
            // Generate unique name
            std::string iconName = data->iconManager->GenerateUniqueName(baseName);
            
            // Set loading state
            data->ui->isImportingIcon = true;
            data->ui->importingIconName = iconName;
            
            // Transfer ownership to job callback using shared_ptr
            // (job callback outlives this SDL callback)
            auto dataShared = std::shared_ptr<CallbackData>(data.release());
            
            // Enqueue processing job
            dataShared->jobs->Enqueue(
                JobType::ProcessIcon,
                [path, iconName, dataShared]() {
                    std::vector<uint8_t> pixels;
                    int width, height;
                    std::string errorMsg;
                    
                    if (!IconManager::ProcessIconFromFile(path, pixels, 
                                                          width, height, errorMsg)) {
                        throw std::runtime_error(errorMsg);
                    }
                    
                    if (!dataShared->iconManager->AddIconFromMemory(
                            iconName, pixels.data(), width, height, "marker")) {
                        throw std::runtime_error("Failed to add icon to memory");
                    }
                },
                [dataShared, iconName](bool success, const std::string& error) {
                    dataShared->ui->isImportingIcon = false;
                    
                    if (success) {
                        // Build atlas on main thread
                        dataShared->iconManager->BuildAtlas();
                        
                        dataShared->ui->ShowToast("Icon imported: " + iconName, 
                                          Toast::Type::Success, 2.0f);
                        dataShared->ui->m_canvasPanel.selectedIconName = iconName;
                    } else {
                        dataShared->ui->ShowToast("Failed to import: " + error, 
                                          Toast::Type::Error, 3.0f);
                    }
                    
                    // shared_ptr cleans up automatically when all lambdas done
                }
            );
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now, could pass SDL window)
        nullptr,
        // Filters
        filters,
        // Number of filters
        7,
        // Default location
        nullptr,
        // Allow multiple files
        false
    );
}

void UI::ShowSaveProjectDialog(App& app) {
    // Callback struct
    struct CallbackData {
        UI* ui;
        App* app;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->app = &app;
    
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
                data->ui->ShowToast("Failed to open folder dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a folder
            std::string folderPath = filelist[0];
            
            // Save project to folder
            data->app->SaveProjectFolder(folderPath);
            
            // unique_ptr cleans up automatically at end of scope
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Default location
        nullptr,
        // Allow multiple folders
        false
    );
}

void UI::ShowExportPackageDialog(App& app) {
    // Callback struct
    struct CallbackData {
        UI* ui;
        App* app;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->app = &app;
    
    // Setup filter for .cart files
    static SDL_DialogFileFilter filter = { 
        "Cartograph Package", "cart" 
    };
    
    // Show native save file dialog
    SDL_ShowSaveFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filterIndex) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open save dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a path
            std::string path = filelist[0];
            
            // Ensure .cart extension
            if (path.size() < 5 || 
                path.substr(path.size() - 5) != ".cart") {
                path += ".cart";
            }
            
            // Export package
            data->app->ExportPackage(path);
            
            // unique_ptr cleans up automatically at end of scope
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Filters
        &filter,
        // Number of filters
        1,
        // Default location
        nullptr
    );
}

void UI::ShowExportPngDialog(App& app) {
    // Callback struct
    struct CallbackData {
        UI* ui;
        App* app;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->ui = this;
    dataPtr->app = &app;
    
    // Setup filters for PNG files
    static SDL_DialogFileFilter filters[] = {
        { "PNG Image", "png" },
        { "All Files", "*" }
    };
    
    // Show native save file dialog
    SDL_ShowSaveFileDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filterIndex) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->ui->ShowToast("Failed to open save dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a path
            std::string path = filelist[0];
            
            // Ensure .png extension
            if (path.size() < 4 || 
                path.substr(path.size() - 4) != ".png") {
                path += ".png";
            }
            
            // Check for empty content
            ContentBounds bounds = 
                data->app->GetModel().CalculateContentBounds();
            
            if (bounds.isEmpty) {
                data->ui->ShowToast(
                    "Cannot export: No content drawn yet",
                    Toast::Type::Error);
                return;
            }
            
            // Export PNG
            data->app->ExportPng(path);
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Filters
        filters,
        // Number of filters
        2,
        // Default location
        nullptr
    );
}

void UI::HandleDroppedFile(const std::string& filePath, App& app,
                           JobQueue& jobs, IconManager& icons) {
    // Check if we're in Welcome Screen
    if (app.GetState() == AppState::Welcome) {
        // Try to import as project
        bool isValidProject = false;
        
        // Check if it's a .cart file
        if (filePath.size() >= 5 && 
            filePath.substr(filePath.size() - 5) == ".cart") {
            // Validate it's a real file and has ZIP magic bytes
            namespace fs = std::filesystem;
            if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
                isValidProject = true;
            }
        }
        // Check if it's a project folder
        else if (ProjectFolder::IsProjectFolder(filePath)) {
            isValidProject = true;
        }
        
        if (isValidProject) {
            // Start async loading
            showLoadingModal = true;
            loadingFilePath = filePath;
            loadingCancelled = false;
            loadingStartTime = Platform::GetTime();
            
            // Extract filename for display
            size_t lastSlash = filePath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                loadingFileName = filePath.substr(lastSlash + 1);
            } else {
                loadingFileName = filePath;
            }
            
            // Create shared model to load into
            auto loadedModel = std::make_shared<Model>();
            auto tempIcons = std::make_shared<IconManager>();
            
            // Capture necessary variables
            std::string capturedPath = filePath;
            
            // Enqueue async loading job
            jobs.Enqueue(
                JobType::LoadProject,
                [capturedPath, loadedModel, tempIcons, this]() {
                    // Check for cancellation before starting
                    if (loadingCancelled) {
                        throw std::runtime_error("Cancelled by user");
                    }
                    
                    bool success = false;
                    bool isCartFile = (capturedPath.size() >= 5 && 
                                      capturedPath.substr(capturedPath.size() - 5) == ".cart");
                    if (isCartFile) {
                        success = Package::Load(capturedPath, *loadedModel, 
                                              tempIcons.get());
                    } else {
                        success = ProjectFolder::Load(capturedPath, *loadedModel,
                                                     tempIcons.get());
                    }
                    
                    if (!success) {
                        throw std::runtime_error("Failed to load project");
                    }
                },
                [this, &app, loadedModel, tempIcons, &icons, capturedPath](
                    bool success, const std::string& error) {
                    showLoadingModal = false;
                    
                    if (success && !loadingCancelled) {
                        // Swap loaded model into app (main thread safe)
                        app.OpenProject(capturedPath);
                        
                        // Merge loaded icons into main icon manager
                        // (Icons were already added to tempIcons in background)
                        icons.BuildAtlas();  // Rebuild atlas on main thread
                        
                        // Transition to editor
                        app.ShowEditor();
                        
                        ShowToast("Project loaded", Toast::Type::Success);
                    } else if (loadingCancelled) {
                        ShowToast("Loading cancelled", Toast::Type::Info);
                    } else {
                        ShowToast("Failed to load project: " + error, 
                                 Toast::Type::Error);
                    }
                }
            );
        } else {
            // Invalid format
            ShowToast(
                "Invalid format. Drop a .cart file or project folder.",
                Toast::Type::Warning,
                4.0f
            );
        }
    } else {
        // In Editor state - handle as icon import (existing behavior)
        droppedFilePath = filePath;
        hasDroppedFile = true;
    }
}



} // namespace Cartograph
