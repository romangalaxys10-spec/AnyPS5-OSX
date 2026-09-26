cmake_minimum_required(VERSION 3.20)
project(AnyPS5OSX CXX)

# AnyPS5 OSX Edition — macOS (Metal/MetalFX + MoltenVK), Linux (Vulkan), Windows (Vulkan)
# Derived from boykopovar/AnyPS5 (GPL-2.0), see docs/UPSTREAM_README.md and NOTICE.

option(BUILD_TESTING "Build and register relinker tests" OFF)
if(BUILD_TESTING)
    enable_testing()
endif()

option(ANYPS5_ENABLE_SPIRV_TOOLS "Enable SPIRV-Tools validation and optimization" OFF)

# GPU backend selection for the AgcDriver execution layer.
# AUTO  : native Metal on Apple hosts when MetalFX is available, Vulkan elsewhere.
# METAL : force the native Metal + MetalFX backend (Apple only, fails otherwise).
# VULKAN: force the Vulkan backend (on macOS this runs on top of MoltenVK).
set(ANYPS5_GPU_BACKEND "AUTO" CACHE STRING "AgcDriver GPU backend: AUTO, METAL or VULKAN")
set_property(CACHE ANYPS5_GPU_BACKEND PROPERTY STRINGS AUTO METAL VULKAN)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
endif()

if(APPLE)
    enable_language(OBJCXX)
    # Darwin binaries must never be fully statically linked against the system runtime.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Metal -framework MetalFX -framework Foundation -framework QuartzCore -weak_framework MetalFX")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO ON CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(SDL_X11 OFF CACHE BOOL "" FORCE)
else()
    set(SDL_X11 ON CACHE BOOL "" FORCE)
endif()
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
set(ANYPS5_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "")
add_subdirectory(3rdparty/SDL2)
set(CMAKE_EXE_LINKER_FLAGS "${ANYPS5_EXE_LINKER_FLAGS}")

add_subdirectory(core/shader/recompiler)
add_subdirectory(core/libs)
add_subdirectory(core/relinker)
add_subdirectory(launcher)
