cmake_minimum_required(VERSION 3.16)
project(Cartograph VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_WEB "Build for Web (Emscripten)" OFF)
option(USE_STATIC_CRT "Use static CRT on Windows" OFF)
option(USE_SVG_ICONS "Enable SVG icon support via NanoSVG" OFF)

# Platform-specific settings
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    set(CMAKE_MACOSX_RPATH ON)
endif()

if(WIN32 AND USE_STATIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Third-party dependencies
add_subdirectory(third_party)

# Core library: model, persistence, and reachability. No window or UI.
# Shared by the app, the CLI, and the tests.
set(CARTOGRAPH_CORE_SOURCES
    src/Model.cpp
    src/Model_Extras.cpp
    src/Model_Interior.cpp
    src/MarkerKinds.cpp
    src/Reachability.cpp
    src/ProjectLoad.cpp
    src/cli/Cli.cpp
    src/Reachability_Describe.cpp
    src/IOJson_Extras.cpp
    src/IOJson_Interior.cpp
    src/Model_ExpandGridIfNeeded.cpp
    src/Model_FindMarker.cpp
    src/Model_InvalidateAllRoomCellCaches.cpp
    src/Model_DetectedRoom.cpp
    src/Model_GenerateRoomPerimeterWalls.cpp
    src/IOJson.cpp
    src/IOJson_LoadFromString.cpp
    src/IOJson_Migrate.cpp
    src/ProjectFolder.cpp
    src/Package.cpp
    src/Icons.cpp
    src/IconManager_ProcessIconFromFile.cpp
    src/ExportPng.cpp
    src/Theme/Themes.cpp
    src/platform/Fs.cpp
)
add_library(cartograph_core STATIC ${CARTOGRAPH_CORE_SOURCES})
target_include_directories(cartograph_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(cartograph_core PUBLIC
    nlohmann_json::nlohmann_json
    glad
    imgui
    SDL3-static
)
if(TARGET minizip)
    target_link_libraries(cartograph_core PUBLIC minizip)
    target_include_directories(cartograph_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip-ng
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip-ng/compat
    )
endif()
if(APPLE)
    target_link_libraries(cartograph_core PUBLIC "-framework OpenGL")
endif()
if(MSVC)
    target_compile_options(cartograph_core PRIVATE /W4)
else()
    target_compile_options(cartograph_core PRIVATE -Wall -Wextra)
endif()

# Source files
set(CARTOGRAPH_SOURCES
    src/main.cpp
    src/App.cpp
    src/UI.cpp
    src/UI_RenderUnassignedRooms.cpp
    src/UI_RenderRegionNodes.cpp
    src/UI_RenderRoomDetails.cpp
    src/UI_RenderRoomFields.cpp
    src/UI_RenderRegionProperties.cpp
    src/UI_RenderPropsSections.cpp
    src/UI_RenderDoorInspector.cpp
    src/UI_RenderValidationPanel.cpp
    src/UI/InteriorEditor.cpp
    src/UI/InteriorEditor_Tools.cpp
    src/UI/InteriorEditor_Toolbar.cpp
    src/Canvas_RenderInterior.cpp
    src/history/PaintInteriorCommand.cpp
    src/modals/Modals_RenderAppearanceSettings.cpp
    src/modals/Modals_RenderKeybindingSettings.cpp
    src/modals/Modals_RenderGridSettings.cpp
    src/modals/Modals_RenderGameDataSettings.cpp
    src/modals/Modals_RenderProjectSettings.cpp
    src/UI_Render.cpp
    src/UI_RenderMenuBar.cpp
    src/UI_RenderToolsPanel.cpp
    src/UI_RenderPropertiesPanel.cpp
    src/UI_RenderStatusBar.cpp
    src/UI_ImportIcon.cpp
    src/UI_StartAsyncProjectLoad.cpp
    src/App_ApplyTheme.cpp
    src/App_SaveProjectAs.cpp
    src/modals/Modals_RenderColorPickerModal.cpp
    src/modals/Modals_RenderProjectBrowserModal.cpp
    src/modals/Modals_RenderProjectActionModal.cpp
    src/modals/Modals_ShowNewProjectFolderPicker.cpp
    src/UI/WelcomeScreen_Render.cpp
    src/UI/WelcomeScreen_LoadRecentProjects.cpp
    src/App_SetupImGui.cpp
    src/UI/WelcomeScreen_RenderRecentProjectsList.cpp
    src/UI/WelcomeScreen_GeneratePlaceholderTexture.cpp
    src/Canvas_RenderEdges.cpp
    src/Canvas_RenderDoorGates.cpp
    src/Canvas_RenderValidationOverlay.cpp
    src/Canvas_RenderRoomOverlays.cpp
    src/UI/CanvasPanel.cpp
    src/UI/CanvasPanel_DrawEyedropperPreview.cpp
    src/UI/CanvasPanel_DrawFillPreview.cpp
    src/UI/CanvasPanel_DrawMarkerDropPreview.cpp
    src/UI/CanvasPanel_DrawMarkerSnapPreview.cpp
    src/UI/CanvasPanel_DrawPaintPreview.cpp
    src/UI/CanvasPanel_DrawPastePreview.cpp
    src/UI/CanvasPanel_DrawRoomSelectHover.cpp
    src/UI/CanvasPanel_DrawSelectionDrag.cpp
    src/UI/CanvasPanel_DrawSelectionHighlight.cpp
    src/UI/CanvasPanel_HandleCanvasKeys.cpp
    src/UI/CanvasPanel_HandleEraseInput.cpp
    src/UI/CanvasPanel_HandleEyedropperInput.cpp
    src/UI/CanvasPanel_HandleFillInput.cpp
    src/UI/CanvasPanel_HandleMarkerInput.cpp
    src/UI/CanvasPanel_HandlePaintInput.cpp
    src/UI/CanvasPanel_HandlePaintRelease.cpp
    src/UI/CanvasPanel_HandleRoomEraseInput.cpp
    src/UI/CanvasPanel_HandleRoomFillInput.cpp
    src/UI/CanvasPanel_HandleRoomPaintInput.cpp
    src/UI/CanvasPanel_HandleRoomSelectDrag.cpp
    src/UI/CanvasPanel_HandleRoomSelectInput.cpp
    src/UI/CanvasPanel_HandleSelectInput.cpp
    src/UI/CanvasPanel_HandleZoomInput.cpp
    src/UI/CanvasPanel_HandleToolShortcuts.cpp
    src/UI/CanvasPanel_Selection.cpp
    src/UI/CanvasPanel_ClearSelection.cpp
    src/UI/CanvasPanel_SelectionMove.cpp
    src/UI/CanvasPanel_Cursors.cpp
    src/UI/CanvasPanel_DoorSelection.cpp
    src/UI/ImGuiHelpers.cpp
    src/UI/WelcomeScreen.cpp
    src/modals/Modals.cpp
    src/modals/ProjectModals.cpp
    src/modals/RoomRegionModals.cpp
    src/modals/IconMarkerModals.cpp
    src/modals/SettingsExportModals.cpp
    src/modals/ConfirmationModals.cpp
    src/modals/AboutWhatsNew.cpp
    src/modals/RebindModal.cpp
    src/Canvas.cpp
    src/history/History.cpp
    src/platform/Time.cpp
    src/history/PaintTilesCommand.cpp
    src/history/FillTilesCommand.cpp
    src/history/CreateRoomCommand.cpp
    src/history/DeleteRoomCommand.cpp
    src/history/ModifyRoomPropertiesCommand.cpp
    src/history/ModifyRoomAssignmentsCommand.cpp
    src/history/CreateRegionCommand.cpp
    src/history/DeleteRegionCommand.cpp
    src/history/ModifyRegionPropertiesCommand.cpp
    src/history/ModifyEdgesCommand.cpp
    src/history/ModifyDoorPropsCommand.cpp
    src/history/ModifyPropsCommand.cpp
    src/history/SetMarkerKindCommand.cpp
    src/UI/CanvasPanel_PlaceMarker.cpp
    src/UI_RenderMarkerKind.cpp
    src/UI/PropsTable.cpp
    src/history/PlaceMarkerCommand.cpp
    src/history/DeleteMarkerCommand.cpp
    src/history/MoveMarkersCommand.cpp
    src/history/ModifyMarkerPropertiesCommand.cpp
    src/history/DeleteIconCommand.cpp
    src/history/AddPaletteColorCommand.cpp
    src/history/RemovePaletteColorCommand.cpp
    src/history/UpdatePaletteColorCommand.cpp
    src/history/SetZoomCommand.cpp
    src/history/DetectRoomsCommand.cpp
    src/Thumbnail.cpp
    src/Keymap.cpp
    src/Jobs.cpp
    src/Preferences.cpp
    src/render/GlRenderer.cpp
    src/platform/Paths.cpp
    src/platform/Time.cpp
    src/platform/System.cpp
    src/platform/NativeMenu.cpp
    src/platform/NativeMenu_imgui.cpp
    src/UI_RenderEraseOptions.cpp
    src/UI_RenderEyedropperOptions.cpp
    src/UI_RenderMarkerIcons.cpp
    src/UI_RenderMarkerSettings.cpp
    src/UI_RenderPaintOptions.cpp
    src/UI_RenderPlacedMarkers.cpp
    src/UI_RenderRoomToolOptions.cpp
    src/UI_RenderSelectOptions.cpp
    src/UI_RenderToolButtons.cpp
)

# Platform-specific sources
if(APPLE)
    list(APPEND CARTOGRAPH_SOURCES
        src/platform/macos/Fs.mm
        src/platform/macos/NativeMenu.mm
        src/platform/macos/NativeMenuMenus.mm
    )
endif()

if(WIN32)
    list(APPEND CARTOGRAPH_SOURCES
        src/platform/windows/Fs.cpp
        src/platform/windows/app.rc
    )
endif()

set(CARTOGRAPH_HEADERS
    src/App.h
    src/UI.h
    src/UI/CanvasPanel.h
    src/UI/ImGuiHelpers.h
    src/UI/Modals.h
    src/UI/WelcomeScreen.h
    src/modals/Modals.h
    src/modals/ModalTypes.h
    src/Canvas.h
    src/Color.h
    src/Model.h
    src/History.h
    src/history/ICommand.h
    src/history/Constants.h
    src/history/History.h
    src/history/Commands.h
    src/history/RoomPropertiesSnapshot.h
    src/history/RegionPropertiesSnapshot.h
    src/history/MarkerPropertiesSnapshot.h
    src/history/PaintTilesCommand.h
    src/history/FillTilesCommand.h
    src/history/CreateRoomCommand.h
    src/history/DeleteRoomCommand.h
    src/history/ModifyRoomPropertiesCommand.h
    src/history/ModifyRoomAssignmentsCommand.h
    src/history/CreateRegionCommand.h
    src/history/DeleteRegionCommand.h
    src/history/ModifyRegionPropertiesCommand.h
    src/history/ModifyEdgesCommand.h
    src/history/ModifyDoorPropsCommand.h
    src/history/ModifyPropsCommand.h
    src/history/PaintInteriorCommand.h
    src/InteriorColors.h
    src/UI/InteriorEditor.h
    src/history/SetMarkerKindCommand.h
    src/MarkerKinds.h
    src/UI/PropsTable.h
    src/history/PlaceMarkerCommand.h
    src/history/DeleteMarkerCommand.h
    src/history/MoveMarkersCommand.h
    src/history/ModifyMarkerPropertiesCommand.h
    src/history/DeleteIconCommand.h
    src/history/AddPaletteColorCommand.h
    src/history/RemovePaletteColorCommand.h
    src/history/UpdatePaletteColorCommand.h
    src/history/SetZoomCommand.h
    src/history/DetectRoomsCommand.h
    src/IOJson.h
    src/Package.h
    src/ProjectFolder.h
    src/ExportPng.h
    src/Thumbnail.h
    src/Icons.h
    src/Keymap.h
    src/Jobs.h
    src/Preferences.h
    src/Theme/Themes.h
    src/render/Renderer.h
    src/render/GlRenderer.h
    src/platform/Fs.h
    src/platform/Paths.h
    src/platform/Time.h
    src/platform/System.h
    src/platform/NativeMenu.h
    src/platform/NativeMenu_imgui.h
)

# Platform-specific headers
if(APPLE)
    list(APPEND CARTOGRAPH_HEADERS
        src/platform/macos/NativeMenu.h
    )
endif()

# Executable (WIN32 creates GUI app without console window)
if(WIN32)
    add_executable(Cartograph WIN32 ${CARTOGRAPH_SOURCES} ${CARTOGRAPH_HEADERS})
else()
    add_executable(Cartograph ${CARTOGRAPH_SOURCES} ${CARTOGRAPH_HEADERS})
endif()

if(MSVC)
    target_compile_options(Cartograph PRIVATE /W4)
else()
    target_compile_options(Cartograph PRIVATE -Wall -Wextra)
endif()

# Include directories
target_include_directories(Cartograph PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Link libraries
target_link_libraries(Cartograph PRIVATE
    cartograph_core
    imgui
    glad
    nlohmann_json::nlohmann_json
    SDL3-static
)

# Link minizip if available (minizip-ng creates a target called "minizip")
if(TARGET minizip)
    target_link_libraries(Cartograph PRIVATE minizip)
    # Add minizip-ng include directories explicitly (compat layer for old API)
    target_include_directories(Cartograph PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip-ng
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minizip-ng/compat
    )
endif()

if(USE_SVG_ICONS)
    target_compile_definitions(Cartograph PRIVATE CARTOGRAPH_USE_SVG)
    target_compile_definitions(cartograph_core PRIVATE CARTOGRAPH_USE_SVG)
    target_link_libraries(Cartograph PRIVATE nanosvg)
    target_link_libraries(cartograph_core PUBLIC nanosvg)
endif()

# Version string for runtime display
target_compile_definitions(Cartograph PRIVATE
    CARTOGRAPH_VERSION="${PROJECT_VERSION}"
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(Cartograph PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(Cartograph PRIVATE
        opengl32
    )
elseif(UNIX)
    target_link_libraries(Cartograph PRIVATE
        GL
        dl
        pthread
    )
endif()

# macOS app bundle
if(APPLE)
    # Code signing options (set via cmake -D or environment)
    # For development: use ad-hoc signing (default)
    # For distribution: set CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
    if(NOT DEFINED CODESIGN_IDENTITY)
        set(CODESIGN_IDENTITY "-" CACHE STRING "Code signing identity")
    endif()
    
    # Team ID for notarization (required for distribution)
    if(NOT DEFINED DEVELOPMENT_TEAM)
        set(DEVELOPMENT_TEAM "" CACHE STRING "Apple Developer Team ID")
    endif()
    
    set_target_properties(Cartograph PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Cartograph"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unveil.cartograph"
        MACOSX_BUNDLE_INFO_STRING "Unveil Cartograph - Metroidvania Map Editor"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "cartograph.icns"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2025 Unveil. MIT License."
        # Custom Info.plist with UTI declarations for .cart and .cartproj
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/assets/project/Info.plist.in"
        # Signing attributes
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${CODESIGN_IDENTITY}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/Cartograph.entitlements"
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS NO
    )
    
    # Copy icon into bundle Resources
    add_custom_command(TARGET Cartograph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/assets/project/cartograph.icns
        $<TARGET_BUNDLE_CONTENT_DIR:Cartograph>/Resources/cartograph.icns
    )
    
    # Copy assets into bundle (excluding repo/ which is for GitHub only)
    add_custom_command(TARGET Cartograph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_BUNDLE_CONTENT_DIR:Cartograph>/Resources/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/icons
        $<TARGET_BUNDLE_CONTENT_DIR:Cartograph>/Resources/assets/icons
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/project
        $<TARGET_BUNDLE_CONTENT_DIR:Cartograph>/Resources/assets/project
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/tools
        $<TARGET_BUNDLE_CONTENT_DIR:Cartograph>/Resources/assets/tools
    )
endif()

# Windows: Copy assets next to executable (excluding repo/ which is for GitHub only)
if(WIN32)
    add_custom_command(TARGET Cartograph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:Cartograph>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/icons
        $<TARGET_FILE_DIR:Cartograph>/assets/icons
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/project
        $<TARGET_FILE_DIR:Cartograph>/assets/project
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/tools
        $<TARGET_FILE_DIR:Cartograph>/assets/tools
    )
endif()

# Install rules
install(TARGETS Cartograph
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(NOT APPLE)
    # Install assets excluding repo/ (GitHub-only assets)
    install(DIRECTORY assets/icons assets/project assets/tools
            DESTINATION assets)
endif()

# Tests. They link the core library, not the UI.
enable_testing()
add_executable(cartograph_tests
    tests/load_tests.cpp
)
target_link_libraries(cartograph_tests PRIVATE cartograph_core)
target_compile_definitions(cartograph_tests PRIVATE
    CARTOGRAPH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(cartograph_tests PRIVATE /W4)
else()
    target_compile_options(cartograph_tests PRIVATE -Wall -Wextra)
endif()
add_test(NAME load_tests COMMAND cartograph_tests)

# Extra test executables: one source file each.
foreach(test_name format_tests reachability_tests cli_tests
        interior_tests)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE cartograph_core)
    target_compile_definitions(${test_name} PRIVATE
        CARTOGRAPH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W4)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# The undo history is app code but only needs the model.
target_sources(interior_tests PRIVATE
    src/history/History.cpp
    src/platform/Time.cpp
    src/history/PaintInteriorCommand.cpp)

# Headless command line tool: export and validate without a window.
add_executable(cartograph-cli src/cli/main.cpp)
target_link_libraries(cartograph-cli PRIVATE cartograph_core)
if(MSVC)
    target_compile_options(cartograph-cli PRIVATE /W4)
else()
    target_compile_options(cartograph-cli PRIVATE -Wall -Wextra)
endif()
install(TARGETS cartograph-cli RUNTIME DESTINATION bin)
