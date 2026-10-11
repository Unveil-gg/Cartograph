#include "ModifyPropsCommand.h"

namespace Cartograph {

ModifyPropsCommand::ModifyPropsCommand(
    Owner owner,
    const std::string& id,
    const Props& oldProps,
    const Props& newProps
) : m_owner(owner), m_id(id), m_old(oldProps), m_new(newProps) {}

Props* ModifyPropsCommand::Find(Model& model) const {
    switch (m_owner) {
        case Owner::Room: {
            Room* r = model.FindRoom(m_id);
            return r ? &r->props : nullptr;
        }
        case Owner::Region: {
            RegionGroup* g = model.FindRegionGroup(m_id);
            return g ? &g->props : nullptr;
        }
        case Owner::Marker: {
            Marker* m = model.FindMarker(m_id);
            return m ? &m->props : nullptr;
        }
    }
    return nullptr;
}

void ModifyPropsCommand::Execute(Model& model) {
    if (Props* p = Find(model)) {
        *p = m_new;
        model.MarkDirty();
    }
}

void ModifyPropsCommand::Undo(Model& model) {
    if (Props* p = Find(model)) {
        *p = m_old;
        model.MarkDirty();
    }
}

std::string ModifyPropsCommand::GetDescription() const {
    return "Edit Properties";
}

}  // namespace Cartograph
