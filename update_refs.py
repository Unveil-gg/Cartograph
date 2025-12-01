#!/usr/bin/env python3
# Update references to canvas panel state in UI.cpp

import re

with open('src/UI.cpp', 'r') as f:
    content = f.read()

# List of state variables that moved to CanvasPanel
moved_vars = [
    'currentTool',
    'selectedTileId',
    'hoveredTileX',
    'hoveredTileY',
    'isHoveringCanvas',
    'isSelecting',
    'selectionStartX',
    'selectionStartY',
    'selectionEndX',
    'selectionEndY',
    'isPainting',
    'lastPaintedTileX',
    'lastPaintedTileY',
    'currentPaintChanges',
    'twoFingerEraseActive',
    'isModifyingEdges',
    'currentEdgeChanges',
    'selectedIconName',
    'markerLabel',
    'markerColor',
    'markerColorHex',
    'selectedMarker',
    'hoveredMarker',
    'isDraggingMarker',
    'dragStartX',
    'dragStartY',
    'copiedMarkers',
    'hoveredEdge',
    'isHoveringEdge',
    'eyedropperAutoSwitchToPaint',
    'lastTool',
    'selectedRoomId',
    'roomPaintMode',
    'showRoomOverlays',
    'isPaintingRoomCells',
    'lastRoomPaintX',
    'lastRoomPaintY',
    'currentRoomAssignments',
    'eraserBrushSize',
]

# Also need to update Tool enum references
content = content.replace('Tool::', 'CanvasPanel::Tool::')

# Replace each variable reference with m_canvasPanel.variable
# But be careful not to replace within comments or strings
for var in moved_vars:
    # Match word boundaries to avoid partial matches
    # Don't replace if already prefixed with m_canvasPanel.
    pattern = r'(?<!m_canvasPanel\.)(?<!\.)\b' + var + r'\b'
    replacement = 'm_canvasPanel.' + var
    content = re.sub(pattern, replacement, content)

# Write back
with open('src/UI.cpp', 'w') as f:
    f.write(content)

print('Updated references to canvas panel state in UI.cpp')

