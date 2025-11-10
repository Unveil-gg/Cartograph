# Integration Notes

This document describes the critical integration points that need to be completed to make Cartograph fully functional.

---

## 1. Canvas Input Handling

**Location**: `src/UI.cpp` → `RenderCanvasPanel()` and `src/Canvas.cpp`

**Current State**: Canvas panel captures hover/drag but doesn't process tool actions.

**Required Integration**:

```cpp
// In UI::RenderCanvasPanel()
if (ImGui::IsItemHovered()) {
    ImVec2 mousePos = ImGui::GetMousePos();
    float wx, wy;
    canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
    
    int tx, ty;
    canvas.ScreenToTile(mousePos.x, mousePos.y, model.grid.tileSize, &tx, &ty);
    
    // Tool handling
    switch (currentTool) {
        case Tool::Paint:
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                // Find room at tile position
                // Create PaintTilesCommand
                // Add to history
            }
            break;
        case Tool::Erase:
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                // Similar to paint, but set tileId = 0
            }
            break;
        // ... other tools
    }
}
```

**Dependencies**:
- History::AddCommand()
- Model::FindRoom() / SetTileAt()
- PaintTilesCommand construction

---

## 2. Icon Atlas GPU Upload

**Location**: `src/Icons.cpp` → `BuildAtlas()`

**Current State**: Builds CPU-side atlas but doesn't upload to GPU.

**Required Integration**:

```cpp
void IconManager::BuildAtlas() {
    // ... existing packing code ...
    
    // Upload to GPU (OpenGL)
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        m_atlasWidth, m_atlasHeight, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        atlasPixels.data()
    );
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    m_atlasTexture = (ImTextureID)(intptr_t)textureId;
    
    // ... cleanup code ...
}
```

**Dependencies**:
- OpenGL headers (already included in GlRenderer)
- Texture deletion in `Clear()` or destructor

---

## 3. Native File Dialogs

**Location**: `src/platform/Fs.cpp`

**Current State**: Stubs return `std::nullopt`.

### macOS Implementation (Objective-C++)

```cpp
// Fs.mm (rename to .mm for Objective-C++)
#include <Cocoa/Cocoa.h>

std::optional<std::string> ShowOpenFileDialog(...) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        
        // Set allowed types from filters
        // ...
        
        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = [[panel URLs] objectAtIndex:0];
            return std::string([[url path] UTF8String]);
        }
    }
    return std::nullopt;
}
```

### Windows Implementation

```cpp
#include <windows.h>
#include <commdlg.h>

std::optional<std::string> ShowOpenFileDialog(...) {
    OPENFILENAMEA ofn = {};
    char filename[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return std::nullopt;
}
```

**Alternative**: Use ImGui file browser (simpler, cross-platform):
- https://github.com/AirGuanZ/imgui-filebrowser

---

## 4. Export Dialog Integration

**Location**: `src/UI.cpp` → `RenderExportModal()`

**Current State**: Modal renders but "Export" button does nothing.

**Required Integration**:

```cpp
if (ImGui::Button("Export", ImVec2(120, 0))) {
    // Show native save dialog
    auto path = Platform::ShowSaveFileDialog(
        "Export PNG",
        "map.png",
        {{"PNG Image", "*.png"}}
    );
    
    if (path) {
        // Trigger export (can be async via Jobs)
        m_jobs.Enqueue(
            JobType::ExportPng,
            [&model, &canvas, &renderer, *path, exportOptions]() {
                ExportPng::Export(model, canvas, renderer, *path, exportOptions);
            },
            [this, *path](bool success, const std::string& error) {
                if (success) {
                    ShowToast("Exported: " + *path, Toast::Type::Success);
                } else {
                    ShowToast("Export failed: " + error, Toast::Type::Error);
                }
            }
        );
    }
    
    showExportModal = false;
    ImGui::CloseCurrentPopup();
}
```

**Note**: Export needs `model` and `canvas` snapshots to avoid data races in background thread.

---

## 5. Open/Save Menu Integration

**Location**: `src/UI.cpp` → `RenderMenuBar()`

**Current State**: Menu items exist but have `// TODO` comments.

**Required Integration**:

```cpp
if (ImGui::MenuItem("Open...", "Cmd+O")) {
    auto path = Platform::ShowOpenFileDialog(
        "Open Project",
        {{"Cart Files", "*.cart"}, {"JSON Files", "*.json"}}
    );
    if (path) {
        // Access App instance (need to pass App* to UI or use callback)
        app->OpenProject(*path);
    }
}

if (ImGui::MenuItem("Save", "Cmd+S")) {
    if (currentFilePath.empty()) {
        // Show Save As dialog
    } else {
        app->SaveProject();
    }
}
```

**Solution**: Pass `App*` to `UI::Render()` or use callback system.

---

## 6. Room Creation UI

**Location**: New code in `src/UI.cpp` and `src/Canvas.cpp`

**Current State**: Not implemented.

**Design Option A - Tool Mode**:
- Add `Tool::CreateRoom` enum
- Click-drag on canvas to define rectangle
- Prompt for room name/color in properties panel

**Design Option B - Menu-Based**:
- "Edit > New Room" menu item
- Opens modal with X/Y/W/H inputs
- Creates room at specified location

**Recommended**: Option A (more intuitive, direct manipulation)

**Implementation Sketch**:

```cpp
// In UI::RenderCanvasPanel()
case Tool::CreateRoom:
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Start room creation
        m_roomDragStart = {tx, ty};
        m_isCreatingRoom = true;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_isCreatingRoom) {
        // Finish room creation
        Room newRoom;
        newRoom.id = GenerateRoomId();
        newRoom.name = "Room " + std::to_string(model.rooms.size() + 1);
        newRoom.rect = CalculateRect(m_roomDragStart, {tx, ty});
        newRoom.color = Color(0.3f, 0.3f, 0.3f, 1.0f);
        
        model.rooms.push_back(newRoom);
        model.MarkDirty();
        m_isCreatingRoom = false;
    }
    break;
```

---

## 7. Keymap Input Checking

**Location**: `src/Keymap.cpp` → `IsActionTriggered()`

**Current State**: Always returns `false`.

**Required Integration**:

```cpp
bool KeymapManager::IsActionTriggered(const std::string& action) const {
    auto it = m_bindings.find(action);
    if (it == m_bindings.end()) return false;
    
    const std::string& binding = it->second;
    
    // Parse binding string (e.g., "Ctrl+S", "Mouse1")
    // Check ImGui input state
    ImGuiIO& io = ImGui::GetIO();
    
    // Simple implementation for common cases
    if (binding == "Mouse1") {
        return ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    }
    if (binding == "Mouse2") {
        return ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    }
    
    // For keyboard shortcuts, parse modifiers + key
    // TODO: Implement full parser
    
    return false;
}
```

**Better Approach**: Use ImGui's built-in shortcut system:
- `ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)`
- Store shortcuts as ImGuiKeyChord instead of strings

---

## 8. Tile Row-Run Encoding Improvement

**Location**: `src/Model.cpp` → `SetTileAt()`

**Current State**: Naive implementation (replaces entire row).

**Required Improvement**: Proper run-length encoding insertion/update logic.

```cpp
void Model::SetTileAt(const std::string& roomId, int x, int y, int tileId) {
    TileRow* row = FindTileRow(roomId, y);
    if (!row) {
        tiles.push_back({roomId, y, {}});
        row = &tiles.back();
    }
    
    // Find the run containing x
    auto it = std::find_if(row->runs.begin(), row->runs.end(),
        [x](const TileRun& r) {
            return x >= r.startX && x < r.startX + r.count;
        });
    
    if (it != row->runs.end()) {
        // Split or modify existing run
        if (it->tileId == tileId) {
            return;  // No change
        }
        
        // TODO: Implement run splitting logic
        // - Split run at x
        // - Insert new run of length 1
        // - Merge adjacent runs with same tileId
    } else {
        // Insert new run
        // TODO: Find insertion point, merge if adjacent run has same tileId
    }
    
    MarkDirty();
}
```

**Alternative**: Keep it simple for MVP, optimize later if performance is an issue.

---

## 9. Autosave Path Feedback

**Location**: `src/App.cpp` → `DoAutosave()`

**Current State**: Saves to user data dir but doesn't inform user of location.

**Improvement**: Add to toast message:

```cpp
m_ui.ShowToast(
    "Autosaved to " + path, 
    Toast::Type::Success, 
    2.0f
);
```

Or add to status bar.

---

## 10. Canvas Rendering Order

**Location**: `src/Canvas.cpp` → `Render()`

**Current State**: Renders to background draw list, may conflict with ImGui.

**Issue**: ImGui background draw list is cleared each frame.

**Solution**: Render to a dedicated ImGui window or use foreground draw list within canvas panel.

```cpp
// In UI::RenderCanvasPanel()
ImDrawList* drawList = ImGui::GetWindowDrawList();
// Pass drawList to Canvas::Render()
// Use drawList->AddRect(), AddLine(), etc. with absolute coordinates
```

---

## Priority Order

1. **Canvas Input Handling** (critical for any functionality)
2. **Icon Atlas Upload** (needed for marker icons)
3. **Open/Save Integration** (needed to test persistence)
4. **Export Integration** (MVP feature)
5. **Native File Dialogs** (better UX, can use workaround initially)
6. **Room Creation** (essential editing feature)
7. **Tile Encoding Improvement** (optimization)
8. **Keymap Checking** (convenience)
9. **Autosave Feedback** (polish)
10. **Canvas Rendering** (fix if issues arise)

---

**End of Integration Notes**

