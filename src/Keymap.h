#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace Cartograph {

/**
 * Parsed representation of a key binding.
 * Uses int for key to avoid ImGui header dependency.
 * -1 = no key, otherwise ImGuiKey value.
 */
struct ParsedBinding {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool cmd = false;   // Command key (Mac) / Super key
    int key = -1;  // ImGuiKey value, or -1 if none
    int mouseButton = -1;  // -1 = keyboard, 0-2 = mouse buttons
    
    bool IsValid() const;
};

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
    
    /**
     * Validate a binding string.
     * @param binding Binding string to validate
     * @return true if binding is valid
     */
    bool IsBindingValid(const std::string& binding) const;
    
    /**
     * Find conflicting action for a binding.
     * @param binding Binding string to check
     * @param excludeAction Action to exclude from conflict check
     * @return Name of conflicting action, or empty if no conflict
     */
    std::string FindConflict(const std::string& binding, 
                             const std::string& excludeAction = "") const;
    
    /**
     * Get display name for a binding (formatted for UI).
     * @param binding Binding string
     * @return Formatted display name
     */
    std::string GetBindingDisplayName(const std::string& binding) const;
    
    /**
     * Parse a binding string into a ParsedBinding.
     * @param binding Binding string (e.g., "Ctrl+S")
     * @return Parsed binding, or std::nullopt if invalid
     */
    std::optional<ParsedBinding> ParseBinding(
        const std::string& binding
    ) const;
    
private:
    std::unordered_map<std::string, std::string> m_bindings;
    
    // Cache parsed bindings for performance
    mutable std::unordered_map<std::string, ParsedBinding> m_parsedCache;
    
    /**
     * Check if a parsed binding matches current input state.
     * @param parsed Parsed binding to check
     * @return true if input matches
     */
    bool IsBindingPressed(const ParsedBinding& parsed) const;
};

} // namespace Cartograph

