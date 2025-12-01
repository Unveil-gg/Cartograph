#include "Keymap.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Cartograph {

// Helper: Trim whitespace from string
static std::string Trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace(str[start])) {
        start++;
    }
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }
    
    return str.substr(start, end - start);
}

// Helper: Convert string to lowercase
static std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Helper: Map key name to ImGuiKey (returns int)
static int KeyNameToImGuiKey(const std::string& keyName) {
    std::string lower = ToLower(keyName);
    
    // Single characters (A-Z, 0-9)
    if (lower.length() == 1) {
        char c = lower[0];
        if (c >= 'a' && c <= 'z') {
            return (int)ImGuiKey_A + (c - 'a');
        }
        if (c >= '0' && c <= '9') {
            return (int)ImGuiKey_0 + (c - '0');
        }
    }
    
    // Special keys
    if (lower == "space") return (int)ImGuiKey_Space;
    if (lower == "enter" || lower == "return") return (int)ImGuiKey_Enter;
    if (lower == "escape" || lower == "esc") return (int)ImGuiKey_Escape;
    if (lower == "backspace") return (int)ImGuiKey_Backspace;
    if (lower == "delete" || lower == "del") return (int)ImGuiKey_Delete;
    if (lower == "tab") return (int)ImGuiKey_Tab;
    if (lower == "left") return (int)ImGuiKey_LeftArrow;
    if (lower == "right") return (int)ImGuiKey_RightArrow;
    if (lower == "up") return (int)ImGuiKey_UpArrow;
    if (lower == "down") return (int)ImGuiKey_DownArrow;
    if (lower == "home") return (int)ImGuiKey_Home;
    if (lower == "end") return (int)ImGuiKey_End;
    if (lower == "pageup") return (int)ImGuiKey_PageUp;
    if (lower == "pagedown") return (int)ImGuiKey_PageDown;
    
    // Function keys
    if (lower == "f1") return (int)ImGuiKey_F1;
    if (lower == "f2") return (int)ImGuiKey_F2;
    if (lower == "f3") return (int)ImGuiKey_F3;
    if (lower == "f4") return (int)ImGuiKey_F4;
    if (lower == "f5") return (int)ImGuiKey_F5;
    if (lower == "f6") return (int)ImGuiKey_F6;
    if (lower == "f7") return (int)ImGuiKey_F7;
    if (lower == "f8") return (int)ImGuiKey_F8;
    if (lower == "f9") return (int)ImGuiKey_F9;
    if (lower == "f10") return (int)ImGuiKey_F10;
    if (lower == "f11") return (int)ImGuiKey_F11;
    if (lower == "f12") return (int)ImGuiKey_F12;
    
    // Symbols
    if (lower == "=" || lower == "equal") return (int)ImGuiKey_Equal;
    if (lower == "-" || lower == "minus") return (int)ImGuiKey_Minus;
    if (lower == "+" || lower == "plus") return (int)ImGuiKey_Equal;  // + is Shift+=
    if (lower == "[") return (int)ImGuiKey_LeftBracket;
    if (lower == "]") return (int)ImGuiKey_RightBracket;
    if (lower == ";" || lower == "semicolon") return (int)ImGuiKey_Semicolon;
    if (lower == "'" || lower == "apostrophe") return (int)ImGuiKey_Apostrophe;
    if (lower == "," || lower == "comma") return (int)ImGuiKey_Comma;
    if (lower == "." || lower == "period") return (int)ImGuiKey_Period;
    if (lower == "/" || lower == "slash") return (int)ImGuiKey_Slash;
    if (lower == "\\" || lower == "backslash") return (int)ImGuiKey_Backslash;
    if (lower == "`" || lower == "grave") return (int)ImGuiKey_GraveAccent;
    
    return -1;  // ImGuiKey_None
}

// Helper: Map ImGuiKey (int) to display name
static std::string ImGuiKeyToDisplayName(int key) {
    if (key >= (int)ImGuiKey_A && key <= (int)ImGuiKey_Z) {
        return std::string(1, 'A' + (key - (int)ImGuiKey_A));
    }
    if (key >= (int)ImGuiKey_0 && key <= (int)ImGuiKey_9) {
        return std::string(1, '0' + (key - (int)ImGuiKey_0));
    }
    
    if (key == (int)ImGuiKey_Space) return "Space";
    if (key == (int)ImGuiKey_Enter) return "Enter";
    if (key == (int)ImGuiKey_Escape) return "Esc";
    if (key == (int)ImGuiKey_Backspace) return "Backspace";
    if (key == (int)ImGuiKey_Delete) return "Delete";
    if (key == (int)ImGuiKey_Tab) return "Tab";
    if (key == (int)ImGuiKey_LeftArrow) return "Left";
    if (key == (int)ImGuiKey_RightArrow) return "Right";
    if (key == (int)ImGuiKey_UpArrow) return "Up";
    if (key == (int)ImGuiKey_DownArrow) return "Down";
    if (key == (int)ImGuiKey_Equal) return "=";
    if (key == (int)ImGuiKey_Minus) return "-";
    if (key == (int)ImGuiKey_F1) return "F1";
    if (key == (int)ImGuiKey_F2) return "F2";
    if (key == (int)ImGuiKey_F3) return "F3";
    if (key == (int)ImGuiKey_F4) return "F4";
    if (key == (int)ImGuiKey_F5) return "F5";
    if (key == (int)ImGuiKey_F6) return "F6";
    if (key == (int)ImGuiKey_F7) return "F7";
    if (key == (int)ImGuiKey_F8) return "F8";
    if (key == (int)ImGuiKey_F9) return "F9";
    if (key == (int)ImGuiKey_F10) return "F10";
    if (key == (int)ImGuiKey_F11) return "F11";
    if (key == (int)ImGuiKey_F12) return "F12";
    
    return "";
}

bool ParsedBinding::IsValid() const {
    // Must have either a key or a mouse button
    return (key != ImGuiKey_None) || (mouseButton >= 0);
}

KeymapManager::KeymapManager() {
}

void KeymapManager::SetBinding(const std::string& action, 
                                const std::string& binding) {
    m_bindings[action] = binding;
    // Clear cached parse for this binding
    m_parsedCache.erase(binding);
}

std::string KeymapManager::GetBinding(const std::string& action) const {
    auto it = m_bindings.find(action);
    return (it != m_bindings.end()) ? it->second : "";
}

std::optional<ParsedBinding> KeymapManager::ParseBinding(
    const std::string& binding
) const {
    if (binding.empty()) {
        return std::nullopt;
    }
    
    // Check cache first
    auto cacheIt = m_parsedCache.find(binding);
    if (cacheIt != m_parsedCache.end()) {
        return cacheIt->second;
    }
    
    ParsedBinding parsed;
    
    // Split by '+'
    std::vector<std::string> parts;
    std::stringstream ss(binding);
    std::string part;
    while (std::getline(ss, part, '+')) {
        parts.push_back(Trim(part));
    }
    
    if (parts.empty()) {
        return std::nullopt;
    }
    
    // Parse each part
    for (size_t i = 0; i < parts.size(); ++i) {
        std::string lower = ToLower(parts[i]);
        
        // Check for modifiers
        if (lower == "ctrl" || lower == "control") {
            parsed.ctrl = true;
        } else if (lower == "alt" || lower == "option") {
            parsed.alt = true;
        } else if (lower == "shift") {
            parsed.shift = true;
        } else if (lower == "cmd" || lower == "command" || 
                   lower == "super") {
            parsed.cmd = true;
        }
        // Check for mouse buttons
        else if (lower == "mouse1" || lower == "lmb" || 
                 lower == "leftmouse") {
            parsed.mouseButton = 0;
        } else if (lower == "mouse2" || lower == "rmb" || 
                   lower == "rightmouse") {
            parsed.mouseButton = 1;
        } else if (lower == "mouse3" || lower == "mmb" || 
                   lower == "middlemouse") {
            parsed.mouseButton = 2;
        }
        // Must be a key name (should be last part)
        else {
            int key = KeyNameToImGuiKey(parts[i]);
            if (key != -1) {
                parsed.key = key;
            } else {
                // Invalid key name
                return std::nullopt;
            }
        }
    }
    
    // Validate: must have a key or mouse button
    if (!parsed.IsValid()) {
        return std::nullopt;
    }
    
    // Cache the result
    m_parsedCache[binding] = parsed;
    
    return parsed;
}

bool KeymapManager::IsBindingPressed(const ParsedBinding& parsed) const {
    ImGuiIO& io = ImGui::GetIO();
    
    // Check modifiers
    if (parsed.ctrl && !io.KeyCtrl) return false;
    if (parsed.alt && !io.KeyAlt) return false;
    if (parsed.shift && !io.KeyShift) return false;
    if (parsed.cmd && !io.KeySuper) return false;
    
    // Check if wrong modifiers are pressed
    if (!parsed.ctrl && io.KeyCtrl) return false;
    if (!parsed.alt && io.KeyAlt) return false;
    if (!parsed.shift && io.KeyShift) return false;
    if (!parsed.cmd && io.KeySuper) return false;
    
    // Check mouse button
    if (parsed.mouseButton >= 0) {
        return ImGui::IsMouseClicked(parsed.mouseButton, false);
    }
    
    // Check keyboard key
    if (parsed.key != -1) {
        return ImGui::IsKeyPressed((ImGuiKey)parsed.key, false);
    }
    
    return false;
}

bool KeymapManager::IsActionTriggered(const std::string& action) const {
    std::string binding = GetBinding(action);
    if (binding.empty()) {
        return false;
    }
    
    auto parsed = ParseBinding(binding);
    if (!parsed) {
        return false;
    }
    
    return IsBindingPressed(*parsed);
}

void KeymapManager::LoadBindings(
    const std::unordered_map<std::string, std::string>& bindings
) {
    m_bindings = bindings;
    // Clear cache when loading new bindings
    m_parsedCache.clear();
}

bool KeymapManager::IsBindingValid(const std::string& binding) const {
    if (binding.empty()) {
        return true;  // Empty binding is valid (unbound)
    }
    
    auto parsed = ParseBinding(binding);
    return parsed.has_value() && parsed->IsValid();
}

std::string KeymapManager::FindConflict(const std::string& binding,
                                        const std::string& excludeAction) const {
    if (binding.empty()) {
        return "";  // Empty binding can't conflict
    }
    
    // Normalize binding for comparison (case-insensitive)
    std::string normalizedBinding = ToLower(binding);
    
    for (const auto& [action, boundTo] : m_bindings) {
        if (action == excludeAction) {
            continue;  // Skip the action we're rebinding
        }
        
        if (ToLower(boundTo) == normalizedBinding) {
            return action;  // Found conflict
        }
    }
    
    return "";  // No conflict
}

std::string KeymapManager::GetBindingDisplayName(
    const std::string& binding
) const {
    if (binding.empty()) {
        return "(Not bound)";
    }
    
    auto parsed = ParseBinding(binding);
    if (!parsed || !parsed->IsValid()) {
        return binding;  // Return as-is if can't parse
    }
    
    std::string result;
    
    // Add modifiers in standard order
    if (parsed->ctrl) {
        result += "Ctrl+";
    }
    if (parsed->alt) {
        result += "Alt+";
    }
    if (parsed->shift) {
        result += "Shift+";
    }
    if (parsed->cmd) {
#ifdef __APPLE__
        result += "Cmd+";
#else
        result += "Super+";
#endif
    }
    
    // Add key or mouse button
    if (parsed->mouseButton >= 0) {
        switch (parsed->mouseButton) {
            case 0: result += "Mouse1"; break;
            case 1: result += "Mouse2"; break;
            case 2: result += "Mouse3"; break;
            default: result += "Mouse" + std::to_string(parsed->mouseButton);
        }
    } else if (parsed->key != -1) {
        std::string keyName = ImGuiKeyToDisplayName(parsed->key);
        if (!keyName.empty()) {
            result += keyName;
        } else {
            result += "?";
        }
    }
    
    return result;
}

} // namespace Cartograph

