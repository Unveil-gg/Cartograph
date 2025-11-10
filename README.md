# Unveil Cartograph

**Tiny cross-platform C++17 app for designing Metroidvania-style maps.**

## Features

- **Infinite Canvas**: Pan and zoom with grid snapping
- **Rooms**: Create, resize, and organize rectangular rooms
- **Tile Painting**: Multiple brush tools (Paint, Erase, Fill, Rectangle)
- **Doors & Links**: Connect rooms with typed transitions
- **Markers**: Place items, saves, bosses, etc. with custom icons
- **PNG Export**: Multi-scale export with layer control
- **Undo/Redo**: Full history with intelligent brush stroke coalescing
- **Autosave**: Automatic background saves with debouncing
- **Themes**: Dark and Print-Light presets
- **Custom Icons**: Load PNG icons from assets or project folders

## Requirements

### macOS
- macOS 10.15+ (Catalina or later)
- Xcode Command Line Tools
- Homebrew (for dependencies)

### Windows
- Windows 10+
- Visual Studio 2019+ with C++17 support
- CMake 3.16+

## Dependencies

- **SDL3**: Window and input handling
- **Dear ImGui** (docking branch): UI framework
- **OpenGL 3.3**: Rendering
- **nlohmann/json**: JSON serialization
- **stb_image** / **stb_image_write**: Image I/O
- **minizip**: ZIP archive support for .cart files

## Building

### macOS

```bash
# Install dependencies via Homebrew
brew install sdl3 cmake

# Clone repository (including submodules for third-party libs)
git clone https://github.com/unveil/cartograph.git
cd cartograph

# Initialize submodules (ImGui, nlohmann/json, stb, minizip, etc.)
git submodule update --init --recursive

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Run
./Cartograph.app/Contents/MacOS/Cartograph
```

### Windows

```bash
# Clone repository
git clone https://github.com/unveil/cartograph.git
cd cartograph
git submodule update --init --recursive

# Build with CMake
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release

# Run
.\Release\Cartograph.exe
```

## Usage

### Keyboard Shortcuts (macOS)

- **Cmd+N**: New Project
- **Cmd+O**: Open Project
- **Cmd+S**: Save Project
- **Cmd+E**: Export PNG
- **Cmd+Z**: Undo
- **Cmd+Y**: Redo
- **G**: Toggle Grid
- **+/-**: Zoom In/Out
- **Space+Drag**: Pan Canvas
- **Mouse Wheel**: Zoom

### Tools

- **Paint (Mouse1)**: Draw tiles
- **Erase (Mouse2)**: Remove tiles
- **Fill (F)**: Flood fill
- **Rectangle (R)**: Draw filled rectangles
- **Door (D)**: Connect rooms with doors (rendered as dotted lines on walls)
- **Marker (M)**: Place markers/items with custom icons
- **Eyedropper (Alt+Mouse1)**: Pick tile type

### Visual Design

- **Rooms**: Solid rectangular outlines with fills
- **Doors**: Dotted lines on room walls where connections exist
- **Markers**: Icon-based points of interest (save points, bosses, treasure, etc.)
- **Grid**: Optional tile grid overlay for precise editing

### File Format

**`.cart` files** are ZIP containers with:
- `/manifest.json`: Metadata (title, author, version)
- `/project.json`: Complete map data (rooms, tiles, doors, markers)
- `/thumb.png`: Optional preview thumbnail
- `/icons/`: Optional custom PNG icons
- `/themes/`: Optional custom themes

You can also open/save raw `project.json` for debugging.

## Project Structure

```
Cartograph/
├── CMakeLists.txt
├── LICENSE (MIT)
├── README.md
├── THIRD_PARTY_NOTICES.md
├── third_party/             # Third-party dependencies
│   ├── imgui/               # Dear ImGui (docking branch)
│   ├── nlohmann/            # nlohmann::json
│   ├── stb/                 # stb_image, stb_image_write
│   ├── minizip/             # ZIP support
│   └── nanosvg/             # SVG support (optional)
├── src/
│   ├── main.cpp
│   ├── App.{h,cpp}          # Application lifecycle
│   ├── UI.{h,cpp}           # ImGui panels
│   ├── Canvas.{h,cpp}       # View transform, rendering
│   ├── Model.{h,cpp}        # Data structures
│   ├── History.{h,cpp}      # Undo/redo
│   ├── IOJson.{h,cpp}       # JSON serialization
│   ├── Package.{h,cpp}      # .cart ZIP handling
│   ├── ExportPng.{h,cpp}    # PNG export
│   ├── Icons.{h,cpp}        # Icon loading, atlas
│   ├── Keymap.{h,cpp}       # Key remapping
│   ├── Jobs.{h,cpp}         # Background tasks
│   ├── render/
│   │   ├── Renderer.h       # Abstract renderer
│   │   └── GlRenderer.{h,cpp}  # OpenGL 3.3 impl
│   └── platform/
│       ├── Fs.{h,cpp}       # File I/O
│       ├── Paths.{h,cpp}    # Platform paths
│       └── Time.{h,cpp}     # Timing utilities
├── assets/
│   └── icons/               # Default PNG icons (Kenney CC0)
└── samples/
    └── sample.cart          # Example map
```

## License

MIT License. See [LICENSE](LICENSE) for details.

Third-party libraries have their own licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing

Contributions welcome! Please:
1. Follow the existing code style (see `.clang-format`)
2. Keep lines under 80 characters (hard limit: 100)
3. Document all public APIs
4. Test on both macOS and Windows

## Roadmap

### MVP (Current)
- [x] Core canvas with pan/zoom/grid
- [x] Room creation and management
- [x] Tile painting with multiple brushes
- [x] JSON and .cart package I/O
- [x] PNG export with options
- [x] Undo/redo with coalescing
- [x] Autosave
- [x] Icon system (PNG)

### Post-MVP
- [ ] Door placement UI and logic
- [ ] Marker placement and editing
- [ ] Ability-based reachability overlay
- [ ] SVG icon support
- [ ] Minimap panel
- [ ] Legend generation on export
- [ ] Native file dialogs (macOS/Windows)
- [ ] Web build via Emscripten
- [ ] Custom themes editor
- [ ] Collaborative editing (future)

## Credits

- **Unveil Team**: Core development
- **Kenney**: Default icon pack (CC0)
- **Omar Cornut**: Dear ImGui
- **Sam Lantinga**: SDL
- **Niels Lohmann**: nlohmann/json
- **Sean Barrett**: stb libraries

## Support

For issues, questions, or feature requests, please open an issue on GitHub.

