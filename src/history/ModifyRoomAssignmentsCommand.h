#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to assign/unassign cells to rooms.
 * Used for room painting mode.
 */
class ModifyRoomAssignmentsCommand : public ICommand {
public:
    struct CellAssignment {
        int x, y;
        std::string oldRoomId;  // Empty if no previous assignment
        std::string newRoomId;  // Empty to unassign
    };
    
    explicit ModifyRoomAssignmentsCommand(
        const std::vector<CellAssignment>& assignments
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<CellAssignment> m_assignments;
};

} // namespace Cartograph

