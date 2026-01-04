#pragma once

#include "ModalTypes.h"
#include "../Model.h"
#include "../History.h"
#include "../ExportPng.h"
#include <string>
#include <vector>
#include <atomic>

// Forward declarations
namespace Cartograph {
    class App;
    class Canvas;
    class IconManager;
    class JobQueue;
    class KeymapManager;
    class UI;
    struct RecentProject;
    struct Marker;
}

namespace Cartograph {

/**
 * Modals manager - handles all modal dialogs in the application.
 * Extracted from UI class for better organization.
 */
class Modals {
public:
    Modals(UI& ui);
    ~Modals();
    
    /**
     * Render all active modals (called every frame).
     * @param app Application instance
     * @param model Current model
     * @param canvas Canvas instance
     * @param history History for undo/redo
     * @param icons Icon manager
     * @param jobs Job queue
     * @param keymap Keymap manager
     * @param selectedIconName Reference to canvas panel's selected icon
     * @param selectedMarkerId Reference to canvas panel's selected marker ID
     * @param selectedTileId Reference to canvas panel's selected tile ID
     */
    void RenderAll(
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
    );
    
    /**
     * Render project browser modal (needs recentProjects from UI).
     * Public so UI can call it with recentProjects parameter.
     */
    void RenderProjectBrowserModal(
        App& app, 
        std::vector<RecentProject>& recentProjects
    );
    
    /**
     * Render marker label rename modal (rename all vs create new).
     * Called when editing label from placed markers palette.
     */
    void RenderMarkerLabelRenameModal(Model& model, History& history);
    
    /**
     * Render project action modal (delete/remove from list).
     * Called when X button clicked on project card.
     * @param recentProjects Reference to recent projects list for removal
     */
    void RenderProjectActionModal(std::vector<RecentProject>& recentProjects);
    
    /**
     * Check if any modal or dialog is currently visible.
     * Used to prevent thumbnail capture while UI overlays are shown.
     * @return true if any modal is visible
     */
    bool AnyModalVisible() const;
    
    // Modal visibility flags
    bool showExportModal = false;
    bool shouldShowExportPngDialog = false;
    bool showSettingsModal = false;
    bool showRenameIconModal = false;
    bool showDeleteIconModal = false;
    bool showRebindModal = false;
    bool showColorPickerModal = false;
    bool showNewProjectModal = false;
    bool showProjectBrowserModal = false;
    bool showWhatsNew = false;
    bool showAutosaveRecoveryModal = false;
    bool showLoadingModal = false;
    bool showQuitConfirmationModal = false;
    bool showNewRoomDialog = false;
    bool showRenameRoomDialog = false;
    bool showDeleteRoomDialog = false;
    bool showRemoveFromRegionDialog = false;
    bool showRenameRegionDialog = false;
    bool showDeleteRegionDialog = false;
    bool showAboutModal = false;
    bool showSaveBeforeActionModal = false;
    bool showFillConfirmationModal = false;
    bool showProjectActionModal = false;  // Delete/remove project modal
    
    // Modal popup opened tracking flags (to call OpenPopup only once)
    bool exportModalOpened = false;
    bool settingsModalOpened = false;
    bool renameIconModalOpened = false;
    bool deleteIconModalOpened = false;
    bool rebindModalOpened = false;
    bool colorPickerModalOpened = false;
    bool newProjectModalOpened = false;
    bool projectBrowserModalOpened = false;
    bool autosaveRecoveryModalOpened = false;
    bool loadingModalOpened = false;
    bool quitConfirmationModalOpened = false;
    bool newRoomDialogOpened = false;
    bool renameRoomDialogOpened = false;
    bool deleteRoomDialogOpened = false;
    bool removeFromRegionDialogOpened = false;
    bool renameRegionDialogOpened = false;
    bool deleteRegionDialogOpened = false;
    bool aboutModalOpened = false;
    bool saveBeforeActionModalOpened = false;
    bool fillConfirmationModalOpened = false;
    bool whatsNewModalOpened = false;
    bool projectActionModalOpened = false;
    
    // Fill confirmation modal state
    enum class PendingFillType { None, Tile, Room };
    PendingFillType pendingFillType = PendingFillType::None;
    size_t pendingFillCellCount = 0;
    bool fillConfirmed = false;  // Set true when user confirms
    
    // Export modal state
    ExportOptions exportOptions;
    
    // Settings modal state
    int settingsModalSelectedTab = 1;  // 0=Project, 1=Grid&Canvas, 2=Keybindings
    std::string settingsOriginalFolderName;  // Folder name when modal opened
    std::string settingsOriginalTitle;       // Title when modal opened
    
    // Icon rename modal state
    std::string renameIconOldName;
    char renameIconNewName[64] = "";
    
    // Icon delete modal state
    std::string deleteIconName;
    int deleteIconMarkerCount = 0;
    std::vector<std::string> deleteIconAffectedMarkers;
    
    // Marker label rename modal state
    bool showMarkerLabelRenameModal = false;
    std::string markerLabelRenameStyleKey;  // "icon:colorHex" style key
    std::string markerLabelRenameNewLabel;  // New label to apply
    int markerLabelRenameCount = 0;         // How many markers to rename
    
    // Keybinding rebind modal state
    std::string rebindAction = "";
    std::string rebindActionDisplayName = "";
    std::string capturedBinding = "";
    bool isCapturing = false;
    
    // New project modal state
    NewProjectConfig newProjectConfig;
    ProjectTemplate selectedTemplate = ProjectTemplate::Medium;
    
    // Loading modal state
    std::string loadingFilePath;
    std::string loadingFileName;
    std::atomic<bool> loadingCancelled{false};
    double loadingStartTime = 0.0;
    
    // Color picker modal state
    int colorPickerEditingTileId = -1;  // -1 = new color, >0 = editing
    char colorPickerName[128] = "";
    float colorPickerColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // RGBA
    float colorPickerOriginalColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    bool colorPickerDeleteRequested = false;
    
    // Room dialog state
    char newRoomName[64] = "New Room";
    float newRoomColor[3] = {1.0f, 0.5f, 0.5f};
    std::string editingRoomId;      // Room being renamed/deleted
    std::string editingRegionId;    // Region being renamed/deleted
    char renameBuffer[128] = "";
    
    // Save before action modal state
    enum class PendingAction {
        None,
        NewProject,
        OpenProject
    };
    PendingAction pendingAction = PendingAction::None;
    
    // Project action modal state (delete/remove from list)
    std::string projectActionPath;       // Path of project being acted upon
    std::string projectActionName;       // Name of project for display
    
    // About modal logo textures
    unsigned int cartographLogoTexture = 0;
    unsigned int unveilLogoTexture = 0;
    int cartographLogoWidth = 0;
    int cartographLogoHeight = 0;
    int unveilLogoWidth = 0;
    int unveilLogoHeight = 0;
    bool logosLoaded = false;
    
    // Public helpers (called from UI)
    void UpdateNewProjectPath();
    void ShowNewProjectFolderPicker();
    
    // Get UI reference (for modal implementations)
    UI& GetUI() { return m_ui; }
    
private:
    // Room/Region dialogs (RoomRegionModals.cpp)
    void RenderDeleteRoomModal(Model& model, History& history);
    void RenderRemoveFromRegionModal(Model& model);
    void RenderRenameRoomModal(Model& model);
    void RenderRenameRegionModal(Model& model);
    void RenderDeleteRegionModal(Model& model, History& history);
    
    // Icon/Marker modals (IconMarkerModals.cpp)
    void RenderRenameIconModal(Model& model, IconManager& icons, 
                               std::string& selectedIconName);
    void RenderDeleteIconModal(Model& model, IconManager& icons, 
                               History& history, std::string& selectedIconName,
                               std::string& selectedMarkerId);
    
    // Settings/Export modals (SettingsExportModals.cpp)
    void RenderExportModal(Model& model, Canvas& canvas);
    void RenderSettingsModal(App& app, Model& model, KeymapManager& keymap);
    void RenderColorPickerModal(Model& model, History& history,
                                int& selectedTileId);
    
    // Project modals (ProjectModals.cpp)
    void RenderNewProjectModal(App& app, Model& model);
    void RenderSaveBeforeActionModal(App& app, Model& model);
    void ApplyTemplate(ProjectTemplate tmpl);
    
    // Confirmation modals (ConfirmationModals.cpp)
    void RenderFillConfirmationModal(Model& model, History& history);
    void RenderAutosaveRecoveryModal(App& app, Model& model);
    void RenderLoadingModal(App& app, Model& model, JobQueue& jobs, 
                           IconManager& icons);
    void RenderQuitConfirmationModal(App& app, Model& model);
    
    // About/WhatsNew (AboutWhatsNew.cpp)
    void RenderWhatsNewPanel();
    void RenderAboutModal();
    
    // Rebind modal (RebindModal.cpp)
    void RenderRebindModal(Model& model, KeymapManager& keymap);
    
    UI& m_ui;  // Reference to UI for ShowToast
};

} // namespace Cartograph
