#include "Modals.h"
#include "../Config.h"
#include "../platform/Paths.h"
#include "../platform/System.h"
#include <imgui.h>
#include <glad/gl.h>
#include <stb/stb_image.h>

namespace Cartograph {

void Modals::RenderWhatsNewPanel() {
    // Only call OpenPopup once when modal is first shown
    if (!whatsNewModalOpened) {
        ImGui::OpenPopup("What's New in Cartograph");
        whatsNewModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("What's New in Cartograph", &showWhatsNew)) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), 
            "Version %s", CARTOGRAPH_VERSION);
        ImGui::Separator();
        ImGui::Spacing();
        
        // Version 1.1.0 features
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "New in 1.1.0");
        ImGui::BulletText(
            "Coordinates updated with auto-migration for old .cart files");
        ImGui::BulletText("Proper multiline support in descriptions");
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "(Thanks to TomNaughton)");
        ImGui::Unindent();
        ImGui::Spacing();
        
        ImGui::Separator();
        ImGui::Spacing();
        
        // Previous release summary
        if (ImGui::CollapsingHeader("Version 1.0.0 - Initial Release")) {
            ImGui::BulletText("Infinite pan/zoom canvas with grid snapping");
            ImGui::BulletText("Paint, Erase, Fill, and Eyedropper tools");
            ImGui::BulletText("Room painting with auto-wall generation");
            ImGui::BulletText("Walls and doors on cell edges");
            ImGui::BulletText("Named rooms with metadata and tags");
            ImGui::BulletText("Region groups for area organization");
            ImGui::BulletText("Welcome screen with project templates");
            ImGui::BulletText("PNG export with configurable layers");
            ImGui::BulletText("Full undo/redo history");
            ImGui::BulletText("Dark and Light themes");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "What's Next:");
        ImGui::TextWrapped("Have a feature request?");
        ImGui::Spacing();
        
        if (ImGui::SmallButton("Submit a feature request on GitHub")) {
            Platform::OpenURL(
                "https://github.com/Unveil-gg/Cartograph/issues/new"
                "?template=feature_request.md"
            );
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showWhatsNew = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    // Reset opened flag when modal closes
    if (!showWhatsNew) {
        whatsNewModalOpened = false;
    }
}

void Modals::RenderAboutModal() {
    // Only call OpenPopup once when modal is first shown
    if (!aboutModalOpened) {
        ImGui::OpenPopup("About Cartograph");
        aboutModalOpened = true;
    }
    
    // Load logo textures if not already loaded
    if (!logosLoaded) {
        std::string assetsDir = Platform::GetAssetsDir();
        std::string cartographLogoPath = 
            assetsDir + "project/cartograph-logo.png";
        std::string unveilLogoPath = assetsDir + "project/unveil-logo.png";
        
        // Load Cartograph logo
        int width, height, channels;
        unsigned char* data = stbi_load(
            cartographLogoPath.c_str(),
            &width, &height, &channels, 4
        );
        if (data) {
            GLuint texId;
            glGenTextures(1, &texId);
            glBindTexture(GL_TEXTURE_2D, texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                        0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            cartographLogoTexture = texId;
            cartographLogoWidth = width;
            cartographLogoHeight = height;
        }
        
        // Load Unveil logo
        data = stbi_load(
            unveilLogoPath.c_str(),
            &width, &height, &channels, 4
        );
        if (data) {
            GLuint texId;
            glGenTextures(1, &texId);
            glBindTexture(GL_TEXTURE_2D, texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                        0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            unveilLogoTexture = texId;
            unveilLogoWidth = width;
            unveilLogoHeight = height;
        }
        
        logosLoaded = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
                           ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("About Cartograph", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 8));
        
        // Set a reasonable fixed width for the modal
        ImGui::SetNextWindowContentSize(ImVec2(500.0f, 0.0f));
        
        // Cartograph logo at top (smaller and centered)
        if (cartographLogoTexture != 0 && cartographLogoWidth > 0 && 
            cartographLogoHeight > 0) {
            // Smaller max size for compact layout
            float maxSize = 120.0f;
            float aspectRatio = (float)cartographLogoWidth / 
                               (float)cartographLogoHeight;
            float logoWidth, logoHeight;
            
            if (aspectRatio >= 1.0f) {
                logoWidth = maxSize;
                logoHeight = maxSize / aspectRatio;
            } else {
                logoHeight = maxSize;
                logoWidth = maxSize * aspectRatio;
            }
            
            float availWidth = ImGui::GetContentRegionAvail().x;
            float cursorX = (availWidth - logoWidth) * 0.5f;
            if (cursorX > 0) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
            }
            ImGui::Image((ImTextureID)(intptr_t)cartographLogoTexture, 
                        ImVec2(logoWidth, logoHeight));
        }
        
        // Version (centered)
        char versionText[32];
        snprintf(versionText, sizeof(versionText), "v%s", CARTOGRAPH_VERSION);
        float textWidth = ImGui::CalcTextSize(versionText).x;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float cursorX = (availWidth - textWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        ImGui::TextDisabled("%s", versionText);
        
        ImGui::Spacing();
        
        // Description (compact, single line centered)
        const char* descText = "Metroidvania map editor for game developers";
        textWidth = ImGui::CalcTextSize(descText).x;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - textWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        ImGui::Text("%s", descText);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Unveil logo (centered, clickable)
        if (unveilLogoTexture != 0 && unveilLogoWidth > 0 && 
            unveilLogoHeight > 0) {
            float maxSize = 80.0f;
            float aspectRatio = (float)unveilLogoWidth / 
                               (float)unveilLogoHeight;
            float logoWidth, logoHeight;
            
            if (aspectRatio >= 1.0f) {
                logoWidth = maxSize;
                logoHeight = maxSize / aspectRatio;
            } else {
                logoHeight = maxSize;
                logoWidth = maxSize * aspectRatio;
            }
            
            float availWidth = ImGui::GetContentRegionAvail().x;
            float cursorX = (availWidth - logoWidth) * 0.5f;
            if (cursorX > 0) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
            }
            
            ImGui::Image((ImTextureID)(intptr_t)unveilLogoTexture, 
                        ImVec2(logoWidth, logoHeight));
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if (ImGui::IsItemClicked()) {
                Platform::OpenURL("https://unveilengine.com");
            }
        }
        
        // "Made by Unveil" text (centered, directly under logo)
        const char* madeByText = "Made by Unveil";
        textWidth = ImGui::CalcTextSize(madeByText).x;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - textWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        ImGui::TextDisabled("%s", madeByText);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // GitHub button (centered at bottom)
        float buttonWidth = 150.0f;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - buttonWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        if (ImGui::Button("GitHub Repository", ImVec2(buttonWidth, 0))) {
            Platform::OpenURL(
                "https://github.com/Unveil-gg/Cartograph"
            );
        }
        
        ImGui::Spacing();
        
        ImGui::PopStyleVar();
        
        // Close button (centered)
        float closeButtonWidth = 100.0f;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - closeButtonWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        if (ImGui::Button("Close", ImVec2(closeButtonWidth, 0))) {
            showAboutModal = false;
            aboutModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph
