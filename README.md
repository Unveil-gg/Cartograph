# Unveil Cartograph

A lightweight cross-platform tool for designing Metroidvania-style maps.

![License](https://img.shields.io/badge/license-MIT-blue.svg)

## Features

Design interconnected game worlds with an intuitive canvas:
- Infinite pan/zoom canvas with rooms, tiles, and grid snapping
- Multiple brush tools: Paint, Erase, Fill, Rectangle
- Doors and connections between rooms
- Markers for items, saves, bosses with custom icons
- PNG export with configurable layers and scale
- Full undo/redo and autosave
- Customizable keyboard shortcuts and themes

## Quick Start

### macOS

```bash
brew install sdl3 cmake
git clone https://github.com/unveil/cartograph.git
cd cartograph
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make
./Cartograph.app/Contents/MacOS/Cartograph
```

### Windows

```bash
git clone https://github.com/unveil/cartograph.git
cd cartograph
git submodule update --init --recursive
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
.\Release\Cartograph.exe
```

**Requirements:**
- macOS 10.15+ or Windows 10+
- CMake 3.16+
- C++17 compiler (Xcode CLI Tools / Visual Studio 2019+)

## Usage

Launch the app and start designing. Key shortcuts (defaults below):
- **Cmd/Ctrl+N/O/S**: New/Open/Save
- **B/E/F/I**: Brush/Erase/Fill/Eyedropper tools
- **Space+Drag**: Pan canvas
- **Mouse Wheel**: Zoom

Files are saved as `.cart` (ZIP containers with JSON project data and optional icons/themes).

## Contributing

We welcome contributions! Please read [CONTRIBUTORS.md](CONTRIBUTORS.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.

Third-party dependencies have their own licenses - see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Credits

Built with [SDL3](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui), and other open source libraries. Icons from [Lucide](https://lucide.dev) (ISC License).

