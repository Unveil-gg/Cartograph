#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to change a door edge's gate, one-way, spawns, and props.
 * Default-valued props remove the entry.
 */
class ModifyDoorPropsCommand : public ICommand {
public:
    ModifyDoorPropsCommand(
        const EdgeId& edgeId,
        const DoorProps& oldProps,
        const DoorProps& newProps
    );

    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;

private:
    EdgeId m_edgeId;
    DoorProps m_old;
    DoorProps m_new;
};

}  // namespace Cartograph
