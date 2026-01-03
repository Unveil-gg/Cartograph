#pragma once

#include "ICommand.h"
#include "RoomPropertiesSnapshot.h"

namespace Cartograph {

/**
 * Command to modify room properties (name, color, notes, tags).
 * Supports coalescing for rapid edits like typing.
 */
class ModifyRoomPropertiesCommand : public ICommand {
public:
    ModifyRoomPropertiesCommand(
        const std::string& roomId,
        const RoomPropertiesSnapshot& oldProps,
        const RoomPropertiesSnapshot& newProps
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::string m_roomId;
    RoomPropertiesSnapshot m_oldProps;
    RoomPropertiesSnapshot m_newProps;
};

} // namespace Cartograph

