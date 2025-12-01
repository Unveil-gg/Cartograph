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
  - Grid (optional overlay)
  - Rooms (solid outlines, fills, labels)
  - Tiles (from palette, painted within rooms)
  - Doors (dotted lines on room walls where connections exist)
  - Markers (icon-based points: save, boss, treasure, etc.)
- Visibility culling

**Visual Design Philosophy**:
- Rooms: Solid rectangular borders
- Doors: Dotted line segments on walls (not separate marker icons)
- Markers: Icon + label for points of interest
- Grid: Tile-level precision for editing

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

### Ownership Philosophy
Cartograph follows modern C++17 RAII principles with zero manual memory management.
All resources are automatically managed through smart pointers and RAII wrappers.

### Ownership Types

**1. Exclusive Ownership (`std::unique_ptr<T>`)**
- SDL resources: Window, GLContext, Cursor
- OpenGL resources: FboHandle (framebuffer objects)
- Command objects: All ICommand implementations in History
- Renderer: GlRenderer owned by App
- **Rule**: Use when exactly one owner exists

**2. Non-Owning References (raw pointers or references)**
- Function parameters borrowing data
- Pointers into containers (Model::FindMarker, etc.)
- ImGui API pointers (ImDrawList*, texture IDs)
- SDL callback userdata (transferred ownership via unique_ptr)
- **Rule**: Use for borrowed/temporary access, never own resources

**3. Value Semantics (stack objects)**
- Model, Canvas, UI, History, IconManager, JobQueue (owned by App)
- Vectors, strings, standard containers
- **Rule**: Prefer value semantics when possible

### Never Use
❌ `std::shared_ptr` - No shared ownership in this codebase  
❌ Manual `new`/`delete` - Use `std::make_unique` or RAII classes  
❌ Naked `malloc`/`free` - Use `std::vector` for buffers  
❌ `std::auto_ptr` - Deprecated, use `unique_ptr`

### Ownership Conventions

**Function Parameters:**
```cpp
// Borrowed, non-owning (preferred for parameters)
void Process(Model& model);           // Non-null reference
void Render(IconManager* icons);     // Optional pointer (can be nullptr)

// Transferring ownership (rare, use sparingly)
void AddCommand(std::unique_ptr<ICommand> cmd);
```

**Return Values:**
```cpp
// Returning ownership
std::unique_ptr<FboHandle> CreateFramebuffer(int w, int h);

// Returning borrowed reference (valid until container modification)
Marker* FindMarker(const std::string& id);  // May return nullptr
```

**Lifetime Rules:**
- ImGui pointers (`ImDrawList*`) valid only within frame
- Model query pointers (`Marker*`, `Room*`) valid until container mutation
- SDL callbacks receive transferred ownership via `unique_ptr::release()`
- OpenGL resources cleaned up by FboHandle destructor

### Resource Management Examples

**SDL Resources (Custom Deleters):**
```cpp
struct SDL_WindowDeleter {
    void operator()(SDL_Window* w) const { SDL_DestroyWindow(w); }
};
std::unique_ptr<SDL_Window, SDL_WindowDeleter> m_window;
```

**OpenGL Resources (RAII Class):**
```cpp
class FboHandle {
    ~FboHandle() { /* cleanup GL resources */ }
    // Move-only, non-copyable
};
std::unique_ptr<FboHandle> fbo = renderer.CreateFramebuffer(w, h);
```

**SDL Callbacks (Ownership Transfer):**
```cpp
auto data = std::make_unique<CallbackData>();
SDL_ShowDialog([](void* userdata, ...) {
    std::unique_ptr<CallbackData> data(  // Take ownership
        static_cast<CallbackData*>(userdata)
    );
    // Automatic cleanup at end of scope
}, data.release());  // Transfer ownership
```

### Allocation Strategy
- Stack allocation for small objects
- Heap allocation for large buffers (managed by std::vector)
- All heap allocations wrapped in smart pointers or RAII classes
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

