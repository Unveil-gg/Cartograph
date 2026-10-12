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
        [](void* userdata, const char* const* filelist, int /*filter*/) {
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
