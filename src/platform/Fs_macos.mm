#include "Fs.h"
#include "../ProjectFolder.h"
#include <filesystem>

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>

namespace Platform {

/**
 * Validate if selected file is a valid .cart file.
 * @param path Path to file
 * @return true if valid .cart file
 */
static bool IsValidCartFile(const std::string& path) {
    namespace fs = std::filesystem;
    
    // Check extension
    if (path.size() < 5 || path.substr(path.size() - 5) != ".cart") {
        return false;
    }
    
    // Check file exists and is regular file
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        return false;
    }
    
    // Quick ZIP validation: check magic bytes (PK\x03\x04)
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return false;
    
    uint8_t magic[4];
    size_t read = fread(magic, 1, 4, file);
    fclose(file);
    
    if (read != 4) return false;
    
    // ZIP magic: 0x50 0x4B 0x03 0x04 (PK\x03\x04)
    return magic[0] == 0x50 && magic[1] == 0x4B && 
           magic[2] == 0x03 && magic[3] == 0x04;
}

/**
 * Validate if selected folder is a valid project folder.
 * @param path Path to folder
 * @return true if valid project folder
 */
static bool IsValidProjectFolder(const std::string& path) {
    return Cartograph::ProjectFolder::IsProjectFolder(path);
}

/**
 * Show alert for invalid selection.
 * @param path Path that was selected
 * @param allowFiles Whether files were allowed
 * @param allowFolders Whether folders were allowed
 */
static void ShowInvalidSelectionAlert(const std::string& path,
                                      bool allowFiles,
                                      bool allowFolders) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Invalid Project"];
        
        if (allowFiles && allowFolders) {
            [alert setInformativeText:
                @"Please select a .cart file or a folder containing "
                @"project.json"];
        } else if (allowFiles) {
            [alert setInformativeText:
                @"Please select a valid .cart file"];
        } else {
            [alert setInformativeText:
                @"Please select a folder containing project.json"];
        }
        
        [alert setAlertStyle:NSAlertStyleWarning];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    }
}

std::optional<std::string> ShowOpenDialogForImport(
    const std::string& title,
    bool allowFiles,
    bool allowFolders,
    const std::vector<std::string>& fileExtensions,
    const std::string& defaultPath
) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        
        // Configure panel
        [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [panel setCanChooseFiles:(allowFiles ? YES : NO)];
        [panel setCanChooseDirectories:(allowFolders ? YES : NO)];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanCreateDirectories:NO];
        
        // Set file type filter for .cart files
        if (allowFiles && !fileExtensions.empty()) {
            NSMutableArray* types = [NSMutableArray array];
            for (const auto& ext : fileExtensions) {
                [types addObject:[NSString stringWithUTF8String:ext.c_str()]];
            }
            [panel setAllowedFileTypes:types];
            [panel setAllowsOtherFileTypes:NO];
        }
        
        // Set default directory
        if (!defaultPath.empty()) {
            NSURL* url = [NSURL fileURLWithPath:
                [NSString stringWithUTF8String:defaultPath.c_str()]];
            [panel setDirectoryURL:url];
        }
        
        // Show modal dialog (blocks until user selects or cancels)
        NSModalResponse response = [panel runModal];
        
        if (response == NSModalResponseOK) {
            NSURL* url = [panel URL];
            if (!url) {
                return std::nullopt;
            }
            
            const char* pathCStr = [[url path] UTF8String];
            if (!pathCStr) {
                return std::nullopt;
            }
            
            std::string selectedPath(pathCStr);
            
            // Validate selection
            namespace fs = std::filesystem;
            bool isValid = false;
            
            if (fs::is_regular_file(selectedPath)) {
                // File selected - must be .cart
                if (allowFiles && IsValidCartFile(selectedPath)) {
                    isValid = true;
                }
            } else if (fs::is_directory(selectedPath)) {
                // Folder selected - must have project.json
                if (allowFolders && IsValidProjectFolder(selectedPath)) {
                    isValid = true;
                }
            }
            
            if (isValid) {
                return selectedPath;
            } else {
                // Show error alert
                ShowInvalidSelectionAlert(selectedPath, allowFiles, 
                                         allowFolders);
                return std::nullopt;
            }
        }
        
        // User cancelled
        return std::nullopt;
    }
}

} // namespace Platform

#endif // __APPLE__

