#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to create a new room.
 * Supports undo by removing the created room.
 */
class CreateRoomCommand : public ICommand {
public:
    explicit CreateRoomCommand(const Room& room);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
    // Get the created room's ID (for UI to select it after creation)
    const std::string& GetRoomId() const { return m_room.id; }
    
private:
    Room m_room;
};

} // namespace Cartograph

