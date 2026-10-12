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


void UI::StartAsyncProjectLoad(
    const std::string& filePath,
    App& app,
    JobQueue& jobs
) {
    m_modals.showLoadingModal = true;
    m_modals.loadingFilePath = filePath;
    m_modals.loadingCancelled = false;
    m_modals.loadingStartTime = Platform::GetTime();

    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        m_modals.loadingFileName = filePath.substr(lastSlash + 1);
    } else {
        m_modals.loadingFileName = filePath;
    }

    auto loadedModel = std::make_shared<Model>();
    auto tempIcons = std::make_shared<IconManager>();

    jobs.Enqueue(
        JobType::LoadProject,
        [filePath, loadedModel, tempIcons, this]() {
            if (m_modals.loadingCancelled) {
                throw std::runtime_error("Cancelled by user");
            }
            bool success = false;
            if (Package::HasCartExtension(filePath)) {
                success = Package::Load(
                    filePath, *loadedModel, tempIcons.get());
            } else {
                success = ProjectFolder::Load(
                    filePath, *loadedModel, tempIcons.get());
            }
            if (!success) {
                throw std::runtime_error("Failed to load project");
            }
        },
        [this, appPtr = &app, loadedModel, tempIcons, filePath](
            bool success, const std::string& error) {
            m_modals.showLoadingModal = false;
            if (success && !m_modals.loadingCancelled) {
                appPtr->InstallLoadedProject(
                    filePath, std::move(*loadedModel), *tempIcons);
                appPtr->ShowEditor();
            } else if (m_modals.loadingCancelled) {
                ShowToast("Loading cancelled", Toast::Type::Info);
            } else {
                ShowToast(
                    "Failed to load project: " + error,
                    Toast::Type::Error);
            }
        }
    );
}


void UI::HandleDroppedFile(const std::string& filePath, App& app,
                           JobQueue& jobs, IconManager& /*icons*/) {
    // Check if we're in Welcome Screen
    if (app.GetState() == AppState::Welcome) {
        // Try to import as project
        bool isValidProject = false;
        
        // Check if it's a .cart file
        if (Package::HasCartExtension(filePath)) {
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
            StartAsyncProjectLoad(filePath, app, jobs);
        } else {
            // Invalid format
            ShowToast(
                "Invalid format. Drop a .cart file or .cartproj folder.",
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
