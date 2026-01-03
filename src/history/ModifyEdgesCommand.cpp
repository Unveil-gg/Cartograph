#include "ModifyEdgesCommand.h"
#include "Constants.h"

namespace Cartograph {

ModifyEdgesCommand::ModifyEdgesCommand(
    const std::vector<EdgeChange>& changes
) : m_changes(changes) {}

void ModifyEdgesCommand::Execute(Model& model) {
    for (const auto& change : m_changes) {
        model.SetEdgeState(change.edgeId, change.newState);
    }
}

void ModifyEdgesCommand::Undo(Model& model) {
    for (const auto& change : m_changes) {
        model.SetEdgeState(change.edgeId, change.oldState);
    }
}

std::string ModifyEdgesCommand::GetDescription() const {
    return "Modify Edges";
}

bool ModifyEdgesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float distanceSq
) {
    // Only coalesce if within time threshold
    if (timeDelta > COALESCE_TIME_MS) {
        return false;
    }
    
    ModifyEdgesCommand* otherEdge = dynamic_cast<ModifyEdgesCommand*>(other);
    if (!otherEdge) {
        return false;
    }
    
    // Merge the changes (append new changes)
    m_changes.insert(
        m_changes.end(),
        otherEdge->m_changes.begin(),
        otherEdge->m_changes.end()
    );
    
    return true;
}

} // namespace Cartograph

