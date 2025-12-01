#!/usr/bin/env python3
# Remove old canvas-specific implementations from UI.cpp

with open('src/UI.cpp', 'r') as f:
    lines = f.readlines()

# Find and mark sections to remove
remove_sections = []

# Find DetectEdgeHover (should be around line 1974-2033)
for i, line in enumerate(lines):
    if 'bool UI::DetectEdgeHover(' in line:
        # Find the end of this function (closing brace)
        start = i
        brace_count = 0
        found_open = False
        for j in range(i, min(i + 100, len(lines))):
            if '{' in lines[j]:
                found_open = True
                brace_count += lines[j].count('{')
            brace_count -= lines[j].count('}')
            if found_open and brace_count == 0:
                remove_sections.append((start, j + 1))
                print(f'Found DetectEdgeHover: lines {start+1} to {j+1}')
                break
        break

# Find RenderCanvasPanel (should be around line 2035-3783)
for i, line in enumerate(lines):
    if 'void UI::RenderCanvasPanel(' in line:
        # Find the end of this function
        start = i
        brace_count = 0
        found_open = False
        for j in range(i, min(i + 2000, len(lines))):
            if '{' in lines[j]:
                found_open = True
                brace_count += lines[j].count('{')
            brace_count -= lines[j].count('}')
            if found_open and brace_count == 0:
                remove_sections.append((start, j + 1))
                print(f'Found RenderCanvasPanel: lines {start+1} to {j+1}')
                break
        break

# Find LoadEyedropperCursor
for i, line in enumerate(lines):
    if 'void UI::LoadEyedropperCursor(' in line:
        start = i
        brace_count = 0
        found_open = False
        for j in range(i, min(i + 100, len(lines))):
            if '{' in lines[j]:
                found_open = True
                brace_count += lines[j].count('{')
            brace_count -= lines[j].count('}')
            if found_open and brace_count == 0:
                remove_sections.append((start, j + 1))
                print(f'Found LoadEyedropperCursor: lines {start+1} to {j+1}')
                break
        break

# Find UpdateCursor
for i, line in enumerate(lines):
    if 'void UI::UpdateCursor(' in line:
        start = i
        brace_count = 0
        found_open = False
        for j in range(i, min(i + 100, len(lines))):
            if '{' in lines[j]:
                found_open = True
                brace_count += lines[j].count('{')
            brace_count -= lines[j].count('}')
            if found_open and brace_count == 0:
                remove_sections.append((start, j + 1))
                print(f'Found UpdateCursor: lines {start+1} to {j+1}')
                break
        break

# Sort sections by start line (descending) so we can remove from end to start
remove_sections.sort(reverse=True)

# Remove the sections
output_lines = lines.copy()
for start, end in remove_sections:
    print(f'Removing lines {start+1} to {end}')
    del output_lines[start:end]

# Write the cleaned file
with open('src/UI.cpp', 'w') as f:
    f.writelines(output_lines)

original_count = len(lines)
new_count = len(output_lines)
print(f'\nOriginal: {original_count} lines')
print(f'New: {new_count} lines')
print(f'Removed: {original_count - new_count} lines')

