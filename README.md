# Unveil Cartograph

A lightweight cross-platform tool for designing Metroidvania-style maps.

![License](https://img.shields.io/badge/license-MIT-blue.svg)

## Features

Design interconnected game worlds with an intuitive canvas:

**Canvas & Editing**
- Infinite pan/zoom canvas with grid snapping
- Multiple tools: Paint, Erase, Fill, Eyedropper, Marker, Zoom
- Selection tool with copy/paste, cut, move, and nudge
- Walls and doors on cell edges
- Full undo/redo history

**Organization**
- Rooms with custom colors and metadata
- Markers for items, saves, bosses with custom icons
- Region groups for organizing areas

**Export & Themes**
- PNG export with configurable layers and scale
- Dark and Print-Light themes
- Autosave with crash recovery

**Cross-Platform**
- Native apps for macOS and Windows
- `.cart` files (ZIP containers with JSON project data)

## Quick Start

### macOS

```bash
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

See [BUILDING.md](BUILDING.md) for detailed build instructions, options, and distribution setup.

**Requirements:**
- macOS 10.15+ or Windows 10+
- CMake 3.16+
- C++17 compiler

## Keyboard Shortcuts

### Tools

| Shortcut | Action |
|----------|--------|
| V | Move tool (pan canvas) |
| S | Select tool |
| B | Paint brush |
| E | Erase tool |
| F | Fill tool |
| I | Eyedropper |
| M | Marker tool |
| Z | Zoom tool |

### Selection (when Select tool active)

| Shortcut | Action |
|----------|--------|
| Ctrl/Cmd+A | Select all |
| Ctrl/Cmd+C | Copy selection |
| Ctrl/Cmd+X | Cut selection |
| Ctrl/Cmd+V | Paste |
| Delete/Backspace | Delete selection |
| Arrow keys | Nudge selection |
| Escape | Deselect / Cancel paste |

### View

| Shortcut | Action |
|----------|--------|
| Space+Drag | Pan canvas |
| Mouse Wheel | Zoom in/out |
| G | Toggle grid |
| O | Toggle room overlays |
| = / - | Zoom in/out |

### File

| Shortcut | Action |
|----------|--------|
| Ctrl/Cmd+N | New project |
| Ctrl/Cmd+O | Open project |
| Ctrl/Cmd+S | Save |
| Ctrl/Cmd+Shift+S | Save As |
| Ctrl/Cmd+E | Export PNG |
| Ctrl/Cmd+Z | Undo |
| Ctrl/Cmd+Shift+Z (or Ctrl+Y) | Redo |

## File Format

Projects are saved as `.cart` files — ZIP archives containing:
- `project.json` — Project data, rooms, tiles, markers, settings
- `icons/` — Custom marker icons (PNG)

## Contributing

We welcome contributions! Please read [CONTRIBUTORS.md](CONTRIBUTORS.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.

Third-party dependencies have their own licenses - see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Credits

Built with [SDL3](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui), and other open source libraries. Icons from [Lucide](https://lucide.dev) (ISC License).
