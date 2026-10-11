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
void SDL_CursorDeleter::operator()(SDL_Cursor* cursor) const {
    if (cursor) {
        SDL_DestroyCursor(cursor);
    }
}

namespace Cartograph {


/**
 * Sanitize a string for use as a filename.
 * Replaces spaces and special characters with dashes, collapses multiple
 * dashes, and trims leading/trailing dashes.
 * @param input The string to sanitize
 * @return Sanitized filename (without extension)
 */
std::string SanitizeFilename(const std::string& input) {
    if (input.empty()) {
        return "Untitled";
    }
    
    std::string result;
    result.reserve(input.size());
    
    bool lastWasDash = true;  // Start true to trim leading dashes
    
    for (char c : input) {
        // Replace spaces and invalid filename characters with dash
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == '\t' || c == '\n' || c == '\r') {
            if (!lastWasDash) {
                result += '-';
                lastWasDash = true;
            }
        } else {
            result += c;
            lastWasDash = false;
        }
    }
    
    // Trim trailing dashes
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    
    // Fallback if result is empty after sanitization
    if (result.empty()) {
        return "Untitled";
    }
    
    return result;
}


/**
 * Snapshot a room for property-edit commands.
 * @param room Room to copy, or null
 * @return Snapshot of name, color, notes, and tags
 */
RoomPropertiesSnapshot CaptureRoomProps(const Room* room) {
    RoomPropertiesSnapshot snap;
    if (room) {
        snap.name = room->name;
        snap.color = room->color;
        snap.notes = room->notes;
        snap.tags = room->tags;
    }
    return snap;
}

/**
 * Case-insensitive substring test used by hierarchy search.
 * @param haystack Text to search
 * @param needle Query, empty matches everything
 * @return true when needle occurs in haystack
 */
bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    std::string lowerHaystack = haystack;
    std::string lowerNeedle = needle;
    std::transform(lowerHaystack.begin(), lowerHaystack.end(),
                   lowerHaystack.begin(), ::tolower);
    std::transform(lowerNeedle.begin(), lowerNeedle.end(),
                   lowerNeedle.begin(), ::tolower);
    return lowerHaystack.find(lowerNeedle) != std::string::npos;
}


/**
 * Decode an icon off the main thread, then add it on the main thread.
 * @param ui UI that shows progress and the result toast
 * @param icons Icon manager updated only from the callback
 * @param jobs Queue that runs the decode
 * @param path Source image path
 * @param iconName Unique name to store
 */
void UI::EnqueueIconImport(
    UI* ui,
    IconManager& icons,
    JobQueue& jobs,
    const std::string& path,
    const std::string& iconName
) {
    struct DecodedIcon {
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
    };
    ui->isImportingIcon = true;
    ui->importingIconName = iconName;
    auto decoded = std::make_shared<DecodedIcon>();
    IconManager* iconPtr = &icons;

    jobs.Enqueue(
        JobType::ProcessIcon,
        [path, decoded]() {
            std::string errorMsg;
            if (!IconManager::ProcessIconFromFile(
                    path, decoded->pixels, decoded->width,
                    decoded->height, errorMsg)) {
                throw std::runtime_error(errorMsg);
            }
        },
        [ui, iconPtr, iconName, decoded](
            bool success, const std::string& error) {
            ui->isImportingIcon = false;
            if (!success) {
                ui->ShowToast(
                    "Failed to import: " + error,
                    Toast::Type::Error, 3.0f);
                return;
            }
            if (!iconPtr->AddIconFromMemory(
                    iconName, decoded->pixels.data(),
                    decoded->width, decoded->height, "marker")) {
                ui->ShowToast(
                    "Failed to add icon to memory",
                    Toast::Type::Error, 3.0f);
                return;
            }
            iconPtr->BuildAtlas();
            ui->ShowToast(
                "Icon imported: " + iconName,
                Toast::Type::Success, 2.0f);
            ui->m_canvasPanel.selectedIconName = iconName;
        }
    );
}


UI::UI() : m_modals(*this), m_welcomeScreen(*this) {
    // Connect canvas panel to UI state for shared members
    m_canvasPanel.showPropertiesPanel = &showPropertiesPanel;
    m_canvasPanel.layoutInitialized = &m_layoutInitialized;
    m_canvasPanel.modals = &m_modals;
    
    // Create platform-specific native menu (but don't initialize yet)
    m_nativeMenu = CreateNativeMenu();
}


UI::~UI() {
    m_welcomeScreen.UnloadThumbnailTextures();
}


void UI::InitializeNativeMenu() {
    if (m_nativeMenu) {
        m_nativeMenu->Initialize();
    }
}


void UI::SetupDockspace() {
    // Docking setup happens in first frame of Render()
}


void UI::UpdateMenu(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    // Initialize menu callbacks once
    static bool menuCallbacksInitialized = false;
    if (!menuCallbacksInitialized) {
        InitializeMenuCallbacks(app);
        menuCallbacksInitialized = true;
    }
    
    // Set callbacks that need icons/jobs reference
    m_nativeMenu->SetCallback("assets.import_icon", 
        [this, &icons, &jobs]() {
            ImportIcon(icons, jobs);
        }
    );
    
    // Set selection operation callbacks (need model/history/canvasPanel)
    m_nativeMenu->SetCallback("edit.cut", 
        [this, &model, &history]() {
            if (m_canvasPanel.hasSelection && 
                !m_canvasPanel.currentSelection.IsEmpty()) {
                m_canvasPanel.CopySelection(model);
                m_canvasPanel.DeleteSelection(model, history);
            }
        }
    );
    m_nativeMenu->SetCallback("edit.copy", 
        [this, &model]() {
            if (m_canvasPanel.hasSelection && 
                !m_canvasPanel.currentSelection.IsEmpty()) {
                m_canvasPanel.CopySelection(model);
            }
        }
    );
    m_nativeMenu->SetCallback("edit.paste", 
        [this]() {
            if (!m_canvasPanel.clipboard.IsEmpty()) {
                m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
                m_canvasPanel.EnterPasteMode();
            }
        }
    );
    m_nativeMenu->SetCallback("edit.delete", 
        [this, &model, &history]() {
            if (m_canvasPanel.hasSelection && 
                !m_canvasPanel.currentSelection.IsEmpty()) {
                m_canvasPanel.DeleteSelection(model, history);
            }
        }
    );
    m_nativeMenu->SetCallback("edit.selectAll", 
        [this, &model]() {
            m_canvasPanel.currentTool = CanvasPanel::Tool::Select;
            m_canvasPanel.SelectAll(model);
        }
    );
    
    // Update menu state (native on macOS, ImGui on Windows/Linux)
    m_nativeMenu->Update(app, model, canvas, history, icons, jobs);
    
    // For ImGui menus, we need to pass the hierarchy panel visibility pointer
    // This is a bit of a hack but necessary for the ImGui implementation
    if (!m_nativeMenu->IsNative()) {
        NativeMenuImGui* imguiMenu = 
            dynamic_cast<NativeMenuImGui*>(m_nativeMenu.get());
        if (imguiMenu) {
            imguiMenu->SetShowPropertiesPanel(&showPropertiesPanel);
        }
    }
    
    // Render menu (for ImGui implementations)
    m_nativeMenu->Render();
}




} // namespace Cartograph
