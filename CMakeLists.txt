cmake_minimum_required(VERSION 3.16)
project(Cartograph VERSION 1.0.0 LANGUAGES CXX)

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
    src/Canvas.cpp
    src/Model.cpp
    src/History.cpp
    src/IOJson.cpp
    src/Package.cpp
    src/ProjectFolder.cpp
    src/ExportPng.cpp
    src/Thumbnail.cpp
    src/Icons.cpp
    src/Keymap.cpp
    src/Jobs.cpp
    src/render/GlRenderer.cpp
    src/platform/Fs.cpp
    src/platform/Paths.cpp
    src/platform/Time.cpp
)

# Platform-specific sources
if(APPLE)
    list(APPEND CARTOGRAPH_SOURCES
        src/platform/Fs_macos.mm
    )
endif()

set(CARTOGRAPH_HEADERS
    src/App.h
    src/UI.h
    src/Canvas.h
    src/Model.h
    src/History.h
    src/IOJson.h
    src/Package.h
    src/ProjectFolder.h
    src/ExportPng.h
    src/Thumbnail.h
    src/Icons.h
    src/Keymap.h
    src/Jobs.h
    src/render/Renderer.h
    src/render/GlRenderer.h
    src/platform/Fs.h
    src/platform/Paths.h
    src/platform/Time.h
)

# Executable
add_executable(Cartograph ${CARTOGRAPH_SOURCES} ${CARTOGRAPH_HEADERS})

# Include directories
target_include_directories(Cartograph PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Link libraries
target_link_libraries(Cartograph PRIVATE
    imgui
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
    set_target_properties(Cartograph PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Cartograph"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unveil.cartograph"
        MACOSX_BUNDLE_INFO_STRING "Unveil Cartograph - Metroidvania Map Editor"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    )
    
    # Copy assets into bundle
    add_custom_command(TARGET Cartograph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_BUNDLE_CONTENT_DIR:Cartograph>/Resources/assets
    )
endif()

# Windows: Copy assets next to executable
if(WIN32)
    add_custom_command(TARGET Cartograph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:Cartograph>/assets
    )
endif()

# Install rules
install(TARGETS Cartograph
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(NOT APPLE)
    install(DIRECTORY assets DESTINATION .)
endif()

