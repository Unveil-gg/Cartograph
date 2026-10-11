// Inspector for the door edge selected with the Select tool.

#include "UI.h"
#include "History.h"
#include "UI/PropsTable.h"

#include <imgui.h>
#include <cstdio>

namespace Cartograph {

namespace {

// Room name for a cell, or "(no room)".
std::string RoomLabel(const Model& model, int x, int y) {
    const Room* room = model.FindRoom(model.GetCellRoom(x, y));
    return room ? room->name : "(no room)";
}

}  // namespace

/**
 * Draw the door inspector: gate, one-way direction, spawn points, props.
 * Every change is one undoable ModifyDoorPropsCommand.
 * @param model Model being edited
 * @param history Undo history
 */
void UI::RenderDoorInspector(Model& model, History& history) {
    CanvasPanel& panel = m_canvasPanel;
    const EdgeId edge = panel.selectedDoorEdge;
    if (model.GetEdgeState(edge) != EdgeState::Door) {
        panel.hasSelectedDoor = false;
        panel.placingSpawn = 0;
        return;
    }

    const DoorProps* stored = model.GetDoorProps(edge);
    const DoorProps current = stored ? *stored : DoorProps();
    auto commit = [&](const DoorProps& next) {
        history.AddCommand(
            std::make_unique<ModifyDoorPropsCommand>(edge, current, next),
            model, true);
    };

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "DOOR");
    ImGui::Separator();
    std::string roomA = RoomLabel(model, edge.x1, edge.y1);
    std::string roomB = RoomLabel(model, edge.x2, edge.y2);
    ImGui::TextWrapped("A: %s (%d, %d)", roomA.c_str(), edge.x1, edge.y1);
    ImGui::TextWrapped("B: %s (%d, %d)", roomB.c_str(), edge.x2, edge.y2);
    ImGui::Spacing();

    // Gate
    std::string gateLabel = current.gate.empty()
        ? "(none)"
        : (model.FindAbility(current.gate)
               ? model.AbilityName(current.gate)
               : current.gate + " (unknown)");
    if (ImGui::BeginCombo("Gate", gateLabel.c_str())) {
        if (ImGui::Selectable("(none)", current.gate.empty()) &&
            !current.gate.empty()) {
            DoorProps next = current;
            next.gate.clear();
            commit(next);
        }
        for (const auto& ability : model.abilities) {
            bool selected = ability.id == current.gate;
            std::string name = ability.name.empty() ? ability.id
                                                     : ability.name;
            if (ImGui::Selectable(name.c_str(), selected) && !selected) {
                DoorProps next = current;
                next.gate = ability.id;
                commit(next);
            }
        }
        ImGui::EndCombo();
    }
    if (model.abilities.empty()) {
        ImGui::TextDisabled("Add abilities in Settings > Game Data");
    }

    // One-way
    static const char* kOneWayNames[] = {"Both ways", "A to B", "B to A"};
    int oneWay = static_cast<int>(current.oneWay);
    if (ImGui::Combo("One-way", &oneWay, kOneWayNames, 3) &&
        oneWay != static_cast<int>(current.oneWay)) {
        DoorProps next = current;
        next.oneWay = static_cast<OneWay>(oneWay);
        commit(next);
    }

    // Spawn points
    ImGui::Spacing();
    ImGui::Text("Spawn points (arrival)");
    const std::optional<Spawn>* spawns[2] = {&current.spawnA,
                                             &current.spawnB};
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i);
        const char* name = i == 0 ? "A" : "B";
        if (spawns[i]->has_value()) {
            ImGui::Text("%s: (%.2f, %.2f)", name, (*spawns[i])->x,
                        (*spawns[i])->y);
        } else {
            ImGui::TextDisabled("%s: not set", name);
        }
        ImGui::SameLine();
        bool placing = panel.placingSpawn == i + 1;
        if (ImGui::SmallButton(placing ? "Click map..." : "Place")) {
            panel.placingSpawn = placing ? 0 : i + 1;
        }
        if (spawns[i]->has_value()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) {
                DoorProps next = current;
                (i == 0 ? next.spawnA : next.spawnB).reset();
                commit(next);
            }
        }
        ImGui::PopID();
    }

    // Props table edits the stored entry directly (typing is live).
    ImGui::Spacing();
    ImGui::Text("Properties:");
    static PropsEditor editor;
    DoorProps& live = model.doorProps[edge];
    if (editor.Render(live.props, kDoorPropKeys, "doorProps")) {
        DoorProps before = live;
        before.props = editor.before;
        model.MarkDirty();
        history.AddCommand(
            std::make_unique<ModifyDoorPropsCommand>(edge, before, live),
            model, false);
    }
    if (live.IsDefault()) model.doorProps.erase(edge);
}

}  // namespace Cartograph
