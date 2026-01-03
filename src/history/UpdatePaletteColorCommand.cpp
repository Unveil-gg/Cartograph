#include "UpdatePaletteColorCommand.h"

namespace Cartograph {

UpdatePaletteColorCommand::UpdatePaletteColorCommand(
    int tileId,
    const std::string& newName,
    const Color& newColor
) : m_tileId(tileId), 
    m_newName(newName), 
    m_newColor(newColor),
    m_oldName(""),
    m_oldColor() {}

void UpdatePaletteColorCommand::Execute(Model& model) {
    // Save old state on first execution
    if (m_oldName.empty()) {
        const TileType* tile = model.FindPaletteEntry(m_tileId);
        if (tile) {
            m_oldName = tile->name;
            m_oldColor = tile->color;
        }
    }
    
    // Apply new values
    model.UpdatePaletteColor(m_tileId, m_newName, m_newColor);
}

void UpdatePaletteColorCommand::Undo(Model& model) {
    // Restore old values
    model.UpdatePaletteColor(m_tileId, m_oldName, m_oldColor);
}

std::string UpdatePaletteColorCommand::GetDescription() const {
    return "Update Color: " + m_newName;
}

} // namespace Cartograph

