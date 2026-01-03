#include "FillTilesCommand.h"

namespace Cartograph {

FillTilesCommand::FillTilesCommand(const std::vector<TileChange>& changes)
    : m_changes(changes) {}

void FillTilesCommand::Execute(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.newTileId);
    }
}

void FillTilesCommand::Undo(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.oldTileId);
    }
}

std::string FillTilesCommand::GetDescription() const {
    return "Fill Tiles";
}

} // namespace Cartograph

