# Build Instructions for Cartograph

## Prerequisites

Before building, you must populate the `third_party/` directory with third-party dependencies.

### Quick Setup (Recommended)

Run this script to download all required dependencies:

```bash
#!/bin/bash
# setup_deps.sh

cd third_party

# ImGui (docking branch)
if [ ! -d "imgui" ]; then
    echo "Cloning Dear ImGui (docking branch)..."
    git clone --branch docking https://github.com/ocornut/imgui.git imgui
fi

# nlohmann/json
if [ ! -f "nlohmann/json.hpp" ]; then
    echo "Downloading nlohmann/json..."
    mkdir -p nlohmann
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
        -o nlohmann/json.hpp
fi

# stb libraries
if [ ! -f "stb/stb_image.h" ]; then
    echo "Downloading stb libraries..."
    mkdir -p stb
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h \
        -o stb/stb_image.h
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h \
        -o stb/stb_image_write.h
fi

# minizip
if [ ! -d "minizip" ]; then
    echo "Cloning minizip..."
    git clone https://github.com/zlib-contrib/minizip.git minizip-src
    mkdir -p minizip
    cp minizip-src/*.c minizip-src/*.h minizip/ 2>/dev/null || true
fi

# NanoSVG (optional)
if [ ! -f "nanosvg/nanosvg.h" ]; then
    echo "Downloading NanoSVG (optional)..."
    mkdir -p nanosvg
    curl -L https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h \
        -o nanosvg/nanosvg.h
    curl -L https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h \
        -o nanosvg/nanosvgrast.h
fi

echo "Dependencies setup complete!"
```

Save this as `setup_deps.sh` and run:

```bash
chmod +x setup_deps.sh
./setup_deps.sh
```

## macOS Build

```bash
# Install SDL3 via Homebrew
brew install sdl3

# Setup dependencies (if not done already)
./setup_deps.sh

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Run
open Cartograph.app
# or
./Cartograph.app/Contents/MacOS/Cartograph
```

## Windows Build

### Using Visual Studio 2019+

1. Install SDL3:
   - Download SDL3 from https://github.com/libsdl-org/SDL/releases
   - Extract to `C:\SDL3`
   - Add `C:\SDL3\lib\x64` to your PATH

2. Setup dependencies:
   ```powershell
   # Run in PowerShell (or adapt setup_deps.sh for PowerShell)
   cd extern
   # Follow the manual setup steps below
   ```

3. Build:
   ```bash
   mkdir build && cd build
   cmake -G "Visual Studio 16 2019" -A x64 -DSDL3_DIR=C:/SDL3 ..
   cmake --build . --config Release
   ```

4. Run:
   ```bash
   .\Release\Cartograph.exe
   ```

### Manual Dependency Setup

If the script doesn't work, manually download:

1. **ImGui**: Clone https://github.com/ocornut/imgui (docking branch) into `third_party/imgui`
2. **nlohmann/json**: Download `json.hpp` into `third_party/nlohmann/`
3. **stb**: Download `stb_image.h` and `stb_image_write.h` into `third_party/stb/`
4. **minizip**: Clone https://github.com/zlib-contrib/minizip and copy source files to `third_party/minizip/`

## Common Issues

### SDL3 not found
- **macOS**: `brew install sdl3`
- **Windows**: Download from https://github.com/libsdl-org/SDL/releases and set `SDL3_DIR`

### OpenGL headers missing
- **macOS**: Included with Xcode Command Line Tools
- **Linux**: `sudo apt install libgl1-mesa-dev`
- **Windows**: Included with Visual Studio

### zlib not found (minizip dependency)
- **macOS**: `brew install zlib`
- **Linux**: `sudo apt install zlib1g-dev`
- **Windows**: Usually included, or install via vcpkg

### ImGui compilation errors
Make sure you're using the **docking branch**:
```bash
cd third_party/imgui
git checkout docking
```

## Development Build

For faster iteration during development:

```bash
mkdir build-dev && cd build-dev
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## Clean Build

```bash
rm -rf build
mkdir build && cd build
cmake ..
make clean && make
```

## Cross-Platform Notes

- **Line Endings**: Use LF (Unix) for all source files
- **Paths**: Use forward slashes `/` in CMake and C++ code
- **Compiler**: Requires C++17 support (Clang 5+, GCC 7+, MSVC 2019+)

