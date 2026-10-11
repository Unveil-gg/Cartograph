#include "SetMarkerKindCommand.h"

namespace Cartograph {

SetMarkerKindCommand::SetMarkerKindCommand(const std::vector<Change>& changes)
    : m_changes(changes) {}

void SetMarkerKindCommand::Execute(Model& model) {
    for (const auto& c : m_changes) {
        if (Marker* m = model.FindMarker(c.markerId)) m->kind = c.newKind;
    }
    model.MarkDirty();
}

void SetMarkerKindCommand::Undo(Model& model) {
    for (const auto& c : m_changes) {
        if (Marker* m = model.FindMarker(c.markerId)) m->kind = c.oldKind;
    }
    model.MarkDirty();
}

std::string SetMarkerKindCommand::GetDescription() const {
    return "Change Marker Kind";
}

}  // namespace Cartograph
