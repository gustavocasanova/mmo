cmake_minimum_required(VERSION 3.21)

project(mmo
    VERSION 0.1.0
    DESCRIPTION "Custom 3D engine foundation"
    LANGUAGES C CXX)

include(CTest)

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
find_package(glm CONFIG REQUIRED)

add_library(glad STATIC
    third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include")
set_target_properties(glad PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON)

add_library(mmo_character STATIC
    src/animation/animation_controller.cpp
    src/animation/skeleton.cpp
    src/assets/model.cpp
    src/character/character.cpp
    src/character/character_body.cpp
    src/character/character_model.cpp
    src/equipment/equipment.cpp
)
target_include_directories(mmo_character PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mmo_character PUBLIC cxx_std_20)

add_executable(mmo_client
    src/main.cpp
    src/core/application.cpp
    src/core/camera_controller.cpp
    src/platform/window.cpp
    src/renderer/camera.cpp
    src/renderer/character.cpp
    src/renderer/material.cpp
    src/renderer/mesh.cpp
    src/renderer/renderer.cpp
    src/renderer/shader.cpp
)

target_include_directories(mmo_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(mmo_client PRIVATE
    glfw
    glm::glm
    glad
    mmo_character)

if(BUILD_TESTING)
    add_executable(mmo_character_tests
        tests/character_equipment_test.cpp)
    target_link_libraries(mmo_character_tests PRIVATE mmo_character)
    add_test(NAME CharacterEquipmentTest COMMAND mmo_character_tests)
endif()

if(WIN32)
    target_link_libraries(mmo_client PRIVATE opengl32)
endif()

if(MSVC)
    target_compile_options(mmo_client PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(mmo_character PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    if(BUILD_TESTING)
        target_compile_options(mmo_character_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    target_compile_options(glad PRIVATE /W3 /utf-8)
endif()

set_target_properties(mmo_client PROPERTIES
    OUTPUT_NAME "mmo_client"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
