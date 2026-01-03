#include "ModifyRoomAssignmentsCommand.h"

namespace Cartograph {

ModifyRoomAssignmentsCommand::ModifyRoomAssignmentsCommand(
    const std::vector<CellAssignment>& assignments
) : m_assignments(assignments) {}

void ModifyRoomAssignmentsCommand::Execute(Model& model) {
    for (const auto& assignment : m_assignments) {
        model.SetCellRoom(assignment.x, assignment.y, assignment.newRoomId);
    }
}

void ModifyRoomAssignmentsCommand::Undo(Model& model) {
    for (const auto& assignment : m_assignments) {
        model.SetCellRoom(assignment.x, assignment.y, assignment.oldRoomId);
    }
}

std::string ModifyRoomAssignmentsCommand::GetDescription() const {
    return "Assign Room Cells";
}

} // namespace Cartograph

