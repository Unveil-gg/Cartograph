// Gate validation window. Runs ValidateReachability on demand and feeds
// the canvas overlay.

#include "UI.h"
#include "Canvas.h"

#include <imgui.h>

namespace Cartograph {

/**
 * Draw the validation window and keep the canvas overlay in sync.
 * The report is only shown while it matches the model; edits mark it
 * out of date until Run is pressed again.
 * @param model Model to check
 * @param canvas Canvas that draws the overlay
 */
void UI::RenderValidationWindow(Model& model, Canvas& canvas) {
    ValidationState& v = m_validation;
    canvas.validationOverlay = nullptr;
    if (!v.open) return;

    bool stale = !v.hasReport || v.revision != model.revision;
    bool run = false;

    ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Validate Gates", &v.open)) {
        if (ImGui::Button("Run")) run = true;
        ImGui::SameLine();
        ImGui::Checkbox("Show overlay", &v.overlay);
        ImGui::SameLine();
        if (!v.hasReport) {
            ImGui::TextDisabled("Not run yet");
        } else if (stale) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                               "Out of date - run again");
        }

        ImGui::TextDisabled("Ability order (Settings > Game Data):");
        std::string order;
        for (const auto& a : model.abilities) {
            order += (order.empty() ? "" : " > ") + a.name;
        }
        ImGui::TextWrapped("%s", order.empty() ? "(no abilities)"
                                               : order.c_str());
        ImGui::Separator();

        if (v.hasReport) {
            for (const auto& line : v.lines) {
                ImVec4 color(0.6f, 0.9f, 0.6f, 1.0f);
                if (line.rfind("ERROR", 0) == 0) {
                    color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
                } else if (line.rfind("WARN", 0) == 0) {
                    color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextWrapped("%s", line.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            ImGui::TextDisabled(
                "Red rooms: unreachable. Orange: softlock.");
        }
    }
    ImGui::End();

    if (run) {
        v.report = ValidateReachability(model);
        v.lines = DescribeReport(model, v.report);
        v.revision = model.revision;
        v.hasReport = true;
        stale = false;
    }
    if (v.open && v.overlay && v.hasReport && !stale) {
        canvas.validationOverlay = &v.report;
    }
}

}  // namespace Cartograph
