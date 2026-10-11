#include "PropsTable.h"

#include "../Config.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace Cartograph {

const std::vector<const char*> kRoomPropKeys = {"kind", "music", "dark"};
const std::vector<const char*> kRegionPropKeys = {
    "palette", "music", "ambience", "parallaxLayers"};
const std::vector<const char*> kDoorPropKeys = {"locked", "sound"};

bool PropsEditor::Render(
    Props& props,
    const std::vector<const char*>& suggestions,
    const char* id,
    const std::vector<std::string>& hidden
) {
    bool committed = false;
    const Props snapshot = props;  // Table at the start of this frame
    ImGui::PushID(id);

    std::string removeKey;
    for (auto& [key, value] : props) {
        if (std::find(hidden.begin(), hidden.end(), key) != hidden.end()) {
            continue;
        }
        ImGui::PushID(key.c_str());
        char buf[512];
        std::strncpy(buf, value.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ImGui::SetNextItemWidth(-60.0f);
        if (ImGui::InputText(key.c_str(), buf, sizeof(buf))) {
            value = buf;
        }
        if (ImGui::IsItemActivated()) before = snapshot;
        if (ImGui::IsItemDeactivatedAfterEdit()) committed = true;
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) removeKey = key;
        ImGui::PopID();
    }
    if (!removeKey.empty()) {
        before = props;
        props.erase(removeKey);
        committed = true;
    }

    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputTextWithHint("##key", "key", newKey, sizeof(newKey));
    ImGui::SameLine();
    if (ImGui::SmallButton("v")) ImGui::OpenPopup("##suggest");
    if (ImGui::BeginPopup("##suggest")) {
        for (const char* s : suggestions) {
            if (props.count(s)) continue;
            if (ImGui::Selectable(s)) {
                std::strncpy(newKey, s, sizeof(newKey) - 1);
                newKey[sizeof(newKey) - 1] = '\0';
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-50.0f);
    ImGui::InputTextWithHint("##val", "value", newVal, sizeof(newVal));
    ImGui::SameLine();
    bool canAdd = newKey[0] != '\0' && !props.count(newKey) &&
                  props.size() < Limits::MAX_PROPS;
    if (!canAdd) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Add")) {
        before = props;
        props[newKey] = newVal;
        newKey[0] = '\0';
        newVal[0] = '\0';
        committed = true;
    }
    if (!canAdd) ImGui::EndDisabled();

    ImGui::PopID();
    return committed;
}

}  // namespace Cartograph
