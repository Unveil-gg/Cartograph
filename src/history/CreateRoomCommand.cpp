#include "CreateRoomCommand.h"
#include <algorithm>

namespace Cartograph {

CreateRoomCommand::CreateRoomCommand(const Room& room)
    : m_room(room) {}

void CreateRoomCommand::Execute(Model& model) {
    // Add room to model
    model.rooms.push_back(m_room);
    model.MarkDirty();
}

void CreateRoomCommand::Undo(Model& model) {
    // Remove the room we created
    auto it = std::find_if(
        model.rooms.begin(),
        model.rooms.end(),
        [this](const Room& r) { return r.id == m_room.id; }
    );
    if (it != model.rooms.end()) {
        model.rooms.erase(it);
    }
    model.MarkDirty();
}

std::string CreateRoomCommand::GetDescription() const {
    return "Create Room: " + m_room.name;
}

} // namespace Cartograph

