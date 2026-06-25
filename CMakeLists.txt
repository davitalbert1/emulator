cmake_minimum_required(VERSION 3.20)
project(emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT MSVC)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)

# 1. SDL2 Setup (Static build to ease distribution and avoid DLL copy issues)
set(SDL_SHARED OFF CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Install SDL" FORCE)

message(STATUS "Fetching SDL2...")
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.3
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl2"
)
FetchContent_MakeAvailable(SDL2)

# 2. Dear ImGui Setup
message(STATUS "Fetching Dear ImGui...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.8
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui"
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    )

    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PRIVATE SDL2-static)
endif()

# ---------------------------------------------------------------------------
# Emulator Target
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/EmulatorApp.cpp
    src/GameBoy.cpp
    src/GameBoyAdvance.cpp
    src/GameBoyColor.cpp
    src/NintendoDS.cpp
    src/Nintendo2DS.cpp
    src/Nintendo3DS.cpp
    src/Atari2600.cpp
    src/NES.cpp
    src/SNES.cpp
    src/SegaGenesis.cpp
    src/Nintendo64.cpp
    src/NintendoWii.cpp
    src/NintendoSwitch.cpp
)

add_executable(emulator ${SOURCES})

target_include_directories(emulator PRIVATE
    include
)

target_link_libraries(emulator PRIVATE
    imgui
    SDL2-static
)

if(WIN32)
    target_link_libraries(emulator PRIVATE comdlg32)
endif()

# No SDL2main is needed because the app uses SDL_MAIN_HANDLED and a standard main() entry point.

# Platform-specific compile options
if(MSVC)
    target_compile_options(emulator PRIVATE /W4 /O2)
else()
    target_compile_options(emulator PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
        -Wno-sign-compare
        -Wno-unused-variable
    )
endif()

# Static linking for MinGW to make the executable portable (no dynamic gcc/stdC++ DLL dependencies)
if(WIN32 AND NOT MSVC)
    target_link_options(emulator PRIVATE -static-libgcc -static-libstdc++ -static)
endif()