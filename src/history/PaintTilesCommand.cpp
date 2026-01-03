#include "PaintTilesCommand.h"
#include "Constants.h"

namespace Cartograph {

PaintTilesCommand::PaintTilesCommand(const std::vector<TileChange>& changes)
    : m_changes(changes) {}

void PaintTilesCommand::Execute(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.newTileId);
    }
}

void PaintTilesCommand::Undo(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.oldTileId);
    }
}

std::string PaintTilesCommand::GetDescription() const {
    return "Paint Tiles";
}

bool PaintTilesCommand::TryCoalesce(
    ICommand* other, 
    uint64_t timeDelta, 
    float distanceSq
) {
    // Only coalesce if within time/distance thresholds
    if (timeDelta > COALESCE_TIME_MS) {
        return false;
    }
    
    PaintTilesCommand* otherPaint = dynamic_cast<PaintTilesCommand*>(other);
    if (!otherPaint) {
        return false;
    }
    
    // Merge the changes
    // (Simple implementation - just append, more sophisticated could dedupe)
    m_changes.insert(
        m_changes.end(),
        otherPaint->m_changes.begin(),
        otherPaint->m_changes.end()
    );
    
    return true;
}

} // namespace Cartograph

