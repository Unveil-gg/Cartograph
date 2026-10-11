# Agent guide

Cartograph is a C++17 map editor. SDL3 main callbacks in `src/main.cpp`
call `App`. Do not edit `third_party/`.

## Where a change goes

- Domain data and queries: `Model` (`src/Model.h`, `src/Model.cpp`).
- `project.json`: `IOJson`. Both `Package` (`.cart` ZIP) and `ProjectFolder`
  (`.cartproj` folder) must keep calling it. Do not add a third format.
- Undo: a new `ICommand` under `src/history/`. Paint and erase change the
  model during the drag, then push a command with `execute=false` on
  mouse-up. Update both sides or undo breaks.
- Canvas input: `CanvasPanel`. Chrome and panels: `UI` and `src/modals/`.
- OpenGL and `IconManager::BuildAtlas` / `AddIconFromMemory` run on the
  main thread. Workers may only decode bytes.

## File format

Saves write `project.json` version 2. Load accepts versions through 2,
including the v1 Y-flip migration. `ProjectFolder::IsProjectFolder` must
accept the same versions. `.cart` extension checks go through
`Package::HasCartExtension` (case-insensitive).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Keep new source files under 400 lines. Prefer another `.cpp` for the same
class over growing `UI.cpp`, `CanvasPanel.cpp`, or `Model.cpp`.
Directory names are `src/UI` and `src/Theme`; includes must match that case.
