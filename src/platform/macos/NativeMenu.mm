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
    
    m_callbacks["assets.import_icon"] = [this]() {
        // This needs access to UI::ImportIcon
        // Will be handled through external callback set in InitializeMenuCallbacks
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
    NSMenuItem* aboutItem = CreateMenuItem(appMenu, "About Cartograph", 
                                           "help.about", "", 0);
    
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
    NSMenuItem* quitItem = CreateMenuItem(appMenu, "Quit Cartograph",
                                         "file.quit", "q",
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

void NativeMenuMacOS::BuildFileMenu(NSMenu* menuBar) {
    // File menu
    NSMenuItem* fileMenuItem = [[NSMenuItem alloc] init];
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    
    // Disable auto-enable so we can manually control menu item states
    [fileMenu setAutoenablesItems:NO];
    
    CreateMenuItem(fileMenu, "New Project...", "file.new", "n", 
                   NSEventModifierFlagCommand);
    CreateMenuItem(fileMenu, "Open Project...", "file.open", "o", 
                   NSEventModifierFlagCommand);
    
    CreateSeparator(fileMenu);
    
    // Editor-only items (stored for state management, retain for reference)
    m_saveItem = CreateMenuItem(fileMenu, "Save", "file.save", "s", 
                                NSEventModifierFlagCommand);
    [m_saveItem retain];
    
    m_saveAsItem = CreateMenuItem(fileMenu, "Save As...", "file.save_as", "s",
                                  NSEventModifierFlagCommand | 
                                  NSEventModifierFlagShift);
    [m_saveAsItem retain];
    
    CreateSeparator(fileMenu);
    
    m_exportPackageItem = CreateMenuItem(fileMenu, 
                                         "Export Package (.cart)...", 
                                         "file.export_package", "e",
                                         NSEventModifierFlagCommand | 
                                         NSEventModifierFlagShift);
    [m_exportPackageItem retain];
    
    m_exportPngItem = CreateMenuItem(fileMenu, "Export PNG...", 
                                     "file.export_png", "e",
                                     NSEventModifierFlagCommand);
    [m_exportPngItem retain];
    
    [fileMenuItem setSubmenu:fileMenu];
    [menuBar addItem:fileMenuItem];
    [fileMenu release];
    [fileMenuItem release];
}

void NativeMenuMacOS::BuildEditMenu(NSMenu* menuBar) {
    // Edit menu
    NSMenuItem* editMenuItem = [[NSMenuItem alloc] init];
    NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    
    // Disable auto-enable so we can manually control menu item states
    [editMenu setAutoenablesItems:NO];
    
    m_undoItem = CreateMenuItem(editMenu, "Undo", "edit.undo", "z",
                                NSEventModifierFlagCommand);
    [m_undoItem retain];
    
    m_redoItem = CreateMenuItem(editMenu, "Redo", "edit.redo", "y",
                                NSEventModifierFlagCommand);
    [m_redoItem retain];
    
    // Note: Settings moved to application menu (standard macOS pattern)
    
    [editMenuItem setSubmenu:editMenu];
    [menuBar addItem:editMenuItem];
    [editMenu release];
    [editMenuItem release];
}

void NativeMenuMacOS::BuildViewMenu(NSMenu* menuBar) {
    // View menu (entire menu is Editor-only)
    m_viewMenu = [[NSMenuItem alloc] init];
    [m_viewMenu retain];
    NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    
    // Disable auto-enable so we can manually control menu item states
    [viewMenu setAutoenablesItems:NO];
    
    m_propertiesPanelItem = CreateMenuItem(
        viewMenu, "Hierarchy Panel", "view.properties", "p",
        NSEventModifierFlagCommand
    );
    
    CreateSeparator(viewMenu);
    
    m_showGridItem = CreateMenuItem(
        viewMenu, "Show Grid", "view.grid", "g", 0
    );
    
    CreateSeparator(viewMenu);
    
    CreateMenuItem(viewMenu, "Zoom In", "view.zoom_in", "=", 0);
    CreateMenuItem(viewMenu, "Zoom Out", "view.zoom_out", "-", 0);
    CreateMenuItem(viewMenu, "Reset Zoom", "view.zoom_reset", "0", 0);
    
    [m_viewMenu setSubmenu:viewMenu];
    [menuBar addItem:m_viewMenu];
    [viewMenu release];
}

void NativeMenuMacOS::BuildAssetsMenu(NSMenu* menuBar) {
    // Assets menu (entire menu is Editor-only)
    m_assetsMenu = [[NSMenuItem alloc] init];
    [m_assetsMenu retain];
    NSMenu* assetsMenu = [[NSMenu alloc] initWithTitle:@"Assets"];
    
    // Disable auto-enable so we can manually control menu item states
    [assetsMenu setAutoenablesItems:NO];
    
    CreateMenuItem(assetsMenu, "Import Icon...", "assets.import_icon", "", 0);
    
    [m_assetsMenu setSubmenu:assetsMenu];
    [menuBar addItem:m_assetsMenu];
    [assetsMenu release];
}

void NativeMenuMacOS::BuildWindowMenu(NSMenu* menuBar) {
    // Window menu (standard macOS menu)
    NSMenuItem* windowMenuItem = [[NSMenuItem alloc] init];
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    
    // Standard window menu items
    NSMenuItem* minimizeItem = [[NSMenuItem alloc] 
        initWithTitle:@"Minimize"
        action:@selector(performMiniaturize:)
        keyEquivalent:@"m"];
    [minimizeItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [windowMenu addItem:minimizeItem];
    [minimizeItem release];
    
    NSMenuItem* zoomItem = [[NSMenuItem alloc]
        initWithTitle:@"Zoom"
        action:@selector(performZoom:)
        keyEquivalent:@""];
    [windowMenu addItem:zoomItem];
    [zoomItem release];
    
    [windowMenu addItem:[NSMenuItem separatorItem]];
    
    NSMenuItem* bringAllToFrontItem = [[NSMenuItem alloc]
        initWithTitle:@"Bring All to Front"
        action:@selector(arrangeInFront:)
        keyEquivalent:@""];
    [windowMenu addItem:bringAllToFrontItem];
    [bringAllToFrontItem release];
    
    [windowMenuItem setSubmenu:windowMenu];
    [menuBar addItem:windowMenuItem];
    
    // Tell NSApplication to manage this window menu
    [[NSApplication sharedApplication] setWindowsMenu:windowMenu];
    
    [windowMenu release];
    [windowMenuItem release];
}

void NativeMenuMacOS::BuildHelpMenu(NSMenu* menuBar) {
    // Help menu (About moved to application menu on macOS)
    NSMenuItem* helpMenuItem = [[NSMenuItem alloc] init];
    NSMenu* helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
    
    // Disable auto-enable so we can manually control menu item states
    [helpMenu setAutoenablesItems:NO];
    
    CreateMenuItem(helpMenu, "View License", "help.license", "", 0);
    CreateMenuItem(helpMenu, "Report Bug...", "help.report_bug", "", 0);
    
    [helpMenuItem setSubmenu:helpMenu];
    [menuBar addItem:helpMenuItem];
    [helpMenu release];
    [helpMenuItem release];
}

NSMenuItem* NativeMenuMacOS::CreateMenuItem(
    NSMenu* menu,
    const std::string& title,
    const std::string& action,
    const std::string& keyEquivalent,
    unsigned int modifierMask
) {
    NSString* nsTitle = [NSString stringWithUTF8String:title.c_str()];
    NSString* nsKey = [NSString stringWithUTF8String:keyEquivalent.c_str()];
    NSString* nsAction = [NSString stringWithUTF8String:action.c_str()];
    
    NSMenuItem* item = [[NSMenuItem alloc]
        initWithTitle:nsTitle
        action:@selector(menuItemSelected:)
        keyEquivalent:nsKey];
    
    if (modifierMask != 0) {
        [item setKeyEquivalentModifierMask:modifierMask];
    }
    
    [item setTarget:m_delegate];
    [item setRepresentedObject:nsAction];
    
    [menu addItem:item];
    
    // Menu now owns this item, so we can release our alloc
    // If caller wants to keep a reference, they need to retain it
    [item autorelease];
    
    return item;
}

NSMenuItem* NativeMenuMacOS::CreateSeparator(NSMenu* menu) {
    NSMenuItem* separator = [NSMenuItem separatorItem];
    [menu addItem:separator];
    return separator;
}

} // namespace Cartograph

