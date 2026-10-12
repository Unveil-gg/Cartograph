#include "NativeMenu.h"
#include "../../App.h"
#include "../../Model.h"
#include "../../Canvas.h"
#include "../../History.h"
#include "../../Icons.h"
#include "../../Jobs.h"
#include "../System.h"

#import <Cocoa/Cocoa.h>

// Forward declare C++ class for Objective-C delegate
namespace Cartograph {
    class NativeMenuMacOS;
}

// Objective-C delegate to handle menu actions (must be outside namespace)
@interface MenuDelegate : NSObject
@property (nonatomic, assign) Cartograph::NativeMenuMacOS* menuImpl;
- (void)menuItemSelected:(id)sender;
@end

@implementation MenuDelegate
- (void)menuItemSelected:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    NSString* action = [item representedObject];
    if (action && _menuImpl) {
        std::string actionStr = [action UTF8String];
        _menuImpl->TriggerCallback(actionStr);
    }
}
@end

namespace Cartograph {

NativeMenuMacOS::NativeMenuMacOS()
    : m_saveItem(nullptr)
    , m_saveAsItem(nullptr)
    , m_exportPackageItem(nullptr)
    , m_exportPngItem(nullptr)
    , m_settingsItem(nullptr)
    , m_undoItem(nullptr)
    , m_redoItem(nullptr)
    , m_cutItem(nullptr)
    , m_copyItem(nullptr)
    , m_pasteItem(nullptr)
    , m_deleteItem(nullptr)
    , m_selectAllItem(nullptr)
    , m_viewMenu(nullptr)
    , m_assetsMenu(nullptr)
    , m_propertiesPanelItem(nullptr)
    , m_showGridItem(nullptr)
    , m_delegate(nullptr)
    , m_app(nullptr)
    , m_model(nullptr)
    , m_canvas(nullptr)
    , m_history(nullptr)
    , m_icons(nullptr)
    , m_jobs(nullptr)
{
}

NativeMenuMacOS::~NativeMenuMacOS() {
    // Release all stored menu item references
    if (m_saveItem) [m_saveItem release];
    if (m_saveAsItem) [m_saveAsItem release];
    if (m_exportPackageItem) [m_exportPackageItem release];
    if (m_exportPngItem) [m_exportPngItem release];
    if (m_settingsItem) [m_settingsItem release];
    if (m_undoItem) [m_undoItem release];
    if (m_redoItem) [m_redoItem release];
    if (m_viewMenu) [m_viewMenu release];
    if (m_assetsMenu) [m_assetsMenu release];
    
    if (m_delegate) {
        [m_delegate release];
        m_delegate = nullptr;
    }
}

void NativeMenuMacOS::Initialize() {
    // Create delegate
    m_delegate = [[MenuDelegate alloc] init];
    m_delegate.menuImpl = this;
    
    // Build the menu bar
    BuildMenuBar();
}

void NativeMenuMacOS::Update(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
    // Store references for menu updates
    m_app = &app;
    m_model = &model;
    m_canvas = &canvas;
    m_history = &history;
    m_icons = &icons;
    m_jobs = &jobs;
    
    // Check if we're in Editor state (vs Welcome screen)
    bool isEditor = (app.GetState() == AppState::Editor);
    
    // Update Application menu - Settings only in Editor
    if (m_settingsItem) {
        [m_settingsItem setEnabled:isEditor ? YES : NO];
    }
    
    // Update File menu - Save/Export only in Editor
    if (m_saveItem) {
        [m_saveItem setEnabled:isEditor ? YES : NO];
    }
    if (m_saveAsItem) {
        [m_saveAsItem setEnabled:isEditor ? YES : NO];
    }
    if (m_exportPackageItem) {
        [m_exportPackageItem setEnabled:isEditor ? YES : NO];
    }
    if (m_exportPngItem) {
        [m_exportPngItem setEnabled:isEditor ? YES : NO];
    }
    
    // Update Edit menu - Undo/Redo only in Editor with history
    if (m_undoItem) {
        [m_undoItem setEnabled:(isEditor && history.CanUndo()) ? YES : NO];
    }
    if (m_redoItem) {
        [m_redoItem setEnabled:(isEditor && history.CanRedo()) ? YES : NO];
    }
    
    // Update Edit menu - Selection operations only in Editor
    // Note: Fine-grained enable based on selection state is handled by 
    // callbacks checking state at trigger time
    if (m_cutItem) {
        [m_cutItem setEnabled:isEditor ? YES : NO];
    }
    if (m_copyItem) {
        [m_copyItem setEnabled:isEditor ? YES : NO];
    }
    if (m_pasteItem) {
        [m_pasteItem setEnabled:isEditor ? YES : NO];
    }
    if (m_deleteItem) {
        [m_deleteItem setEnabled:isEditor ? YES : NO];
    }
    if (m_selectAllItem) {
        [m_selectAllItem setEnabled:isEditor ? YES : NO];
    }
    
    // View and Assets menus - entire menus only in Editor
    if (m_viewMenu) {
        [m_viewMenu setEnabled:isEditor ? YES : NO];
    }
    if (m_assetsMenu) {
        [m_assetsMenu setEnabled:isEditor ? YES : NO];
    }
    
    // Update action callbacks that need model/history/canvas access
    m_callbacks["edit.undo"] = [&history, &model]() {
        if (history.CanUndo()) {
            history.Undo(model);
        }
    };
    
    m_callbacks["edit.redo"] = [&history, &model]() {
        if (history.CanRedo()) {
            history.Redo(model);
        }
    };
    
    m_callbacks["view.zoom_in"] = [&canvas]() {
        canvas.SetZoom(canvas.zoom * 1.2f);
    };
    
    m_callbacks["view.zoom_out"] = [&canvas]() {
        canvas.SetZoom(canvas.zoom / 1.2f);
    };
    
    m_callbacks["view.zoom_reset"] = [&canvas]() {
        canvas.SetZoom(2.5f);
    };
    
    m_callbacks["assets.import_icon"] = []() {
        // Placeholder - actual callback set via SetCallback() in UI.cpp
    };
}

void NativeMenuMacOS::Render() {
    // No-op for native menus (rendered by macOS)
}

void NativeMenuMacOS::SetCallback(
    const std::string& action,
    MenuCallback callback
) {
    m_callbacks[action] = callback;
}

void NativeMenuMacOS::TriggerCallback(const std::string& action) {
    auto it = m_callbacks.find(action);
    if (it != m_callbacks.end() && it->second) {
        it->second();
    }
}

void NativeMenuMacOS::BuildMenuBar() {
    NSApplication* app = [NSApplication sharedApplication];
    NSMenu* menuBar = [[NSMenu alloc] init];
    
    // Build menus (Application menu is automatically added by macOS as first item)
    // Add empty menu item for app menu (macOS will populate it)
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Cartograph"];
    
    // Disable auto-enable so we can manually control menu item states
    [appMenu setAutoenablesItems:NO];
    
    // About
    CreateMenuItem(appMenu, "About Cartograph", "help.about", "", 0);
    
    [appMenu addItem:[NSMenuItem separatorItem]];
    
    // Settings (Preferences on macOS) - stored for state management
    m_settingsItem = CreateMenuItem(appMenu, "Settings...", 
                                    "edit.settings", ",",
                                    NSEventModifierFlagCommand);
    [m_settingsItem retain];
    
    [appMenu addItem:[NSMenuItem separatorItem]];
    
    // Services (standard macOS item)
    NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
    NSMenuItem* servicesItem = [[NSMenuItem alloc] initWithTitle:@"Services"
                                                          action:nil
                                                   keyEquivalent:@""];
    [servicesItem setSubmenu:servicesMenu];
    [appMenu addItem:servicesItem];
    
    // Tell NSApplication about the services menu (before releasing)
    [app setServicesMenu:servicesMenu];
    
    [servicesMenu release];
    [servicesItem release];
    
    [appMenu addItem:[NSMenuItem separatorItem]];
    
    // Hide
    NSMenuItem* hideItem = [[NSMenuItem alloc] 
        initWithTitle:@"Hide Cartograph"
               action:@selector(hide:)
        keyEquivalent:@"h"];
    [hideItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [appMenu addItem:hideItem];
    [hideItem release];
    
    // Hide Others
    NSMenuItem* hideOthersItem = [[NSMenuItem alloc]
        initWithTitle:@"Hide Others"
               action:@selector(hideOtherApplications:)
        keyEquivalent:@"h"];
    [hideOthersItem setKeyEquivalentModifierMask:
        NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [appMenu addItem:hideOthersItem];
    [hideOthersItem release];
    
    // Show All
    NSMenuItem* showAllItem = [[NSMenuItem alloc]
        initWithTitle:@"Show All"
               action:@selector(unhideAllApplications:)
        keyEquivalent:@""];
    [appMenu addItem:showAllItem];
    [showAllItem release];
    
    [appMenu addItem:[NSMenuItem separatorItem]];
    
    // Quit
    CreateMenuItem(appMenu, "Quit Cartograph", "file.quit", "q",
                   NSEventModifierFlagCommand);
    
    [appMenuItem setSubmenu:appMenu];
    [menuBar addItem:appMenuItem];
    [appMenu release];
    [appMenuItem release];
    
    // Build other menus
    BuildFileMenu(menuBar);
    BuildEditMenu(menuBar);
    BuildViewMenu(menuBar);
    BuildAssetsMenu(menuBar);
    BuildWindowMenu(menuBar);
    BuildHelpMenu(menuBar);
    
    [app setMainMenu:menuBar];
    [menuBar release];
}

} // namespace Cartograph

