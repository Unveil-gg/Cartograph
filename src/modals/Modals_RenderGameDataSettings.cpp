#include "Modals.h"
#include "../Config.h"
#include "../Model.h"

#include <imgui.h>
#include <cctype>
#include <cstring>

namespace Cartograph {

namespace {

/**
 * Turn a display name into an id: lowercase, spaces to dashes, only
 * [a-z0-9-_] kept.
 * @param name Display name
 * @return Slug, "ability" when nothing usable remains
 */
std::string Slugify(const std::string& name) {
    std::string out;
    for (char c : name) {
        unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u)) {
            out += static_cast<char>(std::tolower(u));
        } else if ((c == ' ' || c == '-' || c == '_') && !out.empty() &&
                   out.back() != '-') {
            out += '-';
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "ability" : out;
}

/**
 * Add a new ability with a unique id.
 * @param model Model to modify
 */
void AddAbility(Model& model) {
    Ability a;
    a.name = "Ability " + std::to_string(model.abilities.size() + 1);
    std::string base = Slugify(a.name);
    a.id = base;
    for (int n = 2; model.FindAbility(a.id); ++n) {
        a.id = base + "-" + std::to_string(n);
    }
    model.abilities.push_back(a);
    model.MarkDirty();
}

}  // namespace

/**
 * Game Data tab: ordered ability list and interior sub-grid size.
 * @param model Model to edit (settings edits are not undoable)
 */
void Modals::RenderGameDataSettings(Model& model) {
    settingsModalSelectedTab = 4;
    ImGui::Spacing();
    ImGui::Text("Abilities");
    ImGui::Separator();
    ImGui::TextDisabled(
        "Order is the intended acquisition order. Doors and "
        "ability pickups choose from this list.");
    ImGui::Spacing();

    int remove = -1;
    int moveUp = -1;
    for (size_t i = 0; i < model.abilities.size(); ++i) {
        Ability& a = model.abilities[i];
        ImGui::PushID(static_cast<int>(i));

        float rgb[3] = {a.color.r, a.color.g, a.color.b};
        if (ImGui::ColorEdit3("##color", rgb,
                              ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_NoLabel)) {
            a.color = Color(rgb[0], rgb[1], rgb[2], 1.0f);
            model.MarkDirty();
        }
        ImGui::SameLine();

        char buf[128];
        std::strncpy(buf, a.name.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputText("##name", buf, sizeof(buf))) {
            a.name = buf;
            model.MarkDirty();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("id: %s", a.id.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Up") && i > 0) moveUp = static_cast<int>(i);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) remove = static_cast<int>(i);
        ImGui::PopID();
    }
    if (moveUp > 0) {
        std::swap(model.abilities[moveUp], model.abilities[moveUp - 1]);
        model.MarkDirty();
    }
    if (remove >= 0) {
        model.abilities.erase(model.abilities.begin() + remove);
        model.MarkDirty();
    }
    if (model.abilities.size() < Limits::MAX_ABILITIES &&
        ImGui::Button("+ Add Ability")) {
        AddAbility(model);
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Room Interior Grid");
    ImGui::Separator();
    bool locked = !model.InteriorEmpty();
    if (locked) ImGui::BeginDisabled();
    int size[2] = {model.interiorConfig.width, model.interiorConfig.height};
    if (ImGui::InputInt2("Tiles per cell (W x H)", size)) {
        model.SetInteriorSize(size[0], size[1]);
    }
    if (locked) ImGui::EndDisabled();
    ImGui::TextDisabled(locked
        ? "Locked: clear all interior tiles to change the size."
        : "Applies to every cell. Locks once interior tiles are painted.");

    ImGui::EndTabItem();
}

}  // namespace Cartograph
