// Marker kind picker and per-kind fields in the Marker tool panel.

#include "UI.h"
#include "History.h"
#include "MarkerKinds.h"
#include "UI/PropsTable.h"

#include <imgui.h>
#include <cstring>

namespace Cartograph {

namespace {

// No key suggestions for free-form marker fields.
const std::vector<const char*> kNoSuggestions;

}  // namespace

/**
 * Kind combo plus the selected marker's fields and extra props.
 * Changing the kind of a selected marker is undoable; with no marker
 * selected it sets the kind (and default look) for new markers.
 * @param model Model being edited
 * @param history Undo history
 */
void UI::RenderMarkerKindFields(Model& model, History& history) {
    CanvasPanel& panel = m_canvasPanel;
    Marker* marker = model.FindMarker(panel.selectedMarkerId);

    const MarkerKindInfo* info = FindMarkerKind(panel.selectedMarkerKind);
    std::string preview = info ? info->label : panel.selectedMarkerKind;
    if (ImGui::BeginCombo("Kind", preview.c_str())) {
        for (const auto& kind : MarkerKinds()) {
            bool selected = panel.selectedMarkerKind == kind.id;
            if (!ImGui::Selectable(kind.label, selected) || selected) {
                continue;
            }
            panel.selectedMarkerKind = kind.id;
            if (marker) {
                std::vector<SetMarkerKindCommand::Change> changes;
                changes.push_back({marker->id, marker->kind, kind.id});
                if (std::string(kind.id) == kKindStart) {
                    for (const auto& other : model.markers) {
                        if (other.id != marker->id &&
                            other.kind == kKindStart) {
                            changes.push_back(
                                {other.id, kKindStart, kKindCustom});
                        }
                    }
                }
                history.AddCommand(
                    std::make_unique<SetMarkerKindCommand>(changes), model);
            } else if (kind.icon[0] != '\0') {
                // New markers of this kind start with its default look
                panel.selectedIconName = kind.icon;
                panel.markerColor = kind.color;
                std::string hex = kind.color.ToHex(false);
                std::strncpy(panel.markerColorHex, hex.c_str(),
                             sizeof(panel.markerColorHex) - 1);
                panel.markerColorHex[sizeof(panel.markerColorHex) - 1] = '\0';
            }
        }
        ImGui::EndCombo();
    }
    if (!marker) return;

    // Per-kind fields, stored in marker->props
    static PropsEditor editor;
    std::vector<std::string> hidden;
    if (info) {
        for (const auto& field : info->fields) {
            hidden.push_back(field.key);
            Props before = marker->props;
            Props& props = marker->props;
            if (field.type == MarkerField::Type::Ability) {
                std::string current = props.count(field.key)
                                          ? props[field.key] : "";
                std::string shown = current.empty()
                    ? "(none)" : model.AbilityName(current);
                if (ImGui::BeginCombo(field.label, shown.c_str())) {
                    for (const auto& ability : model.abilities) {
                        if (!ImGui::Selectable(ability.name.c_str(),
                                               ability.id == current)) {
                            continue;
                        }
                        props[field.key] = ability.id;
                        history.AddCommand(
                            std::make_unique<ModifyPropsCommand>(
                                ModifyPropsCommand::Owner::Marker,
                                marker->id, before, props),
                            model, false);
                        model.MarkDirty();
                    }
                    ImGui::EndCombo();
                }
            } else {
                char buf[256];
                std::strncpy(buf, props.count(field.key)
                                      ? props[field.key].c_str() : "",
                             sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText(field.label, buf, sizeof(buf))) {
                    if (buf[0] == '\0') props.erase(field.key);
                    else props[field.key] = buf;
                    model.MarkDirty();
                }
                if (ImGui::IsItemActivated()) editor.before = before;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    history.AddCommand(
                        std::make_unique<ModifyPropsCommand>(
                            ModifyPropsCommand::Owner::Marker, marker->id,
                            editor.before, props),
                        model, false);
                }
            }
        }
    }

    // Any other key/value pairs
    ImGui::Text("Other fields:");
    if (editor.Render(marker->props, kNoSuggestions, "markerProps",
                      hidden)) {
        history.AddCommand(
            std::make_unique<ModifyPropsCommand>(
                ModifyPropsCommand::Owner::Marker, marker->id,
                editor.before, marker->props),
            model, false);
        model.MarkDirty();
    }
}

}  // namespace Cartograph
