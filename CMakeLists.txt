cmake_minimum_required(VERSION 3.16)
project(Cartograph VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Source files
set(CARTOGRAPH_SOURCES
    src/main.cpp
    src/App.cpp
    src/UI.cpp
    src/ui/CanvasPanel.cpp
    src/ui/ImGuiHelpers.cpp
    src/ui/WelcomeScreen.cpp
    src/modals/Modals.cpp
    src/modals/ProjectModals.cpp
    src/modals/RoomRegionModals.cpp
    src/modals/IconMarkerModals.cpp
    src/modals/SettingsExportModals.cpp
    src/modals/ConfirmationModals.cpp
    src/modals/AboutWhatsNew.cpp
    src/modals/RebindModal.cpp
    src/Canvas.cpp
    src/Model.cpp
    src/history/History.cpp
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
    src/IOJson.cpp
    src/Package.cpp
    src/ProjectFolder.cpp
    src/ExportPng.cpp
    src/Thumbnail.cpp
    src/Icons.cpp
    src/Keymap.cpp
    src/Jobs.cpp
    src/Preferences.cpp
    src/theme/Themes.cpp
    src/render/GlRenderer.cpp
    src/platform/Fs.cpp
    src/platform/Paths.cpp
    src/platform/Time.cpp
    src/platform/System.cpp
    src/platform/NativeMenu.cpp
    src/platform/NativeMenu_imgui.cpp
)

# Platform-specific sources
if(APPLE)
    list(APPEND CARTOGRAPH_SOURCES
        src/platform/macos/Fs.mm
        src/platform/macos/NativeMenu.mm
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
    src/ui/CanvasPanel.h
    src/ui/ImGuiHelpers.h
    src/ui/Modals.h
    src/ui/WelcomeScreen.h
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
    src/theme/Themes.h
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

# Include directories
target_include_directories(Cartograph PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Link libraries
target_link_libraries(Cartograph PRIVATE
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
    target_link_libraries(Cartograph PRIVATE nanosvg)
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

