#include "MoveMarkersCommand.h"

namespace Cartograph {

MoveMarkersCommand::MoveMarkersCommand(const std::vector<MarkerMove>& moves)
    : m_moves(moves) {}

MoveMarkersCommand::MoveMarkersCommand(
    const std::string& markerId,
    float oldX, float oldY,
    float newX, float newY
) {
    m_moves.push_back({markerId, oldX, oldY, newX, newY});
}

void MoveMarkersCommand::Execute(Model& model) {
    for (const auto& move : m_moves) {
        Marker* marker = model.FindMarker(move.markerId);
        if (marker) {
            marker->x = move.newX;
            marker->y = move.newY;
        }
    }
    model.MarkDirty();
}

void MoveMarkersCommand::Undo(Model& model) {
    for (const auto& move : m_moves) {
        Marker* marker = model.FindMarker(move.markerId);
        if (marker) {
            marker->x = move.oldX;
            marker->y = move.oldY;
        }
    }
    model.MarkDirty();
}

std::string MoveMarkersCommand::GetDescription() const {
    if (m_moves.size() == 1) {
        return "Move Marker";
    } else {
        return "Move " + std::to_string(m_moves.size()) + " Markers";
    }
}

} // namespace Cartograph

