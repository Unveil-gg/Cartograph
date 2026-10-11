#include "ModifyDoorPropsCommand.h"

namespace Cartograph {

ModifyDoorPropsCommand::ModifyDoorPropsCommand(
    const EdgeId& edgeId,
    const DoorProps& oldProps,
    const DoorProps& newProps
) : m_edgeId(edgeId), m_old(oldProps), m_new(newProps) {}

void ModifyDoorPropsCommand::Execute(Model& model) {
    model.SetDoorProps(m_edgeId, m_new);
}

void ModifyDoorPropsCommand::Undo(Model& model) {
    model.SetDoorProps(m_edgeId, m_old);
}

std::string ModifyDoorPropsCommand::GetDescription() const {
    return "Edit Door";
}

}  // namespace Cartograph
