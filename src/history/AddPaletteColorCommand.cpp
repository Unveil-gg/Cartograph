#include "AddPaletteColorCommand.h"

namespace Cartograph {

AddPaletteColorCommand::AddPaletteColorCommand(
    const std::string& name, 
    const Color& color
) : m_name(name), m_color(color), m_tileId(-1) {}

void AddPaletteColorCommand::Execute(Model& model) {
    if (m_tileId == -1) {
        // First execution - add color and capture ID
        m_tileId = model.AddPaletteColor(m_name, m_color);
    } else {
        // Redo - restore with same ID
        model.palette.push_back({m_tileId, m_name, m_color});
        model.MarkDirty();
    }
}

void AddPaletteColorCommand::Undo(Model& model) {
    // Remove the color we added
    model.RemovePaletteColor(m_tileId);
}

std::string AddPaletteColorCommand::GetDescription() const {
    return "Add Color: " + m_name;
}

} // namespace Cartograph

