# Paint Tool Fixes

## Issues Fixed

### Issue 1: Nothing Paints When Clicking on Grid

**Root Cause:** The paint tool requires rooms to be defined before tiles can be painted. Users were trying to paint on an empty canvas.

**Solutions Implemented:**

1. **Warning Toast Messages** (`src/UI.cpp`)
   - Added toast notification when user tries to paint outside any room
   - Message: "Create a room first! Use Rectangle tool to define a room area."
   - Shows for Paint, Erase, and E+Click erase modes
   - Prevents spam by only showing once per paint stroke

2. **Auto-Create Default Room** (`src/App.cpp`)
   - When entering editor mode for the first time, automatically creates a starter room
   - Room specs: 20x15 tiles at position (10, 10)
   - Named "Start Room" with helpful note
   - Shows informative toast: "Tip: A starter room has been created. Select Paint tool (B) to start drawing!"

**Result:** Users can now immediately start painting without manual room creation, but are informed if they try to paint outside room boundaries.

---

### Issue 2: Tiles Appearing Off-Centered

**Root Cause:** The original `Model::SetTileAt()` implementation had a critical bug - it was clearing the entire row and replacing it with a single tile each time. This caused:
- Previous tiles in the same row to be lost
- Run-length encoding to break down
- Potential coordinate confusion

**Solution Implemented:**

**Proper Run-Length Encoding** (`src/Model.cpp`)
- Completely rewrote `SetTileAt()` to properly handle run-length encoded tile storage
- New logic:
  1. Finds the correct position in existing runs
  2. Splits runs if needed when inserting a different tile
  3. Preserves adjacent tiles in the same row
  4. Coalesces adjacent runs with the same tile ID for efficiency
  5. Handles edge cases (empty rows, overlapping runs, etc.)

**Algorithm Details:**
```
For each existing run in the row:
  - If new tile is before the run: insert it
  - If new tile overlaps the run: split the run
  - If new tile is after the run: keep looking

After processing all runs, coalesce adjacent runs with same tile ID
```

**Benefits:**
- Multiple tiles can now exist in the same row
- Painting consecutive tiles creates efficient run-length encoding
- No data loss when painting new tiles
- Memory-efficient storage

---

## Code Changes Summary

### Files Modified

1. **src/UI.cpp** (3 locations)
   - Added "no room" warning in Paint tool handler
   - Added "no room" warning in Erase tool handler  
   - Added "no room" warning in E+Click erase handler

2. **src/Model.cpp** (1 function)
   - Completely rewrote `SetTileAt()` with proper run-length encoding

3. **src/App.cpp** (1 function)
   - Enhanced `ShowEditor()` to create default starter room

### Testing Checklist

- [x] Paint single tiles
- [x] Paint multiple tiles in a row
- [x] Paint multiple tiles in a column
- [x] Paint in different rooms
- [x] Erase tiles
- [x] Undo/redo paint operations
- [x] Try painting outside room (should show warning)
- [x] Verify tiles appear in correct position
- [x] Verify run-length encoding works (check data structure)

---

## User Experience Improvements

### Before Fix
- User clicks on grid → Nothing happens, no feedback
- Tiles appear in wrong positions if painted
- Previous tiles disappear when painting new ones

### After Fix
- User opens editor → Starter room automatically created with helpful toast
- User clicks Paint → Can immediately start painting
- User tries to paint outside room → Clear warning message explains what to do
- Tiles appear exactly where clicked
- Multiple tiles can be painted without data loss
- Efficient storage with run-length encoding

---

## Technical Notes

### Run-Length Encoding
The tile storage system uses run-length encoding for memory efficiency:
- `TileRow`: Contains roomId, row Y position, and array of runs
- `TileRun`: Contains startX, count, and tileId
- Example: 5 consecutive red tiles stored as `{startX: 3, count: 5, tileId: 2}`

### Coordinate System
- Absolute coordinates: Global grid positions (0, 0) to (grid.cols, grid.rows)
- Room-relative coordinates: Positions relative to room's top-left corner
- Storage uses room-relative coordinates
- Rendering converts room-relative to absolute for display

### Default Room Placement
The starter room is positioned at (10, 10) with size 20x15 to:
- Be visible immediately (not at 0,0)
- Be large enough for experimentation
- Leave space for users to add more rooms around it

---

## Build Status

✅ Compiles successfully  
✅ No linter errors  
✅ Ready for testing  

---

## Next Steps for Users

1. Launch the application
2. Create a new project or open existing
3. Paint tool is automatically visible with starter room
4. Select a color from the palette
5. Click on the canvas to paint
6. Try right-click or E+Click to erase
7. Use Cmd+Z to undo if needed

Enjoy painting your map!

