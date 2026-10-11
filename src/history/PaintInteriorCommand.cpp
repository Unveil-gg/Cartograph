#include "PaintInteriorCommand.h"

namespace Cartograph {

PaintInteriorCommand::PaintInteriorCommand(std::vector<Change> changes)
    : m_changes(std::move(changes)) {}

void PaintInteriorCommand::Execute(Model& model) {
    for (const Change& c : m_changes) {
        model.SetInteriorTile(c.ix, c.iy, c.newTile);
    }
}

void PaintInteriorCommand::Undo(Model& model) {
    for (const Change& c : m_changes) {
        model.SetInteriorTile(c.ix, c.iy, c.oldTile);
    }
}

std::string PaintInteriorCommand::GetDescription() const {
    return "Paint Interior";
}

}  // namespace Cartograph
