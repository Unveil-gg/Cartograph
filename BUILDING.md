# Building Cartograph

Cartograph is a C++17 app. Dependencies are vendored under `third_party/`
(SDL3, Dear ImGui, GLAD, nlohmann/json, stb, minizip-ng, NanoSVG). There is
no `.gitmodules` file. If a dependency directory is empty, run
`scripts/setup_deps.sh` (macOS/Linux) or `scripts/setup_deps.ps1` (Windows).

## Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

`CMAKE_EXPORT_COMPILE_COMMANDS` is on by default. Point clangd at
`build/compile_commands.json`.

### Options

- `USE_STATIC_CRT` — static MSVC runtime (CI sets this on Windows).
- `USE_SVG_ICONS` — link NanoSVG and define `CARTOGRAPH_USE_SVG`.
- `CODESIGN_IDENTITY`, `DEVELOPMENT_TEAM` — macOS signing. See
  `scripts/codesign.sh` and `scripts/notarize.sh`.

The cache default for `CMAKE_OSX_DEPLOYMENT_TARGET` is 10.15. Release CI
passes 11.0.

On Windows, pick a Visual Studio generator that matches the installed toolset.
CI uses `cmake -G "Visual Studio 17 2022" -A x64`.

## Tests

```bash
cmake --build build --target cartograph_tests
ctest --test-dir build --output-on-failure
```

`cartograph_tests` covers JSON round-trip, the v1 coordinate migration,
folder version detection, `.cart` path checks, and the grid size cap.

## Run

macOS: `build/Cartograph.app/Contents/MacOS/Cartograph`

Windows: `build/Release/Cartograph.exe`
