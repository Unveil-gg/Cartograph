#pragma once

#include <functional>
#include <string>
#include <memory>

namespace Cartograph {

// Forward declarations
class App;
class Model;
class Canvas;
class History;
class IconManager;
class JobQueue;

/**
 * Menu item callback function type.
 * Called when a menu item is selected.
 */
using MenuCallback = std::function<void()>;

/**
 * Native menu interface for platform-specific menu implementations.
 * 
 * On macOS: Uses NSMenu and NSMenuItem for native OS integration
 * On Windows/Linux: Falls back to ImGui-rendered menu bar
 */
class INativeMenu {
public:
    virtual ~INativeMenu() = default;
    
    /**
     * Initialize the menu system.
     * On macOS, this sets up the application menu bar.
     * On Windows/Linux, this prepares for ImGui rendering.
     */
    virtual void Initialize() = 0;
    
    /**
     * Update menu state (enable/disable items, checkmarks, etc.).
     * Called every frame.
     */
    virtual void Update(
        App& app,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        JobQueue& jobs
    ) = 0;
    
    /**
     * Render the menu (for ImGui-based implementations).
     * For native implementations, this is a no-op.
     */
    virtual void Render() = 0;
    
    /**
     * Check if this implementation uses native OS menus.
     * @return true for native (macOS), false for ImGui fallback
     */
    virtual bool IsNative() const = 0;
    
    /**
     * Set callback for a menu action.
     * @param action Action identifier (e.g., "file.new", "edit.undo")
     * @param callback Function to call when action is triggered
     */
    virtual void SetCallback(
        const std::string& action,
        MenuCallback callback
    ) = 0;
};

/**
 * Factory function to create the appropriate menu implementation
 * for the current platform.
 * @return Smart pointer to platform-specific menu implementation
 */
std::unique_ptr<INativeMenu> CreateNativeMenu();

} // namespace Cartograph

