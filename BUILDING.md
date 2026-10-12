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

## Web (Emscripten)

Builds a WebGL2 / WebAssembly version for hosting (e.g. itch.io). Needs
[Emscripten](https://emscripten.org) with Python 3.10+ (`brew install
emscripten` or emsdk). The CLI and tests are not built on web.

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target Cartograph
python3 -m http.server -d build-web 8080   # open http://localhost:8080
```

The first configure runs `embuilder build zlib` (needed for `.cart`
export). Passing `-DBUILD_WEB=ON` without `emcmake` is an error.

Output is `index.html`, `index.js`, `index.wasm` and `index.data`. For
itch.io, zip those four files, upload as "HTML", and tick "This file will
be played in the browser".

How I/O works in the browser:

- Projects save to IndexedDB (mounted at `/persist`) and survive reloads.
- Open Project, Import Project, Import Icon and drag-and-drop use the
  browser file picker; uploaded `.cart` files are imported into storage.
- Export Package and Export PNG download through the browser.
- Folder (`.cartproj`) projects live in browser storage only; use
  Export Package to move a project off the browser.
- Jobs run on the main thread, so no COOP/COEP headers are required.
