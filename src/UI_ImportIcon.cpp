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
            
            data->ui->EnqueueIconImport(
                data->ui, *data->iconManager, *data->jobs,
                path, iconName);
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
    
    // Build default filename from project title
    std::string defaultFilename = 
        SanitizeFilename(app.GetModel().meta.title) + ".cart";
    
    // Build default path in Downloads folder
    std::string defaultPath;
    const char* downloadsDir = SDL_GetUserFolder(SDL_FOLDER_DOWNLOADS);
    if (downloadsDir) {
        defaultPath = std::string(downloadsDir) + defaultFilename;
    } else {
        defaultPath = defaultFilename;
    }
    
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
            if (!Package::HasCartExtension(path)) {
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
        // Default path (Downloads folder + filename)
        defaultPath.c_str()
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
    
    // Build default filename from project title
    std::string defaultFilename = 
        SanitizeFilename(app.GetModel().meta.title) + ".png";
    
    // Build default path in Downloads folder
    std::string defaultPath;
    const char* downloadsDir = SDL_GetUserFolder(SDL_FOLDER_DOWNLOADS);
    if (downloadsDir) {
        defaultPath = std::string(downloadsDir) + defaultFilename;
    } else {
        defaultPath = defaultFilename;
    }
    
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
        // Default path (Downloads folder + filename)
        defaultPath.c_str()
    );
}




} // namespace Cartograph
