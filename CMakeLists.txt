cmake_minimum_required(VERSION 3.21)

project(mmo
    VERSION 0.1.0
    DESCRIPTION "Custom 3D engine foundation"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "vcpkg toolchain not found. Set VCPKG_ROOT to your vcpkg clone "
        "(the directory that contains scripts/buildsystems/vcpkg.cmake) "
        "and reconfigure.")
endif()

find_package(glfw3 CONFIG REQUIRED)

add_library(glad STATIC
    third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include")
set_target_properties(glad PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON)

add_executable(mmo_client
    src/main.cpp
    src/core/application.cpp
    src/core/camera_controller.cpp
    src/platform/window.cpp
    src/renderer/renderer.cpp)

target_include_directories(mmo_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(mmo_client PRIVATE
    glfw
    glad)

if(WIN32)
    target_link_libraries(mmo_client PRIVATE opengl32)
endif()

if(MSVC)
    target_compile_options(mmo_client PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(glad PRIVATE /W3 /utf-8)
endif()

set_target_properties(mmo_client PROPERTIES
    OUTPUT_NAME "mmo_client"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
