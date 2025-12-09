/**
 * Windows-specific file system operations.
 * Uses Win32 IFileOpenDialog for native file/folder selection.
 */

#ifdef _WIN32

#include "../Fs.h"
#include "../../ProjectFolder.h"
#include <filesystem>
#include <windows.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <comdef.h>

#pragma comment(lib, "Shlwapi.lib")

namespace Platform {

/**
 * Validate if selected file is a valid .cart file.
 * Checks extension and ZIP magic bytes.
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
    size_t bytesRead = fread(magic, 1, 4, file);
    fclose(file);
    
    if (bytesRead != 4) return false;
    
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
    const wchar_t* message;
    
    if (allowFiles && allowFolders) {
        message = L"Please select a .cart file or a .cartproj project folder";
    } else if (allowFiles) {
        message = L"Please select a valid .cart file";
    } else {
        message = L"Please select a .cartproj project folder";
    }
    
    MessageBoxW(nullptr, message, L"Invalid Project", 
                MB_OK | MB_ICONWARNING);
}

/**
 * Convert UTF-8 string to wide string for Win32 APIs.
 * @param str UTF-8 string
 * @return Wide string
 */
static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                                   static_cast<int>(str.size()), 
                                   nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                        static_cast<int>(str.size()), 
                        &result[0], size);
    return result;
}

/**
 * Convert wide string to UTF-8 for internal use.
 * @param wstr Wide string
 * @return UTF-8 string
 */
static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), 
                                   static_cast<int>(wstr.size()), 
                                   nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), 
                        static_cast<int>(wstr.size()), 
                        &result[0], size, nullptr, nullptr);
    return result;
}

std::optional<std::string> ShowOpenDialogForImport(
    const std::string& title,
    bool allowFiles,
    bool allowFolders,
    const std::vector<std::string>& fileExtensions,
    const std::string& defaultPath
) {
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }
    
    // Ensure COM is uninitialized on exit
    struct ComGuard {
        HRESULT hr;
        ~ComGuard() { 
            if (SUCCEEDED(hr) || hr == S_FALSE) CoUninitialize(); 
        }
    } comGuard{hr};
    
    IFileOpenDialog* pDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                          IID_IFileOpenDialog, 
                          reinterpret_cast<void**>(&pDialog));
    if (FAILED(hr)) {
        return std::nullopt;
    }
    
    // Smart pointer for automatic release
    struct DialogGuard {
        IFileOpenDialog* p;
        ~DialogGuard() { if (p) p->Release(); }
    } dialogGuard{pDialog};
    
    // Set title
    std::wstring wTitle = Utf8ToWide(title);
    pDialog->SetTitle(wTitle.c_str());
    
    // Configure options
    DWORD options = 0;
    pDialog->GetOptions(&options);
    
    if (allowFolders) {
        options |= FOS_PICKFOLDERS;
    }
    
    // Note: IFileOpenDialog with FOS_PICKFOLDERS only allows folders.
    // To allow both files AND folders, we need to NOT set FOS_PICKFOLDERS
    // and handle folder selection differently. For simplicity, if both
    // are allowed, we prioritize folder selection (user can navigate into
    // folders and select them, or select .cart files shown).
    // 
    // Actually, Windows doesn't natively support selecting both files and
    // folders in a single dialog. We'll use folder picker if allowFolders
    // is the primary mode, otherwise file picker with folder validation.
    
    if (allowFiles && allowFolders) {
        // Allow both: use file picker, but user can type folder path
        // or we accept if they select a folder through navigation
        options &= ~FOS_PICKFOLDERS;
        options |= FOS_FILEMUSTEXIST;
    } else if (allowFolders) {
        options |= FOS_PICKFOLDERS;
    } else {
        options |= FOS_FILEMUSTEXIST;
    }
    
    pDialog->SetOptions(options);
    
    // Set file type filter for .cart files (only if not folder-only mode)
    if (allowFiles && !fileExtensions.empty() && !allowFolders) {
        // Build filter string like "*.cart"
        std::wstring filterSpec = L"*." + Utf8ToWide(fileExtensions[0]);
        for (size_t i = 1; i < fileExtensions.size(); ++i) {
            filterSpec += L";*." + Utf8ToWide(fileExtensions[i]);
        }
        
        COMDLG_FILTERSPEC filters[] = {
            { L"Cart Files", filterSpec.c_str() },
            { L"All Files", L"*.*" }
        };
        pDialog->SetFileTypes(2, filters);
        pDialog->SetFileTypeIndex(1);
    }
    
    // Set default directory
    if (!defaultPath.empty()) {
        IShellItem* pDefaultFolder = nullptr;
        std::wstring wDefaultPath = Utf8ToWide(defaultPath);
        hr = SHCreateItemFromParsingName(wDefaultPath.c_str(), nullptr,
                                         IID_IShellItem,
                                         reinterpret_cast<void**>(
                                             &pDefaultFolder));
        if (SUCCEEDED(hr) && pDefaultFolder) {
            pDialog->SetFolder(pDefaultFolder);
            pDefaultFolder->Release();
        }
    }
    
    // Show dialog
    hr = pDialog->Show(nullptr);
    if (FAILED(hr)) {
        // User cancelled or error
        return std::nullopt;
    }
    
    // Get result
    IShellItem* pItem = nullptr;
    hr = pDialog->GetResult(&pItem);
    if (FAILED(hr) || !pItem) {
        return std::nullopt;
    }
    
    struct ItemGuard {
        IShellItem* p;
        ~ItemGuard() { if (p) p->Release(); }
    } itemGuard{pItem};
    
    // Get path
    PWSTR pszFilePath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (FAILED(hr) || !pszFilePath) {
        return std::nullopt;
    }
    
    std::string selectedPath = WideToUtf8(pszFilePath);
    CoTaskMemFree(pszFilePath);
    
    // Validate selection
    namespace fs = std::filesystem;
    bool isValid = false;
    
    if (fs::is_regular_file(selectedPath)) {
        // File selected - must be .cart
        if (allowFiles && IsValidCartFile(selectedPath)) {
            isValid = true;
        }
    } else if (fs::is_directory(selectedPath)) {
        // Folder selected - must be valid .cartproj or project folder
        if (allowFolders && IsValidProjectFolder(selectedPath)) {
            isValid = true;
        }
    }
    
    if (isValid) {
        return selectedPath;
    } else {
        // Show error alert
        ShowInvalidSelectionAlert(selectedPath, allowFiles, allowFolders);
        return std::nullopt;
    }
}

} // namespace Platform

#endif // _WIN32
