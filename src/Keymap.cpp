#include "Keymap.h"
#include <imgui.h>

namespace Cartograph {

KeymapManager::KeymapManager() {
}

void KeymapManager::SetBinding(const std::string& action, const std::string& binding) {
    m_bindings[action] = binding;
}

std::string KeymapManager::GetBinding(const std::string& action) const {
    auto it = m_bindings.find(action);
    return (it != m_bindings.end()) ? it->second : "";
}

bool KeymapManager::IsActionTriggered(const std::string& action) const {
    // TODO: Implement input state checking
    // For now, return false
    return false;
}

void KeymapManager::LoadBindings(
    const std::unordered_map<std::string, std::string>& bindings
) {
    m_bindings = bindings;
}

} // namespace Cartograph

