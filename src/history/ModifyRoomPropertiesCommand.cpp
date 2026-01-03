#include "ModifyRoomPropertiesCommand.h"
#include "Constants.h"

namespace Cartograph {

ModifyRoomPropertiesCommand::ModifyRoomPropertiesCommand(
    const std::string& roomId,
    const RoomPropertiesSnapshot& oldProps,
    const RoomPropertiesSnapshot& newProps
) : m_roomId(roomId), m_oldProps(oldProps), m_newProps(newProps) {}

void ModifyRoomPropertiesCommand::Execute(Model& model) {
    Room* room = model.FindRoom(m_roomId);
    if (room) {
        room->name = m_newProps.name;
        room->color = m_newProps.color;
        room->notes = m_newProps.notes;
        room->tags = m_newProps.tags;
        model.MarkDirty();
    }
}

void ModifyRoomPropertiesCommand::Undo(Model& model) {
    Room* room = model.FindRoom(m_roomId);
    if (room) {
        room->name = m_oldProps.name;
        room->color = m_oldProps.color;
        room->notes = m_oldProps.notes;
        room->tags = m_oldProps.tags;
        model.MarkDirty();
    }
}

std::string ModifyRoomPropertiesCommand::GetDescription() const {
    return "Modify Room: " + m_newProps.name;
}

bool ModifyRoomPropertiesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float /*distanceSq*/
) {
    // Only coalesce within time threshold
    if (timeDelta > PROPERTY_COALESCE_TIME_MS) {
        return false;
    }
    
    auto* otherCmd = dynamic_cast<ModifyRoomPropertiesCommand*>(other);
    if (!otherCmd) {
        return false;
    }
    
    // Only coalesce if same room
    if (m_roomId != otherCmd->m_roomId) {
        return false;
    }
    
    // Keep original oldProps, update to latest newProps
    m_newProps = otherCmd->m_newProps;
    return true;
}

} // namespace Cartograph

