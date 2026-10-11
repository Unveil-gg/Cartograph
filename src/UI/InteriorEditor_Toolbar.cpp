// Interior editor: toolbar and hotkeys.

#include "InteriorEditor.h"
#include "../InteriorColors.h"

namespace Cartograph {

/**
 * Tool, tile picker and view buttons.
 * @param model Project model
 * @param history Undo history
 */
void InteriorEditor::RenderToolbar(Model& model, History& history) {
    struct ToolDef { Tool tool; const char* label; const char* tip; };
    const ToolDef tools[] = {
        {Tool::Paint, "Paint", "Paint tiles (P)"},
        {Tool::Erase, "Erase", "Erase tiles (E)"},
        {Tool::Fill, "Fill", "Fill a connected area (F)"},
    };
    for (const ToolDef& t : tools) {
        bool active = m_tool == t.tool;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(t.label)) m_tool = t.tool;
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.tip);
        ImGui::SameLine();
    }
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    for (int i = 1; i < kInteriorTileKinds; ++i) {
        InteriorTile t = static_cast<InteriorTile>(i);
        ImGui::PushID(i);
        ImVec4 col = InteriorTileColor(t).ToImVec4();
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoTooltip |
                                    ImGuiColorEditFlags_NoAlpha |
                                    ImGuiColorEditFlags_NoPicker;
        if (ImGui::ColorButton("##tile", col, flags, ImVec2(22, 22))) {
            m_tile = t;
            if (m_tool == Tool::Erase) m_tool = Tool::Paint;
        }
        if (m_tile == t) {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s (%d)", InteriorTileName(t), i);
        }
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::BeginDisabled(!history.CanUndo());
    if (ImGui::Button("Undo")) history.Undo(model);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!history.CanRedo());
    if (ImGui::Button("Redo")) history.Redo(model);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Fit")) m_fit = true;
    ImGui::SameLine();
    if (ImGui::Button("Close")) open = false;
}

/**
 * Hotkeys while the window is focused: P/E/F tools, 1-6 tiles, Home fits,
 * Escape closes.
 * @param model Project model
 * @param history Undo history
 */
void InteriorEditor::HandleKeys(Model& model, History& history) {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
        ImGui::GetIO().WantTextInput) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) open = false;
    if (ImGui::IsKeyPressed(ImGuiKey_P)) m_tool = Tool::Paint;
    if (ImGui::IsKeyPressed(ImGuiKey_E)) m_tool = Tool::Erase;
    if (ImGui::IsKeyPressed(ImGuiKey_F)) m_tool = Tool::Fill;
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) m_fit = true;
    for (int i = 1; i < kInteriorTileKinds; ++i) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + i))) {
            m_tile = static_cast<InteriorTile>(i);
            if (m_tool == Tool::Erase) m_tool = Tool::Paint;
        }
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl || io.KeySuper) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (io.KeyShift) history.Redo(model);
            else history.Undo(model);
        }
    }
}

}  // namespace Cartograph
