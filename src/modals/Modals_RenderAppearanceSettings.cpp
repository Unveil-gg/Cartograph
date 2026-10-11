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

void Modals::RenderAppearanceSettings(
    App& app,
    Model& model
) {
                settingsModalSelectedTab = 3;
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::Text("Theme");
                ImGui::Separator();
                ImGui::Spacing();
                
                // Theme dropdown - use theme registry
                auto themes = GetAvailableThemes();
                int currentTheme = 0;
                for (size_t i = 0; i < themes.size(); ++i) {
                    if (model.theme.name == themes[i]) {
                        currentTheme = static_cast<int>(i);
                        break;
                    }
                }
                
                ImGui::Text("Color Theme:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                
                // Build combo items string
                std::string comboItems;
                for (const auto& t : themes) {
                    comboItems += t + '\0';
                }
                comboItems += '\0';
                
                if (ImGui::Combo("##ThemeCombo", &currentTheme, 
                                comboItems.c_str())) {
                    std::string newThemeName = themes[currentTheme];
                    if (newThemeName != model.theme.name) {
                        model.InitDefaultTheme(newThemeName);
                        app.ApplyTheme(model.theme);
                        // Save to global preferences (not project)
                        Preferences::themeName = newThemeName;
                        Preferences::Save();
                        m_ui.ShowToast(
                            "Theme changed to " + newThemeName,
                            Toast::Type::Success
                        );
                    }
                }
                
                // Show description for selected theme
                ImGui::Spacing();
                std::string desc = GetThemeDescription(model.theme.name);
                if (!desc.empty()) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "%s", desc.c_str());
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // UI Scale
                ImGui::Text("Display");
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Text("UI Scale:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                float uiScale = model.theme.uiScale;
                if (ImGui::SliderFloat("##UIScale", &uiScale, 0.8f, 1.5f, 
                                       "%.1fx")) {
                    model.theme.uiScale = uiScale;
                    app.ApplyTheme(model.theme);
                    // Save to global preferences (not project)
                    Preferences::uiScale = uiScale;
                    Preferences::Save();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset")) {
                    model.theme.uiScale = 1.0f;
                    app.ApplyTheme(model.theme);
                    Preferences::uiScale = 1.0f;
                    Preferences::Save();
                }
                
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Adjust UI element sizes for different display densities");
                
                ImGui::Spacing();
                ImGui::EndTabItem();
}

}  // namespace Cartograph
