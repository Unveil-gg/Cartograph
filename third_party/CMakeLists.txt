# Third-party dependencies for Cartograph

# SDL3
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found, will need to be vendored or installed")
    # For now, assume SDL3 is installed via Homebrew or system package manager
    # On macOS: brew install sdl3
    find_package(SDL3 REQUIRED)
endif()

# Dear ImGui (docking branch)
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
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

target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)

if(APPLE)
    target_link_libraries(imgui PUBLIC "-framework OpenGL")
elseif(WIN32)
    target_link_libraries(imgui PUBLIC opengl32)
elseif(UNIX)
    target_link_libraries(imgui PUBLIC GL)
endif()

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
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/minizip-ng/CMakeLists.txt)
    add_subdirectory(minizip-ng EXCLUDE_FROM_ALL)
    # Create an alias for easier linking
    add_library(minizip ALIAS minizip-ng)
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

