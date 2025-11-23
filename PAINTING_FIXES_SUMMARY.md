# Painting Fixes Summary

## Issues Fixed

### Issue 1: Undo Not Working for Painting
**Problem:** Room paint mode was directly modifying the model without creating history commands, making undo/redo impossible.

**Solution:**
- Created new `ModifyRoomAssignmentsCommand` class in `History.h` and `History.cpp`
- Added state tracking in `UI.h`: `isPaintingRoomCells`, `lastRoomPaintX`, `lastRoomPaintY`, `currentRoomAssignments`
- Updated room paint mode in `UI.cpp` to:
  - Track cell assignment changes during painting
  - Apply changes immediately for visual feedback
  - Create and commit command to history on mouse release
  - Support both assigning cells to rooms and unassigning them

### Issue 2: Gaps When Drawing Quickly
**Problem:** When dragging the mouse quickly, tiles were only painted at discrete mouse sample points, creating gaps in lines.

**Solution:**
- Implemented Bresenham's line algorithm in `GetTilesAlongLine()` helper function
- Updated all painting/erasing operations to interpolate between last position and current position:
  - Regular tile painting (Paint tool)
  - Tile erasing (Paint tool with E key or right-click)
  - Standalone Erase tool
  - Room cell assignment (room paint mode)
  - Room cell unassignment (room paint mode)

## Files Modified

### src/History.h
- Added `ModifyRoomAssignmentsCommand` class with:
  - `CellAssignment` struct (x, y, oldRoomId, newRoomId)
  - Execute/Undo/GetDescription methods

### src/History.cpp
- Implemented `ModifyRoomAssignmentsCommand`:
  - Execute: Apply new room assignments
  - Undo: Restore old room assignments

### src/UI.h
- Added state variables for room painting:
  - `isPaintingRoomCells`
  - `lastRoomPaintX`, `lastRoomPaintY`
  - `currentRoomAssignments`

### src/UI.cpp
- Added `GetTilesAlongLine()` helper function (Bresenham's algorithm)
- Updated regular tile painting to use line interpolation
- Updated tile erasing (in Paint tool) to use line interpolation
- Updated standalone Erase tool to use line interpolation
- Updated room paint mode to:
  - Use line interpolation for continuous strokes
  - Track changes and commit to history on mouse release
  - Support both assignment and unassignment with history

## How It Works

### Line Interpolation
When the user drags to paint/erase:
1. On first click, paint/erase the starting tile
2. On subsequent frames while dragging:
   - If mouse moved to a different tile:
     - Calculate all tiles along the line from last position to current
     - Paint/erase each tile in sequence
     - Update last position to current position

### History Integration
For room painting (and all painting operations):
1. While dragging:
   - Build up a list of changes (tile changes or room assignments)
   - Apply each change immediately for visual feedback
2. On mouse release:
   - Create appropriate command with all accumulated changes
   - Add command to history with `execute=false` (already applied)
   - Clear the accumulated changes list
3. Undo/Redo:
   - Command restores/reapplies all changes atomically

## Testing

All painting modes now support:
- **Continuous strokes**: No gaps when drawing fast
- **Undo/Redo**: Complete operation is undoable as a single action
- **Visual feedback**: Changes appear immediately while dragging

### Test Cases
1. ✅ Paint tiles with rapid mouse movement - should draw continuous line
2. ✅ Erase tiles with rapid mouse movement - should erase continuous line
3. ✅ Paint room cells with rapid movement - should assign continuous path
4. ✅ Undo tile painting - should undo entire stroke
5. ✅ Undo room cell assignment - should undo entire stroke
6. ✅ Redo after undo - should restore entire stroke

## Technical Details

### Bresenham's Line Algorithm
The algorithm efficiently calculates all integer grid points along a line:
- Time complexity: O(max(dx, dy))
- Space complexity: O(max(dx, dy)) for result vector
- Guarantees connected path with no gaps

### Command Pattern
Each painting operation creates a command that:
- Stores old and new state for each affected tile/cell
- Can be executed and undone atomically
- Integrates with the history stack for undo/redo

## Notes

- All changes are backward compatible with existing save files
- The interpolation is deterministic and produces consistent results
- History commands properly track both tile IDs and room assignments
- Empty room ID ("") is used to represent "unassigned" state

