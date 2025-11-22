# Welcome Screen - Quick Reference

A cheat sheet for developers working with the welcome screen system.

---

## 🎯 Quick Links

| What | Where |
|------|-------|
| State Management | `src/App.h`, `src/App.cpp` |
| Welcome UI | `src/UI.cpp::RenderWelcomeScreen()` |
| Project Modal | `src/UI.cpp::RenderNewProjectModal()` |
| Templates | `src/UI.cpp::ApplyTemplate()` |
| State Variables | `src/UI.h` (lines 70-78) |

---

## 🔄 State Transitions

### Show Welcome Screen
```cpp
app.ShowWelcomeScreen();
// Sets m_appState = AppState::Welcome
```

### Show Editor
```cpp
app.ShowEditor();
// Sets m_appState = AppState::Editor
// Initializes model if needed
```

### Check Current State
```cpp
if (app.GetState() == AppState::Welcome) {
    // Handle welcome state
}
```

---

## 🎨 UI Components

### Main Welcome Screen
```cpp
// Located in: UI.cpp::RenderWelcomeScreen()
- ASCII Art Title
- Create New Project Button
- Import Project Button (stubbed)
- Recent Projects List (if available)
- What's New Button
```

### New Project Modal
```cpp
// Located in: UI.cpp::RenderNewProjectModal()
- Project Name Input
- Template Radio Buttons
- Cell Size Slider
- Map Width/Height Inputs
- Live Preview Stats
- Create/Cancel Buttons
```

---

## 📋 Adding a New Template

**1. Add to enum** (`src/UI.h`):
```cpp
enum class ProjectTemplate {
    Custom, Small, Medium, Large, Metroidvania,
    YourTemplate  // Add here
};
```

**2. Add radio button** (`src/UI.cpp::RenderNewProjectModal()`):
```cpp
if (ImGui::RadioButton("Your Template Name (512x512)",
    selectedTemplate == ProjectTemplate::YourTemplate)) {
    selectedTemplate = ProjectTemplate::YourTemplate;
    ApplyTemplate(ProjectTemplate::YourTemplate);
}
```

**3. Define settings** (`src/UI.cpp::ApplyTemplate()`):
```cpp
case ProjectTemplate::YourTemplate:
    newProjectConfig.cellSize = 20;
    newProjectConfig.mapWidth = 512;
    newProjectConfig.mapHeight = 512;
    break;
```

---

## 💾 Project Configuration

### NewProjectConfig Structure
```cpp
struct NewProjectConfig {
    char projectName[256];  // Project name
    int cellSize;           // 8-64 pixels
    int mapWidth;           // 16-1024 cells
    int mapHeight;          // 16-1024 cells
};
```

### Applying Configuration
```cpp
// In RenderNewProjectModal() on Create button:
model.meta.title = std::string(newProjectConfig.projectName);
model.grid.tileSize = newProjectConfig.cellSize;
model.grid.cols = newProjectConfig.mapWidth;
model.grid.rows = newProjectConfig.mapHeight;
model.InitDefaultPalette();
model.InitDefaultKeymap();
model.InitDefaultTheme("Dark");
```

---

## 🎭 Showing Modals

### New Project Modal
```cpp
ui.showNewProjectModal = true;
// Modal will render on next frame
```

### What's New Panel
```cpp
ui.showWhatsNew = true;
// Panel will render on next frame
```

---

## 📢 Toast Notifications

### Show a Toast
```cpp
ui.ShowToast("Message text", Toast::Type::Info, 3.0f);
```

### Toast Types
```cpp
Toast::Type::Info      // Blue
Toast::Type::Success   // Green
Toast::Type::Warning   // Yellow
Toast::Type::Error     // Red
```

---

## 🗂️ Recent Projects

### Data Structure
```cpp
struct RecentProject {
    std::string path;           // Absolute path to .cart file
    std::string name;           // Display name
    std::string lastModified;   // Timestamp string
};
```

### Accessing Recent Projects
```cpp
// In UI class:
std::vector<RecentProject> recentProjects;

// Iterate:
for (const auto& recent : ui.recentProjects) {
    std::cout << recent.name << " at " << recent.path << std::endl;
}
```

### Loading Recent Projects (TODO)
```cpp
void UI::LoadRecentProjects() {
    // 1. Read from Platform::GetUserDataDir() + "recent_projects.json"
    // 2. Parse JSON
    // 3. Validate paths
    // 4. Populate recentProjects vector
}
```

### Adding to Recent (TODO)
```cpp
void UI::AddRecentProject(const std::string& path) {
    // 1. Create RecentProject entry
    // 2. Add to front of list
    // 3. Save to JSON
}
```

---

## 🎨 Customizing Colors

### Welcome Screen Background
```cpp
// In App.cpp::Render()
Color bgColor = (m_appState == AppState::Welcome) 
    ? Color(0.1f, 0.1f, 0.12f, 1.0f)  // Change this
    : m_model.theme.background;
```

### ASCII Art Color
```cpp
// In UI.cpp::RenderWelcomeScreen()
ImGui::TextColored(
    ImVec4(0.4f, 0.7f, 1.0f, 1.0f),  // Change this (RGBA)
    "ASCII art text here"
);
```

### Button Colors
Use ImGui theme colors or push style colors:
```cpp
ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
ImGui::Button("Green Button", ImVec2(400, 60));
ImGui::PopStyleColor();
```

---

## 🔧 Common Tasks

### Change Default Template
```cpp
// In UI.h:
ProjectTemplate selectedTemplate = ProjectTemplate::Small;  // Change here
```

### Change Default Project Name
```cpp
// In UI.cpp::RenderWelcomeScreen():
std::strcpy(newProjectConfig.projectName, "My Default Name");
```

### Change Cell Size Range
```cpp
// In UI.cpp::RenderNewProjectModal():
ImGui::SliderInt("Cell Size (px)", &newProjectConfig.cellSize,
    4, 128);  // Change min/max
```

### Change Map Size Limits
```cpp
// In UI.cpp::RenderNewProjectModal():
if (newProjectConfig.mapWidth < 8) newProjectConfig.mapWidth = 8;
if (newProjectConfig.mapWidth > 2048) newProjectConfig.mapWidth = 2048;
```

---

## 🐛 Debugging

### Print Current State
```cpp
std::cout << "App State: " 
          << (app.GetState() == AppState::Welcome ? "Welcome" : "Editor")
          << std::endl;
```

### Print Modal State
```cpp
std::cout << "New Project Modal: " 
          << (ui.showNewProjectModal ? "Open" : "Closed")
          << std::endl;
```

### Print Configuration
```cpp
std::cout << "Config: " 
          << ui.newProjectConfig.projectName << ", "
          << ui.newProjectConfig.cellSize << "px, "
          << ui.newProjectConfig.mapWidth << "x"
          << ui.newProjectConfig.mapHeight
          << std::endl;
```

---

## 📱 Platform-Specific Code

### Get Config Directory
```cpp
std::string configDir = Platform::GetUserDataDir();
// macOS: ~/Library/Application Support/Unveil Cartograph/
// Windows: %APPDATA%/Unveil/Cartograph/
// Linux: ~/.local/share/unveil-cartograph/
```

### Check if File Exists
```cpp
if (Platform::Fs::FileExists(path)) {
    // File exists
}
```

---

## 🎯 Common Workflows

### Opening a Project from Welcome Screen
```cpp
// User clicks recent project or import button
app.OpenProject("/path/to/project.cart");
app.ShowEditor();
ui.AddRecentProject("/path/to/project.cart");  // TODO: Implement
```

### Creating a New Project
```cpp
// User configures and clicks Create in modal
model.meta.title = "New Map";
model.grid.tileSize = 16;
model.grid.cols = 256;
model.grid.rows = 256;
model.InitDefaults();
app.ShowEditor();
```

### Returning to Welcome Screen (Future)
```cpp
// From File menu or keyboard shortcut
app.ShowWelcomeScreen();
// Note: Currently not implemented in UI
```

---

## 🚨 Important Notes

### Modal Lifecycle
- Modal is shown when `showNewProjectModal = true`
- Modal auto-opens popup on first render
- Must call `ImGui::CloseCurrentPopup()` to close
- Set `showNewProjectModal = false` after closing

### State Initialization
- Welcome state doesn't fully initialize Model
- Only theme is initialized for UI
- Full model init happens in `app.ShowEditor()`

### Validation
- Cell size: 8-64 pixels
- Map dimensions: 16-1024 cells
- Validation happens in modal, not in Model

### Memory Management
- Recent projects stored in `std::vector`
- No limit currently enforced (should limit to 10)
- Thumbnails not cached (memory leak risk if implemented)

---

## 📚 Related Documentation

- `WELCOME_SCREEN_IMPLEMENTATION.md` - Full implementation details
- `WELCOME_FLOW_DIAGRAM.md` - Visual flow diagrams
- `WELCOME_SCREEN_TODOS.md` - Outstanding TODOs
- `ARCHITECTURE.md` - Overall architecture
- `INTEGRATION_NOTES.md` - Integration guidelines

---

## 🔑 Key Files at a Glance

```
src/
├── App.h                    [AppState enum, state methods]
├── App.cpp                  [State transitions, conditional rendering]
├── UI.h                     [Welcome screen types, state vars]
└── UI.cpp                   [Welcome screen implementation]
    ├── RenderWelcomeScreen()        [Main welcome UI]
    ├── RenderNewProjectModal()      [Project configuration]
    ├── RenderRecentProjectsList()   [Recent projects (stub)]
    ├── RenderWhatsNewPanel()        [Changelog]
    ├── ApplyTemplate()              [Template presets]
    ├── LoadRecentProjects()         [TODO: Persistence]
    └── AddRecentProject()           [TODO: Save to config]
```

---

## 💡 Pro Tips

1. **Use templates for quick testing**: Switch template to set up test projects fast
2. **Check state before operations**: Always verify app state before state-dependent actions
3. **Toast for feedback**: Show toast notifications for user actions
4. **Validate early**: Validate inputs in UI before applying to model
5. **Keep welcome fast**: Welcome screen should be instant, avoid heavy operations
6. **Stub with TODOs**: Clearly mark incomplete features with TODO comments
7. **Test transitions**: Verify smooth transitions between states

---

**Quick Reference Version:** 1.0  
**Last Updated:** November 22, 2025  
**For:** Cartograph Welcome Screen System

