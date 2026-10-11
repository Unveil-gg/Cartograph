// Key/value property sections for rooms and regions.

#include "UI.h"
#include "History.h"
#include "UI/PropsTable.h"

#include <imgui.h>

namespace Cartograph {

/**
 * Draw the key/value table for a room. Edits become undo commands.
 * @param model Model being edited
 * @param history Undo history
 * @param room Selected room (non-null)
 */
void UI::RenderRoomProps(Model& model, History& history, Room* room) {
    static PropsEditor editor;
    ImGui::Spacing();
    ImGui::Text("Properties:");
    if (editor.Render(room->props, kRoomPropKeys, "roomProps")) {
        model.MarkDirty();
        history.AddCommand(
            std::make_unique<ModifyPropsCommand>(
                ModifyPropsCommand::Owner::Room, room->id, editor.before,
                room->props),
            model, false);
    }
}

/**
 * Draw the key/value table for a region group.
 * @param model Model being edited
 * @param history Undo history
 * @param region Selected region group (non-null)
 */
void UI::RenderRegionPropsTable(
    Model& model, History& history, RegionGroup* region
) {
    static PropsEditor editor;
    ImGui::Spacing();
    ImGui::Text("Properties:");
    if (editor.Render(region->props, kRegionPropKeys, "regionProps")) {
        model.MarkDirty();
        history.AddCommand(
            std::make_unique<ModifyPropsCommand>(
                ModifyPropsCommand::Owner::Region, region->id,
                editor.before, region->props),
            model, false);
    }
}

}  // namespace Cartograph
