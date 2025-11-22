# Welcome Screen - TODO Tracking

This document tracks all the TODOs for completing the welcome screen features.

---

## 🎯 High Priority

### 1. Implement Import Functionality

**Status:** 🔴 Not Started  
**Location:** `src/UI.cpp::RenderWelcomeScreen()` (line ~110)  
**Estimated Time:** 2-3 hours

**Current Code:**
```cpp
if (ImGui::Button("Import Project", ImVec2(400, 60))) {
    // TODO: Implement file picker dialog for .cart files
    ShowToast("Import feature coming soon!", Toast::Type::Info);
}
```

**Implementation Steps:**
1. Create native file picker using `Platform::Fs::OpenFileDialog()`
2. Filter for `.cart` and `.json` file extensions
3. Validate selected file exists and is readable
4. Call `app.OpenProject(selectedPath)`
5. Call `app.ShowEditor()` to transition
6. Add error handling for invalid files

**Dependencies:**
- May need to implement `Platform::Fs::OpenFileDialog()` if not already done
- Consider using a cross-platform library like NFD (Native File Dialog)

**Testing:**
- Test with valid .cart file
- Test with valid .json file
- Test with invalid file
- Test cancel action
- Test with non-existent file path

---

### 2. Recent Projects Persistence

**Status:** 🔴 Not Started  
**Location:** `src/UI.cpp::LoadRecentProjects()` and `AddRecentProject()`  
**Estimated Time:** 3-4 hours

#### 2a. LoadRecentProjects()

**Current Code:**
```cpp
void UI::LoadRecentProjects() {
    // TODO: Load recent projects from persistent storage
    recentProjects.clear();
    
    std::string configDir = Platform::GetUserDataDir();
    std::string recentPath = configDir + "recent_projects.json";
    
    // Stub: For now, just clear the list
}
```

**Implementation Steps:**
1. Check if config file exists at `Platform::GetUserDataDir() + "recent_projects.json"`
2. Read and parse JSON file using `nlohmann::json`
3. Validate each project entry:
   - Check if file path still exists
   - Verify file is readable
   - Load last modified timestamp
4. Sort by most recent first
5. Limit to 10 most recent
6. Populate `recentProjects` vector
7. Optional: Load thumbnail paths

**JSON Schema:**
```json
{
  "version": 1,
  "projects": [
    {
      "path": "/absolute/path/to/project.cart",
      "name": "Project Name",
      "lastModified": "2025-11-22T10:30:00Z",
      "lastOpened": "2025-11-22T14:00:00Z",
      "thumbnail": "/path/to/.cartograph/thumbnails/abc123.png"
    }
  ]
}
```

#### 2b. AddRecentProject()

**Current Code:**
```cpp
void UI::AddRecentProject(const std::string& path) {
    // TODO: Add project to recent list and save to persistent storage
    (void)path; // Suppress unused warning
}
```

**Implementation Steps:**
1. Create `RecentProject` struct entry
2. Extract project name from file path or metadata
3. Get current timestamp for `lastOpened`
4. Check if project already exists in list (remove duplicate)
5. Add to front of `recentProjects` vector
6. Limit list to 10 entries (remove oldest)
7. Save entire list to JSON file
8. Ensure config directory exists
9. Optional: Generate thumbnail (see TODO #3)

**Integration Points:**
- Call `AddRecentProject()` in `App::OpenProject()` after successful load
- Call `AddRecentProject()` in `App::SaveProject()` after successful save
- Call `LoadRecentProjects()` in `UI::UI()` constructor or `App::Init()`

---

### 3. Thumbnail Generation

**Status:** 🔴 Not Started  
**Location:** `src/UI.cpp::RenderRecentProjectsList()` (line ~800)  
**Estimated Time:** 4-6 hours

**Current Code:**
```cpp
// TODO: Add thumbnail preview here
ImGui::Text("[Thumbnail]");
ImGui::SameLine();
```

**Implementation Steps:**

#### Phase 1: Generation
1. Create `GenerateThumbnail(const std::string& projectPath)` method
2. Load project into temporary Model
3. Use `ExportPng::Export()` with small scale (256×256)
4. Save to `Platform::GetUserDataDir() + "thumbnails/"`
5. Use hash of project path as filename (e.g., `md5(path).png`)
6. Store thumbnail path in `RecentProject` struct

#### Phase 2: Loading
1. Load thumbnail as ImGui texture
2. Create texture cache to avoid reloading
3. Use `stb_image.h` to load PNG
4. Upload to GPU using `glTexImage2D()`
5. Store texture ID in `RecentProject` or separate cache

#### Phase 3: Display
1. Replace `ImGui::Text("[Thumbnail]")` with `ImGui::Image()`
2. Set thumbnail size (e.g., 64×64 or 128×128)
3. Add hover effect to enlarge thumbnail
4. Handle missing thumbnails gracefully

**Helper Methods Needed:**
```cpp
// In UI.h
struct ThumbnailCache {
    std::unordered_map<std::string, ImTextureID> textures;
};

// In UI.cpp
void GenerateThumbnail(const std::string& projectPath);
ImTextureID LoadThumbnail(const std::string& thumbnailPath);
void ClearThumbnailCache();
```

**Performance Considerations:**
- Generate thumbnails asynchronously using `JobQueue`
- Cache loaded textures to avoid repeated GPU uploads
- Lazy-load thumbnails (only load when scrolled into view)
- Set max thumbnail cache size (e.g., 50 thumbnails)

---

## 🟡 Medium Priority

### 4. File Picker Integration

**Status:** 🔴 Not Started  
**Location:** `src/platform/Fs.cpp`  
**Estimated Time:** 2-3 hours

**Current State:**
`Platform::Fs::OpenFileDialog()` may be stubbed or not implemented.

**Implementation Options:**

#### Option A: Native File Dialogs (Recommended)
- **macOS:** Use `NSOpenPanel` (Objective-C++)
- **Windows:** Use `IFileOpenDialog` (COM)
- **Linux:** Use GTK file chooser

#### Option B: Use NFD Library
- Cross-platform C library
- Simple API
- Easy to integrate
- Already works with SDL

**Example Integration:**
```cpp
#include <nfd.h>

std::string Platform::Fs::OpenFileDialog(
    const std::string& title,
    const std::string& defaultPath,
    const std::vector<std::string>& filters
) {
    nfdchar_t* outPath = nullptr;
    nfdfilteritem_t filterItems[] = {
        {"Cartograph Files", "cart,json"}
    };
    
    nfdresult_t result = NFD_OpenDialog(
        &outPath, filterItems, 1, defaultPath.c_str()
    );
    
    if (result == NFD_OKAY) {
        std::string path(outPath);
        NFD_FreePath(outPath);
        return path;
    }
    
    return "";
}
```

---

### 5. Drag & Drop Support

**Status:** 🔴 Not Started  
**Location:** `src/UI.cpp::RenderWelcomeScreen()`  
**Estimated Time:** 2-3 hours

**Implementation Steps:**
1. Enable SDL drop events in `App.cpp::ProcessEvents()`
2. Listen for `SDL_EVENT_DROP_FILE`
3. Validate dropped file is `.cart` or `.json`
4. Call `app.OpenProject(droppedPath)`
5. Transition to editor
6. Show visual feedback during drag (highlight drop zone)

**SDL3 Code:**
```cpp
// In App.cpp::ProcessEvents()
case SDL_EVENT_DROP_FILE:
    if (m_appState == AppState::Welcome) {
        std::string path = event.drop.file;
        if (IsValidCartographFile(path)) {
            OpenProject(path);
            ShowEditor();
        } else {
            m_ui.ShowToast("Invalid file type", Toast::Type::Error);
        }
        SDL_free(event.drop.file);
    }
    break;
```

**Visual Feedback:**
- Highlight welcome screen when file is being dragged over
- Show accepted file types
- Animate drop zone

---

### 6. Custom Template Saving

**Status:** 🔴 Not Started  
**Location:** New feature  
**Estimated Time:** 3-4 hours

**Feature Description:**
Allow users to save their current project configuration as a reusable template.

**Implementation Steps:**
1. Add "Save as Template" button in New Project Modal
2. Create template storage directory: `Platform::GetUserDataDir() + "templates/"`
3. Save template as JSON:
   ```json
   {
     "name": "My Custom Template",
     "cellSize": 16,
     "mapWidth": 320,
     "mapHeight": 240,
     "description": "For sidescroller games"
   }
   ```
4. Load custom templates in `RenderNewProjectModal()`
5. Display custom templates alongside built-in ones
6. Allow deletion of custom templates

**UI Changes:**
- Add "Custom Templates" section in modal
- List custom templates with delete button
- Add template description tooltip

---

## 🟢 Low Priority / Enhancements

### 7. Template Browser

**Status:** 🔴 Not Started  
**Estimated Time:** 4-5 hours

**Feature Description:**
Separate window showing all templates with visual previews and descriptions.

**Implementation:**
- Extract template selection from modal
- Create dedicated window
- Show template previews (grid visualization)
- Add template descriptions
- Allow template favoriting

---

### 8. What's New Auto-Show

**Status:** 🔴 Not Started  
**Estimated Time:** 1 hour

**Feature Description:**
Automatically show "What's New" panel on first launch after update.

**Implementation:**
1. Store last seen version in config
2. Compare with current version
3. Auto-show panel if version changed
4. Add "Don't show again" checkbox

---

### 9. Recent Projects Search/Filter

**Status:** 🔴 Not Started  
**Estimated Time:** 2 hours

**Feature Description:**
Search bar to filter recent projects by name.

**Implementation:**
- Add text input above recent projects list
- Filter `recentProjects` vector by search term
- Highlight matching text
- Show "No results" when empty

---

### 10. Project Statistics

**Status:** 🔴 Not Started  
**Estimated Time:** 2-3 hours

**Feature Description:**
Show basic stats for recent projects (room count, tile count, etc.).

**Implementation:**
- Add stats to `RecentProject` struct
- Calculate during `LoadRecentProjects()`
- Display below project name
- Keep stats cache to avoid repeated parsing

---

## 📊 Progress Tracking

| TODO | Priority | Status | Estimated Time | Assigned To |
|------|----------|--------|---------------|-------------|
| 1. Import Functionality | 🔴 High | Not Started | 2-3h | - |
| 2a. Load Recent Projects | 🔴 High | Not Started | 2-3h | - |
| 2b. Add Recent Projects | 🔴 High | Not Started | 1h | - |
| 3. Thumbnails | 🔴 High | Not Started | 4-6h | - |
| 4. File Picker | 🟡 Medium | Not Started | 2-3h | - |
| 5. Drag & Drop | 🟡 Medium | Not Started | 2-3h | - |
| 6. Custom Templates | 🟡 Medium | Not Started | 3-4h | - |
| 7. Template Browser | 🟢 Low | Not Started | 4-5h | - |
| 8. What's New Auto | 🟢 Low | Not Started | 1h | - |
| 9. Recent Search | 🟢 Low | Not Started | 2h | - |
| 10. Project Stats | 🟢 Low | Not Started | 2-3h | - |

**Total Estimated Time:** 26-38 hours

---

## 🚀 Suggested Implementation Order

1. **File Picker** (needed for import)
2. **Import Functionality** (core feature)
3. **Load Recent Projects** (persistence foundation)
4. **Add Recent Projects** (complete persistence)
5. **Thumbnails** (visual enhancement)
6. **Drag & Drop** (UX improvement)
7. **Custom Templates** (power user feature)
8. Remaining features as time permits

---

## 📝 Notes

### Testing Strategy

For each TODO:
1. Manual testing with various inputs
2. Edge case testing (empty, invalid, missing files)
3. Performance testing (large recent lists, many thumbnails)
4. Cross-platform testing (macOS, Windows, Linux)

### Code Quality

- Follow existing code style (clang-format)
- Add comments for complex logic
- Keep functions under 100 lines
- Write helper functions for repeated code
- Handle errors gracefully

### User Experience

- Provide feedback for all actions (toasts)
- Handle errors without crashing
- Show progress for long operations
- Make features discoverable
- Add tooltips where helpful

---

## 🔗 Related Files

- `src/App.h` / `src/App.cpp` - Application state
- `src/UI.h` / `src/UI.cpp` - UI implementation
- `src/Model.h` / `src/Model.cpp` - Data structures
- `src/platform/Fs.h` / `src/platform/Fs.cpp` - File operations
- `src/platform/Paths.h` / `src/platform/Paths.cpp` - Path utilities
- `src/IOJson.h` / `src/IOJson.cpp` - JSON serialization
- `src/ExportPng.h` / `src/ExportPng.cpp` - PNG export (for thumbnails)

---

**Last Updated:** November 22, 2025  
**Status:** All core infrastructure complete, TODOs ready for implementation

