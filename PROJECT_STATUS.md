# Cartograph - Project Status

**Version**: 1.0.0-alpha  
**Last Updated**: November 10, 2025  
**Status**: Foundation Complete, Needs Dependencies & Testing

---

## ✅ Completed Components

### Core Architecture
- [x] **CMake Build System** - Multi-platform support (macOS, Windows)
- [x] **Project Structure** - Organized source tree with clear separation of concerns
- [x] **Main Application Loop** - SDL3 window, OpenGL context, frame timing
- [x] **ImGui Integration** - Docking layout with fullscreen dockspace

### Data Model
- [x] **Model.h/.cpp** - Complete data structures:
  - Grid configuration (tile size, dimensions)
  - Palette (tile types with colors)
  - Rooms (rectangular regions with metadata)
  - Tiles (row-run encoded tile data)
  - Doors (typed connections between rooms)
  - Markers (points of interest with icons)
  - Keymap (customizable shortcuts)
  - Theme (Dark/Print-Light presets)
  - Metadata (title, author)

### Rendering
- [x] **Renderer.h** - Abstract 2D rendering interface
- [x] **GlRenderer.h/.cpp** - OpenGL 3.3 implementation
  - Framebuffer support for offscreen rendering
  - Pixel readback for export
  - HiDPI awareness

### Canvas & View
- [x] **Canvas.h/.cpp** - Viewport management:
  - Pan/zoom transformations
  - Coordinate conversions (screen ↔ world ↔ tile)
  - Rendering passes (grid, rooms, tiles, doors, markers)
  - Visibility culling

### User Interface
- [x] **UI.h/.cpp** - ImGui panel system:
  - Menu bar (File, Edit, View)
  - Palette panel (tile type selection)
  - Tools panel (tool switching)
  - Properties panel (project metadata)
  - Canvas panel (main viewport)
  - Status bar (zoom, dirty state)
  - Toast notifications (timed messages)
  - Export modal (PNG export options)

### History & Undo
- [x] **History.h/.cpp** - Command pattern:
  - Undo/redo stacks
  - Command coalescing (brush strokes within 150ms)
  - PaintTilesCommand implementation
  - ModifyRoomCommand stub

### File I/O
- [x] **IOJson.h/.cpp** - JSON serialization:
  - Save/load model to/from JSON
  - Stable ordering for version control
  - Schema version 1
- [x] **Package.h/.cpp** - .cart ZIP packaging:
  - Manifest generation
  - Multi-file container (project.json, icons, themes)
  - ZIP read/write via minizip

### Export
- [x] **ExportPng.h/.cpp** - PNG image export:
  - Multi-scale rendering (1×, 2×, 3×, custom)
  - Transparency support
  - Layer visibility control
  - Crop modes (used area, full grid)
  - Offscreen FBO rendering
  - stb_image_write integration

### Systems
- [x] **Icons.h/.cpp** - Icon management:
  - PNG loading via stb_image
  - Texture atlas building
  - Icon lookup by name
  - SVG support scaffolding (NanoSVG optional)
- [x] **Jobs.h/.cpp** - Background task queue:
  - Worker thread for async operations
  - Callback system for completion
  - Desktop: std::thread, Web: sync fallback
- [x] **Keymap.h/.cpp** - Input mapping:
  - Action → binding storage
  - Cmd/Ctrl platform normalization
  - Input state checking stub

### Platform Layer
- [x] **Fs.h/.cpp** - File operations:
  - File read/write (binary, text)
  - File existence checks
  - Native dialog stubs (TODO)
- [x] **Paths.h/.cpp** - Platform paths:
  - User data directory (~/Library/Application Support on macOS)
  - Autosave directory
  - Assets directory (app bundle aware)
  - Directory creation
- [x] **Time.h/.cpp** - Timing utilities:
  - High-precision time (SDL_GetTicks)
  - Timestamp milliseconds

### Documentation
- [x] **README.md** - Comprehensive project overview
- [x] **LICENSE** - MIT license
- [x] **THIRD_PARTY_NOTICES.md** - Dependency attributions
- [x] **BUILD_INSTRUCTIONS.md** - Detailed build guide
- [x] **setup_deps.sh** - Automated dependency setup script

---

## ⚠️ TODO: Implementation Gaps

### Critical (Blocks First Build)
1. **Third-Party Dependencies** - Need to populate `third_party/`:
   ```bash
   ./setup_deps.sh
   brew install sdl3 zlib
   ```

2. **Painting Tool Logic** - Canvas input handling:
   - Mouse event capture in canvas panel
   - Tool-specific drawing logic (paint, erase, fill, rect)
   - Integration with History system
   - Cursor feedback

### High Priority (Core Features)
3. **Native File Dialogs** - Platform-specific implementations:
   - macOS: Use NSOpenPanel/NSSavePanel via Objective-C bridge
   - Windows: Use IFileDialog COM interface
   - Fallback: ImGui file browser

4. **Icon Atlas GPU Upload** - OpenGL texture creation:
   - Generate GL texture from atlas pixels
   - Return ImTextureID for ImGui
   - Texture deletion on cleanup

5. **Room Editing UI** - Interactive room manipulation:
   - Create room tool
   - Resize handles
   - Move/delete operations
   - Selection system

6. **Door Placement UI** - Door creation and linking:
   - Two-click placement (endpoint A & B)
   - Visual connection lines
   - Door type selection
   - Gate/ability configuration

7. **Marker Placement UI** - Marker creation:
   - Click-to-place
   - Icon picker
   - Label/kind editing
   - Color customization

### Medium Priority (Polish)
8. **Minimap Panel** - Small overview:
   - Render-to-texture
   - Click-to-navigate
   - Visible region indicator

9. **Legend Generation** - Export feature:
   - Automatic tile type legend
   - Marker icon legend
   - Configurable position

10. **Theme Editor** - Custom themes:
    - Color pickers for all theme values
    - Save/load custom themes
    - Export theme to .json

11. **Hot-Reload for Icons** - Desktop only:
    - File system watching
    - Automatic atlas rebuild
    - No-restart icon updates

### Low Priority (Nice-to-Have)
12. **Reachability Overlay** - Ability-gated analysis:
    - Ability progression graph
    - Reachable regions visualization
    - Gate requirement display

13. **Web Build** - Emscripten target:
    - IDBFS for persistence
    - Async file operations
    - GLES compatibility

14. **Collaborative Editing** - Future:
    - Operational transform
    - WebSocket sync
    - Conflict resolution

---

## 🏗️ Build Status

### macOS
**Status**: Ready to build after dependencies  
**Requirements**:
- Xcode Command Line Tools
- Homebrew (for SDL3)
- Run `./setup_deps.sh`

**Commands**:
```bash
brew install sdl3
./setup_deps.sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
open Cartograph.app
```

### Windows
**Status**: Ready to build after dependencies  
**Requirements**:
- Visual Studio 2019+ with C++17
- CMake 3.16+
- SDL3 installed or in PATH

**Commands**:
```powershell
# Setup deps manually or adapt script
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
.\Release\Cartograph.exe
```

### Linux (Untested)
**Status**: Should work with minor adjustments  
**Requirements**:
- GCC 7+ or Clang 5+
- SDL3, OpenGL, zlib development packages

---

## 📊 Code Statistics

**Total Source Files**: 35+  
**Lines of Code**: ~5,000+ (estimated)  
**Languages**: C++ (primary), CMake, Shell

### File Breakdown
| Component | Files | Purpose |
|-----------|-------|---------|
| Core | App, UI, Canvas | Application lifecycle, UI panels, viewport |
| Model | Model, History | Data structures, undo/redo |
| I/O | IOJson, Package, ExportPng | Serialization, .cart files, PNG export |
| Rendering | Renderer, GlRenderer | Abstract API, OpenGL implementation |
| Systems | Icons, Jobs, Keymap | Icon loading, background tasks, input |
| Platform | Fs, Paths, Time | File I/O, paths, timing |
| Build | CMakeLists.txt | Multi-platform build configuration |

---

## 🎯 Next Steps

### Immediate (To First Working Build)
1. Run `./setup_deps.sh` to fetch third-party libraries
2. Install SDL3 via Homebrew
3. Build the project
4. Fix any compilation errors (expected: OpenGL headers, linking)
5. Test basic window launch and ImGui rendering

### Short-Term (MVP Completion)
6. Implement canvas mouse input handling
7. Wire up painting tools (paint/erase at minimum)
8. Test JSON save/load round-trip
9. Verify PNG export works
10. Create a sample .cart file

### Medium-Term (First Release)
11. Implement native file dialogs
12. Add room creation/editing UI
13. Implement door and marker placement
14. Test on Windows
15. Package as distributable (app bundle, installer)

---

## 📝 Known Issues & TODOs

### Code TODOs (Search for "TODO:" in source)
- [ ] `platform/Fs.cpp`: Native file dialog implementations
- [ ] `Icons.cpp`: GPU texture upload for atlas
- [ ] `Canvas.cpp`: Mouse input handling for tools
- [ ] `UI.cpp`: Export dialog triggers actual export
- [ ] `App.cpp`: Open/Save dialog integration
- [ ] `History.cpp`: ModifyRoomCommand implementation
- [ ] `ExportPng.cpp`: Layer visibility logic
- [ ] `Keymap.cpp`: Input state checking from ImGui

### Architectural Decisions Needed
- **Room Editing**: Direct manipulation vs. property panel?
- **Door UI**: Drag-and-drop vs. two-click placement?
- **Marker Icons**: Custom vs. predefined only?
- **Multi-select**: Single selection only or multi-select rooms/markers?

---

## 🧪 Testing Strategy

### Manual Testing Checklist
- [ ] Launch application (window appears, no crashes)
- [ ] ImGui docking works (panels can be moved/docked)
- [ ] Canvas renders (grid visible, can pan/zoom)
- [ ] Palette panel shows tile types
- [ ] Create a test room
- [ ] Paint some tiles
- [ ] Undo/redo works
- [ ] Save project as .cart
- [ ] Open saved project (data intact)
- [ ] Export PNG (file created, looks correct)
- [ ] Autosave triggers (check user data directory)

### Unit Testing (Future)
- Model serialization/deserialization
- Coordinate transformations (screen/world/tile)
- History command coalescing
- Row-run encoding/decoding
- Icon atlas packing

---

## 📚 Resources

### Dependencies
- [SDL3](https://github.com/libsdl-org/SDL)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [nlohmann/json](https://github.com/nlohmann/json)
- [stb libraries](https://github.com/nothings/stb)
- [minizip](https://github.com/zlib-contrib/minizip)

### References
- OpenGL 3.3 Core Profile Spec
- C++17 Standard
- ImGui Docking Branch Documentation
- Metroidvania Design Patterns

---

**End of Status Report**

