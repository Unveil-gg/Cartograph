#pragma once

#include "NativeMenu.h"
#include <map>

// Forward declare Objective-C classes
#ifdef __OBJC__
@class NSMenu;
@class NSMenuItem;
@class MenuDelegate;
#else
typedef struct objc_object NSMenu;
typedef struct objc_object NSMenuItem;
typedef struct objc_object MenuDelegate;
#endif

namespace Cartograph {

/**
 * macOS native menu implementation using NSMenu and NSMenuItem.
 * Integrates with the system menu bar for proper macOS behavior.
 */
class NativeMenuMacOS : public INativeMenu {
public:
    NativeMenuMacOS();
    ~NativeMenuMacOS() override;
    
    void Initialize() override;
    void Update(
        App& app,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        JobQueue& jobs
    ) override;
    void Render() override;
    bool IsNative() const override { return true; }
    void SetCallback(
        const std::string& action,
        MenuCallback callback
    ) override;
    
    /**
     * Trigger a callback by action name.
     * Called from Objective-C menu delegate.
     */
    void TriggerCallback(const std::string& action);
    
private:
    void BuildMenuBar();
    void BuildFileMenu(NSMenu* menuBar);
    void BuildEditMenu(NSMenu* menuBar);
    void BuildViewMenu(NSMenu* menuBar);
    void BuildAssetsMenu(NSMenu* menuBar);
    void BuildWindowMenu(NSMenu* menuBar);
    void BuildHelpMenu(NSMenu* menuBar);
    
    NSMenuItem* CreateMenuItem(
        NSMenu* menu,
        const std::string& title,
        const std::string& action,
        const std::string& keyEquivalent = "",
        unsigned int modifierMask = 0
    );
    
    NSMenuItem* CreateSeparator(NSMenu* menu);
    
    // Stored references for updating menu state (Editor-only items)
    NSMenuItem* m_saveItem;
    NSMenuItem* m_saveAsItem;
    NSMenuItem* m_exportPackageItem;
    NSMenuItem* m_exportPngItem;
    NSMenuItem* m_settingsItem;
    NSMenuItem* m_undoItem;
    NSMenuItem* m_redoItem;
    NSMenuItem* m_viewMenu;
    NSMenuItem* m_assetsMenu;
    NSMenuItem* m_propertiesPanelItem;
    NSMenuItem* m_showGridItem;
    
    // Callback map
    std::map<std::string, MenuCallback> m_callbacks;
    
    // Menu delegate (Objective-C object)
    MenuDelegate* m_delegate;
    
    // Keep references to UI state
    App* m_app;
    Model* m_model;
    Canvas* m_canvas;
    History* m_history;
    IconManager* m_icons;
    JobQueue* m_jobs;
};

} // namespace Cartograph

