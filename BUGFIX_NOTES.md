# Bug Fix Notes

## Build Error Fix - Platform::GetConfigDir()

**Date:** November 22, 2025  
**Error:** `no member named 'GetConfigDir' in namespace 'Platform'`

### Problem
The welcome screen implementation referenced `Platform::GetConfigDir()`, which doesn't exist in the Platform API.

### Root Cause
The Platform API uses `Platform::GetUserDataDir()` instead of `GetConfigDir()`. This function provides the same functionality:
- macOS: `~/Library/Application Support/Unveil Cartograph/`
- Windows: `%APPDATA%\Unveil\Cartograph\`
- Linux: `~/.local/share/unveil-cartograph/`

### Solution
Replaced all occurrences of `Platform::GetConfigDir()` with `Platform::GetUserDataDir()`.

### Files Changed
1. **src/UI.cpp** (line 885)
   - Changed: `std::string configDir = Platform::GetConfigDir();`
   - To: `std::string configDir = Platform::GetUserDataDir();`

2. **Documentation files updated:**
   - `WELCOME_SCREEN_IMPLEMENTATION.md` (2 occurrences)
   - `WELCOME_SCREEN_TODOS.md` (3 occurrences)
   - `WELCOME_FLOW_DIAGRAM.md` (1 occurrence)
   - `WELCOME_SCREEN_QUICK_REFERENCE.md` (2 occurrences)

### Verification
- ✅ No linter errors
- ✅ Correct Platform API used
- ✅ Documentation updated for consistency

### Related Platform Functions
```cpp
namespace Platform {
    std::string GetUserDataDir();    // User config/data storage
    std::string GetAutosaveDir();    // Autosave location
    std::string GetAssetsDir();      // Application assets
    bool EnsureDirectoryExists(const std::string& path);
}
```

**Status:** ✅ Fixed and verified


