#include "NativeMenu_imgui.h"
#include "../App.h"
#include "../Model.h"
#include "../Canvas.h"
#include "../History.h"
#include "../Icons.h"
#include "../Jobs.h"
#include "System.h"
#include <imgui.h>

namespace Cartograph {

NativeMenuImGui::NativeMenuImGui()
    : m_app(nullptr)
    , m_model(nullptr)
    , m_canvas(nullptr)
    , m_history(nullptr)
    , m_icons(nullptr)
    , m_jobs(nullptr)
    , m_showPropertiesPanel(nullptr)
    , m_showGrid(nullptr)
{
}

NativeMenuImGui::~NativeMenuImGui() {
}

void NativeMenuImGui::Initialize() {
    // No special initialization needed for ImGui menus
}

void NativeMenuImGui::Update(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    // Store references for rendering
    m_app = &app;
    m_model = &model;
    m_canvas = &canvas;
    m_history = &history;
    m_icons = &icons;
    m_jobs = &jobs;
    m_showGrid = &canvas.showGrid;
    
    // Update action callbacks that need model/history access
    m_callbacks["edit.undo"] = [&history, &model]() {
        if (history.CanUndo()) {
            history.Undo(model);
        }
    };
    
    m_callbacks["edit.redo"] = [&history, &model]() {
        if (history.CanRedo()) {
            history.Redo(model);
        }
    };
    
    m_callbacks["view.zoom_in"] = [&canvas]() {
        canvas.SetZoom(canvas.zoom * 1.2f);
    };
    
    m_callbacks["view.zoom_out"] = [&canvas]() {
        canvas.SetZoom(canvas.zoom / 1.2f);
    };
    
    m_callbacks["view.zoom_reset"] = [&canvas]() {
        canvas.SetZoom(2.5f);
    };
    
    m_callbacks["assets.import_icon"] = [this]() {
        // This needs access to UI::ImportIcon
        // Will be handled through external callback
    };
}

void NativeMenuImGui::Render() {
    if (!m_app || !m_model || !m_canvas || !m_history) {
        return;
    }
    
    RenderMenuBar();
}

void NativeMenuImGui::SetCallback(
    const std::string& action,
    MenuCallback callback
) {
    m_callbacks[action] = callback;
}

void NativeMenuImGui::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        // Check if we're in Editor state (vs Welcome screen)
        bool isEditor = m_app && (m_app->GetState() == AppState::Editor);
        
        // File Menu
        if (ImGui::BeginMenu("File")) {
            // New project (always available)
            std::string newShortcut = Platform::FormatShortcut("N");
            if (ImGui::MenuItem("New Project...", newShortcut.c_str())) {
                auto it = m_callbacks.find("file.new");
                if (it != m_callbacks.end()) it->second();
            }
            
            // Open project (always available)
            std::string openShortcut = Platform::FormatShortcut("O");
            if (ImGui::MenuItem("Open Project...", openShortcut.c_str())) {
                auto it = m_callbacks.find("file.open");
                if (it != m_callbacks.end()) it->second();
            }
            
            // Editor-only items
            if (isEditor) {
                ImGui::Separator();
                
                // Save
                std::string saveShortcut = Platform::FormatShortcut("S");
                if (ImGui::MenuItem("Save", saveShortcut.c_str())) {
                    auto it = m_callbacks.find("file.save");
                    if (it != m_callbacks.end()) it->second();
                }
                
                // Save As
                std::string saveAsShortcut = 
                    Platform::FormatShortcut("Shift+S");
                if (ImGui::MenuItem("Save As...", saveAsShortcut.c_str())) {
                    auto it = m_callbacks.find("file.save_as");
                    if (it != m_callbacks.end()) it->second();
                }
                
                ImGui::Separator();
                
                // Export Package
                std::string exportPkgShortcut = 
                    Platform::FormatShortcut("Shift+E");
                if (ImGui::MenuItem("Export Package (.cart)...", 
                                   exportPkgShortcut.c_str())) {
                    auto it = m_callbacks.find("file.export_package");
                    if (it != m_callbacks.end()) it->second();
                }
                
                // Export PNG
                std::string exportPngShortcut = Platform::FormatShortcut("E");
                if (ImGui::MenuItem("Export PNG...", 
                                   exportPngShortcut.c_str())) {
                    auto it = m_callbacks.find("file.export_png");
                    if (it != m_callbacks.end()) it->second();
                }
                
                ImGui::Separator();
            }
            
            // Exit (Windows/Linux uses "Exit" instead of "Quit")
#ifdef _WIN32
            std::string quitLabel = "Exit";
#else
            std::string quitLabel = "Quit";
#endif
            std::string quitShortcut = Platform::FormatShortcut("Q");
            if (ImGui::MenuItem(quitLabel.c_str(), quitShortcut.c_str())) {
                auto it = m_callbacks.find("file.quit");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::EndMenu();
        }
        
        // Edit Menu (only show in Editor)
        if (isEditor && ImGui::BeginMenu("Edit")) {
            bool canUndo = m_history->CanUndo();
            bool canRedo = m_history->CanRedo();
            
            std::string undoShortcut = Platform::FormatShortcut("Z");
            if (ImGui::MenuItem("Undo", undoShortcut.c_str(), 
                               false, canUndo)) {
                auto it = m_callbacks.find("edit.undo");
                if (it != m_callbacks.end()) it->second();
            }
            
            std::string redoShortcut = Platform::FormatShortcut("Y");
            if (ImGui::MenuItem("Redo", redoShortcut.c_str(), 
                               false, canRedo)) {
                auto it = m_callbacks.find("edit.redo");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::Separator();
            
            std::string settingsShortcut = Platform::FormatShortcut(",");
            if (ImGui::MenuItem("Settings...", settingsShortcut.c_str())) {
                auto it = m_callbacks.find("edit.settings");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::EndMenu();
        }
        
        // View Menu (only show in Editor)
        if (isEditor && ImGui::BeginMenu("View")) {
            std::string propPanelShortcut = Platform::FormatShortcut("P");
            bool showProps = m_showPropertiesPanel ? 
                            *m_showPropertiesPanel : false;
            if (ImGui::MenuItem("Hierarchy Panel", 
                               propPanelShortcut.c_str(), showProps)) {
                auto it = m_callbacks.find("view.properties");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::Separator();
            
            if (m_showGrid) {
                ImGui::MenuItem("Show Grid", "G", m_showGrid);
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Zoom In", "=")) {
                auto it = m_callbacks.find("view.zoom_in");
                if (it != m_callbacks.end()) it->second();
            }
            if (ImGui::MenuItem("Zoom Out", "-")) {
                auto it = m_callbacks.find("view.zoom_out");
                if (it != m_callbacks.end()) it->second();
            }
            if (ImGui::MenuItem("Reset Zoom", "0")) {
                auto it = m_callbacks.find("view.zoom_reset");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::EndMenu();
        }
        
        // Assets Menu (only show in Editor)
        if (isEditor && ImGui::BeginMenu("Assets")) {
            if (ImGui::MenuItem("Import Icon...")) {
                auto it = m_callbacks.find("assets.import_icon");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::EndMenu();
        }
        
        // Help Menu
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Cartograph")) {
                auto it = m_callbacks.find("help.about");
                if (it != m_callbacks.end()) it->second();
            }
            
            if (ImGui::MenuItem("View License")) {
                auto it = m_callbacks.find("help.license");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Report Bug...")) {
                auto it = m_callbacks.find("help.report_bug");
                if (it != m_callbacks.end()) it->second();
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

} // namespace Cartograph

