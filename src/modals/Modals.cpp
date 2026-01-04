#include "Modals.h"
#include "../UI.h"
#include <glad/gl.h>

namespace Cartograph {

Modals::Modals(UI& ui) : m_ui(ui) {
    // Initialize modal state
}

Modals::~Modals() {
    // Cleanup logo textures
    if (cartographLogoTexture != 0) {
        glDeleteTextures(1, &cartographLogoTexture);
        cartographLogoTexture = 0;
    }
    if (unveilLogoTexture != 0) {
        glDeleteTextures(1, &unveilLogoTexture);
        unveilLogoTexture = 0;
    }
}

bool Modals::AnyModalVisible() const {
    return showExportModal ||
           showSettingsModal ||
           showRenameIconModal ||
           showDeleteIconModal ||
           showRebindModal ||
           showColorPickerModal ||
           showNewProjectModal ||
           showProjectBrowserModal ||
           showWhatsNew ||
           showAutosaveRecoveryModal ||
           showLoadingModal ||
           showQuitConfirmationModal ||
           showNewRoomDialog ||
           showRenameRoomDialog ||
           showDeleteRoomDialog ||
           showRemoveFromRegionDialog ||
           showRenameRegionDialog ||
           showDeleteRegionDialog ||
           showAboutModal ||
           showSaveBeforeActionModal ||
           showFillConfirmationModal ||
           showProjectActionModal ||
           showMarkerLabelRenameModal;
}

void Modals::RenderAll(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs,
    KeymapManager& keymap,
    std::string& selectedIconName,
    std::string& selectedMarkerId,
    int& selectedTileId
) {
    if (showExportModal) RenderExportModal(model, canvas);
    if (showSettingsModal) RenderSettingsModal(app, model, keymap);
    if (showRenameIconModal) {
        RenderRenameIconModal(model, icons, selectedIconName);
    }
    if (showDeleteIconModal) {
        RenderDeleteIconModal(model, icons, history, selectedIconName, 
                              selectedMarkerId);
    }
    if (showRebindModal) RenderRebindModal(model, keymap);
    if (showColorPickerModal) {
        RenderColorPickerModal(model, history, selectedTileId);
    }
    if (showNewProjectModal) RenderNewProjectModal(app, model);
    if (showWhatsNew) RenderWhatsNewPanel();
    if (showAutosaveRecoveryModal) RenderAutosaveRecoveryModal(app, model);
    if (showLoadingModal) RenderLoadingModal(app, model, jobs, icons);
    if (showQuitConfirmationModal) RenderQuitConfirmationModal(app, model);
    if (showSaveBeforeActionModal) RenderSaveBeforeActionModal(app, model);
    if (showAboutModal) RenderAboutModal();
    if (showDeleteRoomDialog) RenderDeleteRoomModal(model, history);
    if (showRemoveFromRegionDialog) RenderRemoveFromRegionModal(model);
    if (showRenameRoomDialog) RenderRenameRoomModal(model);
    if (showRenameRegionDialog) RenderRenameRegionModal(model);
    if (showDeleteRegionDialog) RenderDeleteRegionModal(model, history);
    if (showFillConfirmationModal) RenderFillConfirmationModal(model, history);
    if (showMarkerLabelRenameModal) {
        RenderMarkerLabelRenameModal(model, history);
    }
}

} // namespace Cartograph
