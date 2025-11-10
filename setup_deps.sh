#!/bin/bash
# Setup script for Cartograph third-party dependencies

set -e

echo "Setting up Cartograph dependencies..."

cd third_party

# ImGui (docking branch)
if [ ! -d "imgui" ]; then
    echo "Cloning Dear ImGui (docking branch)..."
    git clone --branch docking https://github.com/ocornut/imgui.git imgui
else
    echo "ImGui already exists, skipping..."
fi

# nlohmann/json
if [ ! -f "nlohmann/json.hpp" ]; then
    echo "Downloading nlohmann/json..."
    mkdir -p nlohmann
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
        -o nlohmann/json.hpp
else
    echo "nlohmann/json already exists, skipping..."
fi

# stb libraries
if [ ! -f "stb/stb_image.h" ]; then
    echo "Downloading stb libraries..."
    mkdir -p stb
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h \
        -o stb/stb_image.h
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h \
        -o stb/stb_image_write.h
else
    echo "stb libraries already exist, skipping..."
fi

# minizip
if [ ! -d "minizip" ]; then
    echo "Setting up minizip..."
    if [ ! -d "minizip-src" ]; then
        git clone https://github.com/zlib-contrib/minizip.git minizip-src
    fi
    mkdir -p minizip
    cp minizip-src/*.c minizip-src/*.h minizip/ 2>/dev/null || true
else
    echo "minizip already exists, skipping..."
fi

# NanoSVG (optional)
if [ ! -f "nanosvg/nanosvg.h" ]; then
    echo "Downloading NanoSVG (optional)..."
    mkdir -p nanosvg
    curl -L https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h \
        -o nanosvg/nanosvg.h
    curl -L https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h \
        -o nanosvg/nanosvgrast.h
else
    echo "NanoSVG already exists, skipping..."
fi

cd ..

echo ""
echo "✓ Dependencies setup complete!"
echo ""
echo "Next steps:"
echo "1. Install SDL3: brew install sdl3"
echo "2. Build: mkdir build && cd build && cmake .. && make"
echo "3. Run: ./Cartograph.app/Contents/MacOS/Cartograph"

