#pragma once

#include <string>
#include <unordered_map>

namespace Cartograph {

/**
 * Keymap manager.
 * Handles key binding storage and input matching.
 */
class KeymapManager {
public:
    KeymapManager();
    
    /**
     * Set a key binding.
     * @param action Action name (e.g., "paint", "undo")
     * @param binding Key binding string (e.g., "Ctrl+S", "Mouse1")
     */
    void SetBinding(const std::string& action, const std::string& binding);
    
    /**
     * Get a key binding.
     * @param action Action name
     * @return Binding string, or empty if not found
     */
    std::string GetBinding(const std::string& action) const;
    
    /**
     * Check if an action is bound to the current input state.
     * @param action Action name
     * @return true if action is triggered
     */
    bool IsActionTriggered(const std::string& action) const;
    
    /**
     * Load bindings from a map.
     * @param bindings Map of action -> binding
     */
    void LoadBindings(const std::unordered_map<std::string, std::string>& bindings);
    
    /**
     * Get all bindings.
     * @return Map of action -> binding
     */
    const std::unordered_map<std::string, std::string>& GetAllBindings() const {
        return m_bindings;
    }
    
private:
    std::unordered_map<std::string, std::string> m_bindings;
};

} // namespace Cartograph

