#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../Canvas.h"
#include "../Config.h"
#include "../Preferences.h"
#include "../Package.h"
#include "../ProjectFolder.h"
#include "../Theme/Themes.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>


namespace Cartograph {

void Modals::RenderGridSettings(
    Model& model
) {
                settingsModalSelectedTab = 1;
                ImGui::Spacing();
                ImGui::Spacing();
                
                // Grid Cell Type
                ImGui::Text("Grid Cell Type");
                ImGui::Separator();
                ImGui::Spacing();
                
                bool canChangePreset = model.CanChangeGridPreset();
                
                if (!canChangePreset) {
                    ImGui::BeginDisabled();
                }
                
                bool isSquare = (model.grid.preset == GridPreset::Square);
                if (ImGui::RadioButton("Square (16×16)", isSquare)) {
                    if (canChangePreset) {
                        model.ApplyGridPreset(GridPreset::Square);
                    } else {
                        m_ui.ShowToast(
                            "Cannot change cell type - delete all markers first",
                            Toast::Type::Warning, 3.0f);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Square cells for top-down games. "
                        "Markers snap to center only.");
                }
                
                bool isRectangle = 
                    (model.grid.preset == GridPreset::Rectangle);
                if (ImGui::RadioButton("Rectangle (32×16)", isRectangle)) {
                    if (canChangePreset) {
                        model.ApplyGridPreset(GridPreset::Rectangle);
                    } else {
                        m_ui.ShowToast(
                            "Cannot change cell type - delete all markers first",
                            Toast::Type::Warning, 3.0f);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Rectangular cells for side-scrollers. "
                        "Markers snap to left/right positions.");
                }
                
                if (!canChangePreset) {
                    ImGui::EndDisabled();
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f),
                        "🔒 Locked (%zu marker%s placed)",
                        model.markers.size(),
                        model.markers.size() == 1 ? "" : "s");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            "Delete all markers to change cell type");
                    }
                }
                
                // Show current dimensions (read-only info)
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                    "Cell Dimensions: %d×%d px",
                    model.grid.tileWidth, model.grid.tileHeight);
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // Edge Configuration
                ImGui::Text("Edge/Wall Configuration");
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Checkbox("Auto-expand grid", 
                    &model.grid.autoExpandGrid);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Automatically expand grid when placing edges "
                        "near boundaries");
                }
                
                ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderInt("Expansion threshold (cells)", 
                                &model.grid.expansionThreshold, 1, 20);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Distance from grid boundary to trigger expansion");
                }
                
                ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Expansion factor", 
                    &model.grid.expansionFactor, 
                    1.1f, 3.0f, "%.1fx");
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Grid growth multiplier "
                        "(e.g., 1.5x = 50%% growth)");
                }
                
                ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Edge hover threshold", 
                                  &model.grid.edgeHoverThreshold, 
                                  0.1f, 0.5f, "%.2f");
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Distance from cell edge to activate edge mode "
                        "(0.2 = 20%% of cell size)");
                }
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
}

}  // namespace Cartograph
