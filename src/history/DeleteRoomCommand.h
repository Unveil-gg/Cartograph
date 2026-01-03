#pragma once

#include "ICommand.h"
#include <vector>
#include <utility>

namespace Cartograph {

/**
 * Command to delete a room.
 * Saves room data and cell assignments for undo.
 */
class DeleteRoomCommand : public ICommand {
public:
    explicit DeleteRoomCommand(const std::string& roomId);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::string m_roomId;
    Room m_savedRoom;  // Full room data for undo
    std::vector<std::pair<int, int>> m_savedCellAssignments;  // Cells assigned
};

} // namespace Cartograph

