# Third-Party Dependencies

This directory contains vendored third-party libraries.

## Required Setup

Before building, you need to populate this directory with the following libraries:

### Dear ImGui (docking branch)
```bash
git clone --branch docking https://github.com/ocornut/imgui.git third_party/imgui
```

Required files:
- `imgui/*.cpp`, `imgui/*.h`
- `imgui/backends/imgui_impl_sdl3.*`
- `imgui/backends/imgui_impl_opengl3.*`

### nlohmann/json
```bash
# Header-only library
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -P third_party/nlohmann/
```

### stb libraries
```bash
# Header-only libraries
wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -P third_party/stb/
wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -P third_party/stb/
```

### minizip-ng
```bash
git clone https://github.com/zlib-ng/minizip-ng.git third_party/minizip-ng
```

Modern, actively maintained fork of minizip with enhanced features and bug fixes.

### NanoSVG (optional)
```bash
wget https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h -P third_party/nanosvg/
wget https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h -P third_party/nanosvg/
```

## Alternative: Git Submodules

You can also set these up as git submodules:

```bash
git submodule add --branch docking https://github.com/ocornut/imgui.git third_party/imgui
git submodule add https://github.com/nlohmann/json.git third_party/nlohmann-json
git submodule add https://github.com/nothings/stb.git third_party/stb-src
git submodule add https://github.com/zlib-ng/minizip-ng.git third_party/minizip-ng
```

Then copy the needed files to the expected locations.

