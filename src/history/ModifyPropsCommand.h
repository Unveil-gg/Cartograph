#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to replace the key/value table of a room, region group, or
 * marker. Props edits use this instead of the property snapshots so the
 * older snapshot call sites cannot wipe the table by accident.
 */
class ModifyPropsCommand : public ICommand {
public:
    enum class Owner { Room, Region, Marker };

    ModifyPropsCommand(
        Owner owner,
        const std::string& id,
        const Props& oldProps,
        const Props& newProps
    );

    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;

private:
    Props* Find(Model& model) const;

    Owner m_owner;
    std::string m_id;
    Props m_old;
    Props m_new;
};

}  // namespace Cartograph
