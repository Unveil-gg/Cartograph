# Cartograph Architecture

## High-Level Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      User Interface (ImGui)                  │
│  ┌──────────┬──────────┬──────────┬───────────┬───────────┐ │
│  │  Menu    │ Palette  │  Tools   │Properties │  Status   │ │
│  │   Bar    │  Panel   │  Panel   │   Panel   │    Bar    │ │
│  └──────────┴──────────┴──────────┴───────────┴───────────┘ │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                   Canvas Panel                          │ │
│  │         (Pan/Zoom View, Grid, Rooms, Tiles)            │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   App    │  │    UI    │  │  Canvas  │  │ History  │   │
│  │ (Main    │  │ (Panels) │  │ (View)   │  │(Undo/Redo)│   │
│  │  Loop)   │  │          │  │          │  │          │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │             │             │          │
└───────┼─────────────┼─────────────┼─────────────┼──────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────┐
│                        Core Systems                          │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Model   │  │  Icons   │  │  Keymap  │  │  Jobs    │   │
│  │ (Data)   │  │ (Atlas)  │  │(Bindings)│  │ (Async)  │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │             │             │          │
└───────┼─────────────┼─────────────┼─────────────┼──────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────┐
│                      Subsystems                              │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ IOJson   │  │ Package  │  │ExportPng │  │ Renderer │   │
│  │ (Save/   │  │  (.cart  │  │ (Image   │  │(Abstract)│   │
│  │  Load)   │  │   ZIP)   │  │ Export)  │  │          │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │             │             │          │
└───────┼─────────────┼─────────────┼─────────────┼──────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────┐
│                    Platform Layer                            │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   Fs     │  │  Paths   │  │   Time   │  │GlRenderer│   │
│  │ (Files)  │  │  (Dirs)  │  │ (Clock)  │  │ (OpenGL) │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │             │             │          │
└───────┼─────────────┼─────────────┼─────────────┼──────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────┐
│                    Operating System                          │
│     SDL3 │ OpenGL │ FileSystem │ Threads │ Time            │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Breakdown

### 1. User Interface (UI.h/.cpp)
**Purpose**: ImGui-based panel system  
**Responsibilities**:
- Menu bar (File, Edit, View)
- Tool palette (tile selection)
- Properties inspector
- Canvas viewport
- Status bar
- Toast notifications
- Modal dialogs (export)

**Dependencies**: ImGui, Model, Canvas, History, Icons

---

### 2. Application (App.h/.cpp)
**Purpose**: Main application lifecycle  
**Responsibilities**:
- SDL window management
- OpenGL context creation
- Main event loop
- Frame timing
- System initialization
- File operations (New, Open, Save, Export)
- Autosave coordination

**Dependencies**: SDL3, ImGui, all core systems

---

### 3. Canvas (Canvas.h/.cpp)
**Purpose**: Viewport and rendering  
**Responsibilities**:
- Pan/zoom transformations
- Coordinate conversions (screen/world/tile)
- Rendering passes:
  - Grid
  - Rooms (outlines, fills, labels)
  - Tiles (from palette)
  - Doors (endpoints)
  - Markers (icons, labels)
- Visibility culling

**Dependencies**: Model, Renderer

---

### 4. Model (Model.h/.cpp)
**Purpose**: Core data structures  
**Responsibilities**:
- Grid configuration
- Palette (tile types)
- Rooms (rectangles with metadata)
- Tiles (row-run encoded)
- Doors (typed connections)
- Markers (points of interest)
- Keymap (input bindings)
- Theme (colors, UI scale)
- Metadata (title, author)
- Dirty tracking

**Dependencies**: None (pure data)

---

### 5. History (History.h/.cpp)
**Purpose**: Undo/redo system  
**Responsibilities**:
- Command pattern implementation
- Undo/redo stacks
- Command coalescing (brush strokes)
- Command execution
- Concrete commands:
  - PaintTilesCommand
  - ModifyRoomCommand (stub)

**Dependencies**: Model

---

### 6. Icons (Icons.h/.cpp)
**Purpose**: Icon loading and atlas  
**Responsibilities**:
- PNG loading (stb_image)
- SVG rasterization (NanoSVG, optional)
- Texture atlas packing
- Icon lookup by name
- Hot-reload (desktop, future)

**Dependencies**: stb_image, OpenGL (for texture upload)

---

### 7. IOJson (IOJson.h/.cpp)
**Purpose**: JSON serialization  
**Responsibilities**:
- Model → JSON (save)
- JSON → Model (load)
- Stable key ordering
- Schema version 1

**Dependencies**: nlohmann::json, Model, Platform::Fs

---

### 8. Package (Package.h/.cpp)
**Purpose**: .cart ZIP packaging  
**Responsibilities**:
- Manifest generation
- ZIP creation (save)
- ZIP extraction (load)
- Multi-file container:
  - /manifest.json
  - /project.json
  - /thumb.png (optional)
  - /icons/ (optional)
  - /themes/ (optional)

**Dependencies**: minizip, IOJson, Model

---

### 9. ExportPng (ExportPng.h/.cpp)
**Purpose**: PNG image export  
**Responsibilities**:
- Offscreen FBO rendering
- Multi-scale export (1×, 2×, 3×, etc.)
- Transparency control
- Layer visibility:
  - Grid
  - Room outlines/labels
  - Tiles
  - Doors
  - Markers
  - Legend (future)
- Crop modes (used area, full grid)
- Pixel readback
- PNG encoding (stb_image_write)

**Dependencies**: Renderer, Canvas, Model, stb_image_write

---

### 10. Jobs (Jobs.h/.cpp)
**Purpose**: Background task queue  
**Responsibilities**:
- Worker thread (desktop)
- Job queue (FIFO)
- Completion callbacks (main thread)
- Job types:
  - SaveProject
  - ExportPng

**Dependencies**: std::thread, std::mutex

---

### 11. Keymap (Keymap.h/.cpp)
**Purpose**: Input mapping  
**Responsibilities**:
- Action → binding storage
- Binding strings (e.g., "Ctrl+S")
- Platform normalization (Cmd/Ctrl)
- Input state checking (stub)

**Dependencies**: ImGui (for input queries)

---

### 12. Renderer (render/Renderer.h, GlRenderer.h/.cpp)
**Purpose**: 2D rendering abstraction  
**Responsibilities**:
- Abstract API (IRenderer)
- OpenGL 3.3 implementation (GlRenderer)
- Primitives:
  - Filled rectangles
  - Outlined rectangles
  - Lines
- Framebuffer management (offscreen rendering)
- Pixel readback
- HiDPI support

**Dependencies**: OpenGL, SDL3, ImGui (for draw lists)

---

### 13. Platform Layer (platform/)
**Purpose**: OS abstraction  
**Responsibilities**:

#### Fs.h/.cpp (File I/O)
- File read/write (binary, text)
- Native dialogs (open, save) - **stubbed**
- File existence checks

#### Paths.h/.cpp (Directories)
- User data directory
- Autosave directory
- Assets directory (bundle-aware)
- Directory creation

#### Time.h/.cpp (Timing)
- High-precision time (SDL_GetTicks)
- Millisecond timestamps

**Dependencies**: SDL3, C++17 std::filesystem

---

## Data Flow Examples

### Saving a Project
```
User → UI (Menu: Save) → App::SaveProject()
  → IOJson::SaveToString(model)
  → Package::Save(model, path)
    → minizip (write ZIP)
      → manifest.json
      → project.json
  → JobQueue::Enqueue (async)
  → Completion callback → Toast notification
```

### Painting a Tile
```
User clicks canvas → UI::RenderCanvasPanel()
  → Canvas::ScreenToTile(mousePos)
  → Model::FindRoom(tilePos)
  → PaintTilesCommand::new(tileChanges)
  → History::AddCommand(cmd, model)
    → cmd->Execute(model)
    → model.SetTileAt(room, x, y, tileId)
    → model.MarkDirty()
```

### Exporting PNG
```
User → UI (Export modal) → ExportPng::Export()
  → GlRenderer::CreateFramebuffer(w, h)
  → Canvas::Render (to FBO)
  → GlRenderer::ReadPixels()
  → stbi_write_png(path, pixels)
  → GlRenderer::DestroyFramebuffer()
  → Toast notification
```

### Undo/Redo
```
User → UI (Menu: Undo) → History::Undo(model)
  → cmd->Undo(model)
    → model.SetTileAt (restore old values)
  → model.MarkDirty()
  → Canvas re-renders
```

---

## Threading Model

### Main Thread
- SDL event loop
- ImGui rendering
- Canvas rendering
- Input handling
- Job callback processing

### Worker Thread (Jobs)
- Background saves
- Background exports
- No direct model access (uses snapshots)

**Synchronization**: `std::mutex` for job queues

---

## Memory Management

### Ownership
- **App** owns:
  - Renderer (unique_ptr)
  - Model (value)
  - Canvas (value)
  - UI (value)
  - History (value)
  - Systems (value)

- **Model** owns:
  - Vectors of data (rooms, tiles, doors, markers)

- **History** owns:
  - Command stacks (unique_ptr)

### Allocation Strategy
- Stack allocation for small objects
- Heap allocation for large buffers (FBO pixels)
- No manual `new`/`delete` (RAII via smart pointers)

---

## Error Handling

### Strategy
- Return `bool` for success/failure
- Use `std::optional` for optional values
- Log errors to console (stderr)
- Show user-facing errors via toasts

### Examples
```cpp
// File I/O
if (!Platform::WriteFile(path, data)) {
    // Log and show toast
}

// Optional return
auto content = Platform::ReadFile(path);
if (!content) {
    // Handle missing file
}
```

---

## Build System (CMake)

### Structure
```
CMakeLists.txt (root)
  └── third_party/CMakeLists.txt (third-party libs)
```

### Options
- `BUILD_WEB`: Enable Emscripten (future)
- `USE_STATIC_CRT`: Static C runtime (Windows)
- `USE_SVG_ICONS`: Enable NanoSVG (optional)

### Targets
- **Cartograph**: Main executable
- **imgui**: Static library
- **minizip**: Static library
- **nlohmann_json**: Interface library (header-only)
- **stb**: Interface library (header-only)

---

## Extension Points

### Adding a New Tool
1. Add enum to `UI::Tool`
2. Implement tool logic in `UI::RenderCanvasPanel()`
3. Create corresponding command in `History`
4. Update tool panel UI

### Adding a New File Format
1. Create new I/O module (e.g., `IOXml.h/.cpp`)
2. Implement `Load()` and `Save()` functions
3. Add file type to dialogs
4. Update `App` to call new I/O functions

### Adding a New Panel
1. Add panel function to `UI` (e.g., `RenderMinimapPanel()`)
2. Call from `UI::Render()`
3. Use ImGui docking for placement

---

## Performance Considerations

### Rendering
- Visibility culling for off-screen rooms
- Batch primitives via ImGui draw lists
- Offscreen rendering only when exporting

### Data Structures
- Row-run encoding for sparse tile data
- Room lookup: O(n) linear search (fine for <1000 rooms)
- Tile lookup: O(r × n) where r = runs per row

### Future Optimizations
- Spatial index for room lookup (quadtree)
- Improved run-length encoding (merging, splitting)
- GPU-side tile rendering (texture atlas)

---

## Platform-Specific Notes

### macOS
- App bundle structure (Resources/)
- Use of Cocoa for file dialogs (future)
- Metal instead of OpenGL (future)
- Code signing (placeholder)

### Windows
- Portable folder structure
- COM interfaces for file dialogs (future)
- Static CRT option
- MSVC 2019+ required

### Web (Future)
- Emscripten build target
- IDBFS for persistence
- GLES 2.0/3.0 compatibility
- Async file operations

---

**End of Architecture Document**

