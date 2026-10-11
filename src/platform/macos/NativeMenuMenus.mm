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

// Objective-C delegate to handle menu actions (must be outside namespace)



namespace Cartograph {

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
    
    CreateSeparator(editMenu);
    
    m_cutItem = CreateMenuItem(editMenu, "Cut", "edit.cut", "x",
                               NSEventModifierFlagCommand);
    [m_cutItem retain];
    
    m_copyItem = CreateMenuItem(editMenu, "Copy", "edit.copy", "c",
                                NSEventModifierFlagCommand);
    [m_copyItem retain];
    
    m_pasteItem = CreateMenuItem(editMenu, "Paste", "edit.paste", "v",
                                 NSEventModifierFlagCommand);
    [m_pasteItem retain];
    
    m_deleteItem = CreateMenuItem(editMenu, "Delete", "edit.delete", "", 0);
    [m_deleteItem setKeyEquivalentModifierMask:0];
    [m_deleteItem setKeyEquivalent:[NSString stringWithFormat:@"%C", 
                                   (unichar)NSDeleteCharacter]];
    [m_deleteItem retain];
    
    CreateSeparator(editMenu);
    
    m_selectAllItem = CreateMenuItem(editMenu, "Select All", "edit.selectAll", 
                                     "a", NSEventModifierFlagCommand);
    [m_selectAllItem retain];
    
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

