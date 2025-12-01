#!/usr/bin/env python3
# Extract and adapt RenderCanvasPanel to CanvasPanel.cpp

# Read the original UI.cpp
with open('src/UI.cpp', 'r') as f:
    ui_lines = f.readlines()

# Extract GetTilesAlongLine helper (lines 62-97)
helper_start = 61  # 0-indexed
helper_end = 97
helper_lines = ui_lines[helper_start:helper_end]

# Extract DetectEdgeHover (lines 1974-2033)  
detect_start = 1973
detect_end = 2033
detect_lines = ui_lines[detect_start:detect_end]

# Extract RenderCanvasPanel (lines 2035-3783)
render_start = 2034
render_end = 3783
render_lines = ui_lines[render_start:render_end]

# Build the new file
output = []

# Header
output.append('#include "CanvasPanel.h"\n')
output.append('#include "../App.h"\n')
output.append('#include <imgui.h>\n')
output.append('#include <imgui_internal.h>\n')
output.append('#include <cfloat>\n')
output.append('#include <cmath>\n')
output.append('#include <algorithm>\n')
output.append('#include <set>\n')
output.append('#include <cstring>\n')
output.append('\n')
output.append('namespace Cartograph {\n')
output.append('\n')
output.append('// ' + '='*76 + '\n')
output.append('// Helper functions\n')
output.append('// ' + '='*76 + '\n')
output.append('\n')

# Add GetTilesAlongLine (make it non-static and with proper signature)
output.append('/**\n')
output.append(' * Get all tiles along a line from (x0, y0) to (x1, y1).\n')
output.append(' * Uses Bresenham\'s line algorithm to ensure continuous painting.\n')
output.append(' */\n')
for line in helper_lines:
    # Remove 'static ' prefix
    adapted_line = line.replace('static std::vector', 'std::vector')
    output.append(adapted_line)

output.append('\n')
output.append('// ' + '='*76 + '\n')
output.append('// CanvasPanel implementation\n')
output.append('// ' + '='*76 + '\n')
output.append('\n')
output.append('CanvasPanel::CanvasPanel() {\n')
output.append('}\n')
output.append('\n')
output.append('CanvasPanel::~CanvasPanel() {\n')
output.append('}\n')
output.append('\n')

# Add DetectEdgeHover (adapt from UI:: to CanvasPanel::)
for line in detect_lines:
    adapted_line = line.replace('bool UI::DetectEdgeHover', 'bool CanvasPanel::DetectEdgeHover')
    output.append(adapted_line)

output.append('\n')

# Add Render function (adapt from UI::RenderCanvasPanel to CanvasPanel::Render)
for line in render_lines:
    adapted_line = line.replace('void UI::RenderCanvasPanel', 'void CanvasPanel::Render')
    
    # Replace UI member access with CanvasPanel members
    # Note: Some members like showPropertiesPanel are pointers now
    adapted_line = adapted_line.replace('showPropertiesPanel = !showPropertiesPanel', 
                                       '*showPropertiesPanel = !(*showPropertiesPanel)')
    adapted_line = adapted_line.replace('m_layoutInitialized = false', 
                                       '*layoutInitialized = false')
    
    # Remove ShowToast calls (we'll handle this differently later)
    if 'ShowToast(' in adapted_line:
        # Comment it out for now
        adapted_line = '// TODO: Re-enable toast: ' + adapted_line.lstrip()
    
    output.append(adapted_line)

output.append('\n')
output.append('} // namespace Cartograph\n')

# Write the new file
with open('src/UI/CanvasPanel.cpp', 'w') as f:
    f.writelines(output)

print(f'Created CanvasPanel.cpp with {len(output)} lines')
print('Successfully extracted and adapted RenderCanvasPanel')

