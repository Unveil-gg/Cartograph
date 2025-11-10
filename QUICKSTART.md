# Cartograph - Quick Start Guide

Get up and running with Cartograph in 5 minutes.

---

## Prerequisites

### macOS
```bash
# Install Xcode Command Line Tools (if not already installed)
xcode-select --install

# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install SDL3
brew install sdl3
```

### Windows
- Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ workload
- Install [CMake](https://cmake.org/download/)
- Download [SDL3](https://github.com/libsdl-org/SDL/releases)

---

## Build Steps (macOS)

```bash
# 1. Navigate to project directory
cd /path/to/Cartograph

# 2. Setup third-party dependencies (one-time)
./setup_deps.sh

# 3. Create build directory
mkdir build && cd build

# 4. Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# 5. Build
make

# 6. Run
open Cartograph.app
```

---

## Build Steps (Windows)

```powershell
# 1. Navigate to project directory
cd C:\path\to\Cartograph

# 2. Setup dependencies (manual)
# - Run setup_deps.sh in Git Bash or WSL
# - Or manually download dependencies (see BUILD_INSTRUCTIONS.md)

# 3. Create build directory
mkdir build
cd build

# 4. Configure with CMake
cmake -G "Visual Studio 16 2019" -A x64 -DSDL3_DIR=C:/path/to/SDL3 ..

# 5. Build
cmake --build . --config Release

# 6. Run
.\Release\Cartograph.exe
```

---

## Expected First Run

On first successful build and run, you should see:

1. **Window**: 1280×720 window titled "Cartograph"
2. **Menu Bar**: File, Edit, View menus at top
3. **Panels**: 
   - Left: Palette (tile types)
   - Left: Tools (paint, erase, etc.)
   - Center: Canvas (empty grid)
   - Right: Properties (project metadata)
   - Bottom: Status bar (zoom level)

---

## Troubleshooting

### "SDL3 not found"
```bash
# macOS
brew install sdl3

# Windows
# Download SDL3 and set SDL3_DIR in cmake command
```

### "imgui/imgui.h not found"
```bash
# Run the dependency setup script
./setup_deps.sh

# Or manually clone ImGui into third_party/imgui
cd third_party
git clone --branch docking https://github.com/ocornut/imgui.git imgui
```

### "Undefined symbols for architecture x86_64"
- Check that all source files are listed in `CMakeLists.txt`
- Verify OpenGL framework is linked (macOS: `-framework OpenGL`)

### Black canvas / nothing renders
- This is expected initially (canvas input handling not yet implemented)
- Grid should still be visible if you zoom out

### App crashes on launch
- Check console output for error messages
- Verify all dependencies are properly installed
- Try a debug build: `cmake -DCMAKE_BUILD_TYPE=Debug ..`

---

## Next Steps

Once the app launches successfully:

1. **Test Pan/Zoom**: Middle-mouse drag to pan, scroll wheel to zoom
2. **Check Menus**: All menu items should be visible (some not functional yet)
3. **Inspect Panels**: Palette should show default tile types
4. **Create Test Room**: (Not yet implemented - see INTEGRATION_NOTES.md)
5. **Save Project**: File → Save As (not yet fully functional)

---

## Current Limitations (MVP)

The following features are **scaffolded but not fully integrated**:

- ❌ Canvas painting (tool logic exists but input handling not connected)
- ❌ Room creation (UI not implemented)
- ❌ Door placement (UI not implemented)
- ❌ Marker placement (UI not implemented)
- ❌ Open/Save dialogs (native dialogs stubbed, ImGui fallback needed)
- ⚠️ PNG export (code exists but not wired to Export button)

See `INTEGRATION_NOTES.md` for implementation details.

---

## Development Workflow

```bash
# Make changes to source files
# ...

# Rebuild (from build directory)
make

# Run
open Cartograph.app

# Debug with Xcode (optional)
cmake -G Xcode ..
open Cartograph.xcodeproj
```

---

## Getting Help

- **Build Issues**: See `BUILD_INSTRUCTIONS.md`
- **Architecture Questions**: See `PROJECT_STATUS.md`
- **Integration Work**: See `INTEGRATION_NOTES.md`
- **Code Style**: See `.clang-format`

---

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on your platform
5. Submit a pull request

Please follow the existing code style and document all public APIs.

---

**Happy Mapping! 🗺️**

