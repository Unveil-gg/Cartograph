# ImGui Fixed Docking Layout Update

## Summary
Updated Cartograph to use a **fixed, non-user-rearrangeable docking layout** as specified in the project requirements.

## Changes Made

### 1. Updated Dependencies (`setup_deps.sh`)
- Changed ImGui from master branch (v1.91.5) to **docking branch**
- This provides access to the full docking API (`DockSpace`, `DockBuilder*`, etc.)

### 2. App.cpp
- Enabled docking in ImGui: `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable`
- Layout is locked but docking is enabled internally

### 3. UI.h
- Added `BuildFixedLayout(ImGuiID dockspaceId)` function
- Added `m_layoutInitialized` flag to track first-run setup

### 4. UI.cpp - Fixed Docking Layout Implementation

#### Main Render Loop
- Creates a fullscreen dockspace window with these flags:
  - `ImGuiWindowFlags_MenuBar` - Shows menu bar at top
  - `ImGuiWindowFlags_NoDocking` - Prevents this window from being docked
  - `ImGuiWindowFlags_NoTitleBar` - Removes title bar
  - Other flags prevent moving, resizing, collapsing

- Calls `DockSpace()` to create the docking area
- Calls `BuildFixedLayout()` on first frame only

#### BuildFixedLayout() Function
Creates a 4-panel layout:

```
┌─────────────────────────────────────────────────────┐
│                    Menu Bar                         │
├───────────┬──────────────────────┬─────────────────┤
│           │                      │                 │
│  Tools    │       Canvas         │   Inspector     │
│  (300px)  │      (center)        │    (360px)      │
│           │                      │                 │
├───────────┴──────────────────────┴─────────────────┤
│           Console / Status (140px)                  │
└─────────────────────────────────────────────────────┘
```

**Layout Steps:**
1. Split viewport into Bottom (140px) and TopRest
2. Split TopRest into Left (300px) and RightRest  
3. Split RightRest into Right (360px) and Center
4. Dock windows by name:
   - `"Cartograph/Tools"` → Left
   - `"Cartograph/Canvas"` → Center
   - `"Cartograph/Inspector"` → Right
   - `"Cartograph/Console"` → Bottom

**Lock Mechanism:**
- Sets node flags: `NoTabBar`, `NoWindowMenuButton`, `NoCloseButton`
- Windows created with: `NoMove`, `NoCollapse`, `NoDocking`
- Result: Users cannot rearrange, undock, or close panels

#### Updated All Panel Functions
- `RenderToolsPanel()` → "Cartograph/Tools" with locked flags
- `RenderCanvasPanel()` → "Cartograph/Canvas" with locked flags
- `RenderPropertiesPanel()` → "Cartograph/Inspector" with locked flags
- `RenderStatusBar()` → "Cartograph/Console" with locked flags

## Layout Behavior

### What Users CAN Do:
- Resize panel dividers (splitters between panels)
- Use all functionality within each panel
- Interact with menu bar normally

### What Users CANNOT Do:
- Drag/move panels
- Undock panels into floating windows
- Close panels
- Split panels into tabs
- Rearrange panel order

## Technical Notes

1. **First Run Only**: Layout is built programmatically only on the first frame when `m_layoutInitialized == false`

2. **ImGui Internal API**: Uses `imgui_internal.h` for DockBuilder functions (these are stable in docking branch)

3. **Extensibility**: To add/modify panels in the future:
   - Update `BuildFixedLayout()` split logic
   - Add new window with "Cartograph/" prefix
   - Call `DockBuilderDockWindow()` with the new window name

4. **Dev Override**: Can add a dev-only flag to rebuild layout if needed (e.g., for testing different layouts)

## Future Enhancements (TODOs in code)

### Tools Panel
- [ ] Add tile palette section
- [ ] Add icons palette (scrollable grid)
- [ ] Add layer toggles (Tiles, Rooms, Doors, Markers, Notes, Grid)

### Canvas Panel
- Already has pan/zoom input handling
- Rendering hooked up to main loop

### Inspector Panel
- [ ] Context-sensitive properties for selected items
- [ ] Room properties (id, name, rect, color, notes)
- [ ] Door properties (endpoints, type, gate)
- [ ] Marker properties (kind, label, icon, color, notes)
- [ ] Tile properties (type, palette color)

### Console Panel  
- [ ] Error banner area (pinned at top)
- [ ] Ring buffer with severity filtering (Error, Warn, Info)
- [ ] Cursor position display
- [ ] Autosave countdown
- [ ] Last save/export result

## Verification

To verify the layout is working:
1. Build the project: `mkdir build && cd build && cmake .. && make`
2. Run: `./Cartograph.app/Contents/MacOS/Cartograph`
3. Observe the fixed 4-panel layout
4. Try to drag/undock panels - they should be locked in place
5. Resize dividers - this should work normally

## API Reference

Key ImGui docking APIs used:
- `ImGui::DockSpace()` - Create docking area
- `ImGui::DockBuilderRemoveNode()` - Clear existing layout
- `ImGui::DockBuilderAddNode()` - Add dock node
- `ImGui::DockBuilderSetNodeSize()` - Set node dimensions
- `ImGui::DockBuilderSplitNode()` - Split node into regions
- `ImGui::DockBuilderDockWindow()` - Dock window into node
- `ImGui::DockBuilderGetNode()` - Get node for flag setting
- `ImGui::DockBuilderFinish()` - Finalize layout

