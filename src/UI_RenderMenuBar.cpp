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


void UI::RenderMenuBar(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    if (ImGui::BeginMainMenuBar()) {
        // File Menu
        if (ImGui::BeginMenu("File")) {
            // New project
            std::string newShortcut = Platform::FormatShortcut("N");
            if (ImGui::MenuItem("New Project...", newShortcut.c_str())) {
                app.ShowNewProjectDialog();
            }
            
            // Open project
            std::string openShortcut = Platform::FormatShortcut("O");
            if (ImGui::MenuItem("Open Project...", openShortcut.c_str())) {
                app.ShowOpenProjectDialog();
            }
            
            ImGui::Separator();
            
            // Save
            std::string saveShortcut = Platform::FormatShortcut("S");
            if (ImGui::MenuItem("Save", saveShortcut.c_str())) {
                app.SaveProject();
            }
            
            // Save As
            std::string saveAsShortcut = 
                Platform::FormatShortcut("Shift+S");
            if (ImGui::MenuItem("Save As...", saveAsShortcut.c_str())) {
                ShowSaveProjectDialog(app);
            }
            
            ImGui::Separator();
            
            // Export Package
            std::string exportPkgShortcut = 
                Platform::FormatShortcut("Shift+E");
            if (ImGui::MenuItem("Export Package (.cart)...", 
                               exportPkgShortcut.c_str())) {
                ShowExportPackageDialog(app);
            }
            
            // Export PNG
            std::string exportPngShortcut = Platform::FormatShortcut("E");
            if (ImGui::MenuItem("Export PNG...", 
                               exportPngShortcut.c_str())) {
                m_modals.showExportModal = true;
            }
            
            ImGui::Separator();
            
            // Quit (platform-specific label)
#ifdef _WIN32
            std::string quitLabel = "Exit";
#else
            std::string quitLabel = "Quit";
#endif
            std::string quitShortcut = Platform::FormatShortcut("Q");
            if (ImGui::MenuItem(quitLabel.c_str(), quitShortcut.c_str())) {
                app.RequestQuit();
            }
            
            ImGui::EndMenu();
        }
        
        // Edit Menu
        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = history.CanUndo();
            bool canRedo = history.CanRedo();
            
            std::string undoShortcut = Platform::FormatShortcut("Z");
            if (ImGui::MenuItem("Undo", undoShortcut.c_str(), 
                               false, canUndo)) {
                history.Undo(model);
            }
            
            std::string redoShortcut = Platform::FormatShortcut("Y");
            if (ImGui::MenuItem("Redo", redoShortcut.c_str(), 
                               false, canRedo)) {
                history.Redo(model);
            }
            
            ImGui::Separator();
            
            // Selection operations
            bool hasSelection = m_canvasPanel.hasSelection && 
                               !m_canvasPanel.currentSelection.IsEmpty();
            bool hasClipboard = !m_canvasPanel.clipboard.IsEmpty();
            
            std::string cutShortcut = Platform::FormatShortcut("X");
            if (ImGui::MenuItem("Cut", cutShortcut.c_str(), 
                               false, hasSelection)) {
                m_canvasPanel.CopySelection(model);
                m_canvasPanel.DeleteSelection(model, history);
            }
            
            std::string copyShortcut = Platform::FormatShortcut("C");
            if (ImGui::MenuItem("Copy", copyShortcut.c_str(), 
                               false, hasSelection)) {
                m_canvasPanel.CopySelection(model);
            }
            
            std::string pasteShortcut = Platform::FormatShortcut("V");
            if (ImGui::MenuItem("Paste", pasteShortcut.c_str(), 
                               false, hasClipboard)) {
                m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
                m_canvasPanel.EnterPasteMode();
            }
            
            std::string deleteShortcut = Platform::FormatShortcut("Delete");
            if (ImGui::MenuItem("Delete", deleteShortcut.c_str(), 
                               false, hasSelection)) {
                m_canvasPanel.DeleteSelection(model, history);
            }
            
            ImGui::Separator();
            
            std::string selectAllShortcut = Platform::FormatShortcut("A");
            if (ImGui::MenuItem("Select All", selectAllShortcut.c_str())) {
                m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
                m_canvasPanel.SelectAll(model);
            }
            
            ImGui::Separator();
            
            std::string settingsShortcut = Platform::FormatShortcut(",");
            if (ImGui::MenuItem("Settings...", settingsShortcut.c_str())) {
                m_modals.showSettingsModal = true;
            }
            
            ImGui::EndMenu();
        }
        
        // View Menu
        if (ImGui::BeginMenu("View")) {
            std::string propPanelShortcut = Platform::FormatShortcut("P");
            if (ImGui::MenuItem("Hierarchy Panel", propPanelShortcut.c_str(),
                               showPropertiesPanel)) {
                showPropertiesPanel = !showPropertiesPanel;
                m_layoutInitialized = false;  // Trigger layout rebuild
            }
            
            ImGui::Separator();
            
            ImGui::MenuItem("Show Grid", "G", &canvas.showGrid);
            
            // Marker Labels submenu
            if (ImGui::BeginMenu("Marker Labels")) {
                bool isAlways = (canvas.markerLabelMode == MarkerLabelMode::Always);
                bool isNever = (canvas.markerLabelMode == MarkerLabelMode::Never);
                bool isHover = (canvas.markerLabelMode == MarkerLabelMode::HoverOnly);
                
                if (ImGui::MenuItem("Always Show", nullptr, isAlways)) {
                    canvas.markerLabelMode = MarkerLabelMode::Always;
                }
                if (ImGui::MenuItem("Never Show", nullptr, isNever)) {
                    canvas.markerLabelMode = MarkerLabelMode::Never;
                }
                if (ImGui::MenuItem("Hover Only", nullptr, isHover)) {
                    canvas.markerLabelMode = MarkerLabelMode::HoverOnly;
                }
                ImGui::EndMenu();
            }
            
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
        
        // Assets Menu
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
        
        // Help Menu
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Cartograph")) {
                m_modals.showAboutModal = true;
            }
            
            if (ImGui::MenuItem("View License")) {
                // Open LICENSE file
                std::string licensePath = Platform::GetAssetsDir() + 
                                         "../LICENSE";
                Platform::OpenURL("file://" + licensePath);
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Report Bug...")) {
                Platform::OpenURL(
                    "https://github.com/Unveil-gg/Cartograph/issues/new"
                );
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




} // namespace Cartograph
