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

void Modals::RenderProjectSettings(
    App& app,
    Model& model
) {
                settingsModalSelectedTab = 0;
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::Text("Project Information");
                ImGui::Separator();
                ImGui::Spacing();
                
                // Project metadata
                char titleBuf[256];
                strncpy(titleBuf, model.meta.title.c_str(), 
                    sizeof(titleBuf) - 1);
                titleBuf[sizeof(titleBuf) - 1] = '\0';
                if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
                    model.meta.title = titleBuf;
                    model.MarkDirty();
                }
                
                // Show "Rename Folder" button if title differs from folder 
                const std::string& currentPath = app.GetCurrentFilePath();
                // Check if current project is a folder (not a .cart file)
                bool isProjectFolder = !currentPath.empty() &&
                    !Package::HasCartExtension(currentPath);
                
                if (isProjectFolder && !settingsOriginalFolderName.empty()) {
                    std::string sanitizedTitle = 
                        ProjectFolder::SanitizeProjectName(model.meta.title);
                    
                    if (!sanitizedTitle.empty() && 
                        sanitizedTitle != settingsOriginalFolderName) {
                        ImGui::SameLine();
                        if (ImGui::Button("Rename Folder")) {
                            if (app.RenameProjectFolder(model.meta.title)) {
                                // Update tracked names on success
                                settingsOriginalFolderName = sanitizedTitle;
                                settingsOriginalTitle = model.meta.title;
                            } else {
                                // Revert title to match original on failure
                                model.meta.title = settingsOriginalTitle;
                            }
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(
                                "Rename project folder to \"%s\"", 
                                sanitizedTitle.c_str());
                        }
                    }
                }
                
                char authorBuf[256];
                strncpy(authorBuf, model.meta.author.c_str(), 
                    sizeof(authorBuf) - 1);
                authorBuf[sizeof(authorBuf) - 1] = '\0';
                if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
                    model.meta.author = authorBuf;
                    model.MarkDirty();
                }
                
                ImGui::Spacing();
                
                // Project description (multi-line)
                char descBuf[2048];
                strncpy(descBuf, model.meta.description.c_str(), 
                    sizeof(descBuf) - 1);
                descBuf[sizeof(descBuf) - 1] = '\0';
                ImGui::Text("Description");
                if (ImGui::InputTextMultiline("##description", descBuf, 
                                              sizeof(descBuf),
                                              ImVec2(-1, 120.0f),
                                              ImGuiInputTextFlags_WordWrap)) {
                    model.meta.description = descBuf;
                    model.MarkDirty();
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // Preview info
                ImGui::Text("Canvas Information");
                ImGui::Separator();
                ImGui::Spacing();
                
                int totalCells = model.grid.cols * model.grid.rows;
                int pixelWidth = model.grid.cols * model.grid.tileWidth;
                int pixelHeight = model.grid.rows * model.grid.tileHeight;
                
                ImGui::Text("Total cells: %d", totalCells);
                ImGui::Text("Canvas size: %d × %d pixels", 
                    pixelWidth, pixelHeight);
                ImGui::Text("Cell size: %d × %d pixels", 
                    model.grid.tileWidth, model.grid.tileHeight);
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
}

}  // namespace Cartograph
