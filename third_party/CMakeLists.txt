# Third-party dependencies for Cartograph

# SDL3 (vendored)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt)
    # Configure SDL3 options
    set(SDL_SHARED OFF CACHE BOOL "Build SDL3 as static library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 as static library" FORCE)
    set(SDL_TEST OFF CACHE BOOL "Disable SDL3 tests" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Disable SDL3 tests" FORCE)

    # Disable unused SDL3 subsystems to reduce binary size
    set(SDL_AUDIO OFF CACHE BOOL "Disable audio" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "Disable joystick" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "Disable haptic" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "Disable HID API" FORCE)
    set(SDL_POWER OFF CACHE BOOL "Disable power" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "Disable sensor" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "Disable camera" FORCE)
    set(SDL_GPU OFF CACHE BOOL "Disable GPU API" FORCE)
    
    # Add SDL3 subdirectory
    add_subdirectory(SDL EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "SDL not found. Run ./setup_deps.sh to download it.")
endif()

# GLAD (OpenGL loader for cross-platform GL 3.3 Core)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/glad/build/src/gl.c)
    add_library(glad STATIC
        glad/build/src/gl.c
    )
    target_include_directories(glad PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/build/include
    )
    # Link OpenGL framework/library (for context creation, not function loading)
    if(APPLE)
        target_link_libraries(glad PUBLIC "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(glad PUBLIC opengl32)
    elseif(UNIX)
        target_link_libraries(glad PUBLIC GL dl)
    endif()
else()
    message(FATAL_ERROR "GLAD not found. Run ./setup_deps.sh to generate it.")
endif()

# Dear ImGui
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

# Tell ImGui to use GLAD as its OpenGL loader
# Disable unused features to reduce binary size
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLAD2
    IMGUI_DISABLE_DEMO_WINDOWS
    IMGUI_DISABLE_DEBUG_TOOLS
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)

# Link to vendored SDL3-static and GLAD (GLAD provides include dirs)
target_link_libraries(imgui PUBLIC SDL3-static glad)

# nlohmann/json (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann
)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# stb libraries (header-only)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/stb
)

# minizip-ng for .cart ZIP handling
# Configure minizip-ng to only build what we need
set(MZ_COMPAT ON CACHE BOOL "Enable compatibility layer" FORCE)
set(MZ_ZLIB ON CACHE BOOL "Enable ZLIB compression" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "Disable BZIP2 compression" FORCE)
set(MZ_LZMA OFF CACHE BOOL "Disable LZMA compression" FORCE)
set(MZ_ZSTD OFF CACHE BOOL "Disable ZSTD compression" FORCE)
set(MZ_OPENSSL OFF CACHE BOOL "Disable OpenSSL encryption" FORCE)
set(MZ_BUILD_TESTS OFF CACHE BOOL "Disable minizip tests" FORCE)
set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "Disable minizip unit tests" FORCE)
set(MZ_FETCH_LIBS OFF CACHE BOOL "Don't fetch libs automatically" FORCE)

# Add minizip-ng as subdirectory
# Note: minizip-ng creates a target called "minizip"
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/minizip-ng/CMakeLists.txt)
    add_subdirectory(minizip-ng EXCLUDE_FROM_ALL)
    # Target "minizip" is created by minizip-ng's CMakeLists.txt
    # Also available as MINIZIP::minizip alias
else()
    message(WARNING "minizip-ng not found. Run ./setup_deps.sh to download it.")
    # Create a dummy target to prevent build errors
    add_library(minizip INTERFACE)
endif()

# NanoSVG (optional, header-only)
if(USE_SVG_ICONS)
    add_library(nanosvg INTERFACE)
    target_include_directories(nanosvg INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/nanosvg
    )
endif()

