cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain")
endif()

project(mmo VERSION 0.1.0 DESCRIPTION "Custom 3D engine foundation" LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(MMO_BUILD_CLIENT "Build the graphical client (requires GLFW and OpenGL at runtime)" ON)
include(CTest)

find_package(glm CONFIG REQUIRED)

add_library(mmo_build_options INTERFACE)
target_include_directories(mmo_build_options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
    target_compile_options(mmo_build_options INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(mmo_build_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_library(mmo_math INTERFACE)
target_link_libraries(mmo_math INTERFACE mmo_build_options glm::glm)

add_library(mmo_game STATIC
    src/game/characters/character.cpp
    src/game/items/item.cpp)
target_link_libraries(mmo_game PUBLIC mmo_math)

add_library(mmo_assets STATIC
    src/assets/mesh_catalog.cpp
    src/assets/procedural_meshes.cpp)
target_link_libraries(mmo_assets PUBLIC mmo_math)

add_library(mmo_scene STATIC
    src/scene/camera.cpp
    src/scene/camera_controller.cpp)
target_link_libraries(mmo_scene PUBLIC mmo_math)

add_library(mmo_presentation STATIC
    src/game/client/character_visual.cpp
    src/game/client/demo_scene.cpp)
target_link_libraries(mmo_presentation PUBLIC mmo_game mmo_assets mmo_scene)

if(MMO_BUILD_CLIENT)
    find_package(glfw3 CONFIG REQUIRED)
    add_library(glad STATIC third_party/glad/src/gl.c)
    target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include")
    set_target_properties(glad PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

    add_library(mmo_platform STATIC src/platform/window.cpp)
    target_link_libraries(mmo_platform PUBLIC mmo_build_options PRIVATE glfw)

    add_library(mmo_renderer STATIC
        src/renderer/renderer.cpp
        src/renderer/opengl/shader.cpp
        src/renderer/opengl/mesh.cpp)
    target_link_libraries(mmo_renderer PUBLIC mmo_scene mmo_assets PRIVATE glad)
    if(WIN32)
        target_link_libraries(mmo_renderer PRIVATE opengl32)
    endif()

    add_executable(mmo_client src/apps/client/main.cpp src/apps/client/application.cpp)
    target_link_libraries(mmo_client PRIVATE mmo_platform mmo_renderer mmo_presentation)
    set_target_properties(mmo_client PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(BUILD_TESTING)
    add_executable(mmo_domain_tests tests/domain_tests.cpp)
    target_link_libraries(mmo_domain_tests PRIVATE mmo_game mmo_presentation)
    add_test(NAME domain COMMAND mmo_domain_tests)
endif()
