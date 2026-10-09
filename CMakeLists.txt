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
find_package(fastgltf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

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
    src/animation/layered_animation.cpp
    src/character/combat.cpp
    src/assets/gltf_model_loader.cpp
    src/assets/model.cpp
    src/assets/static_model_geometry.cpp
    src/character/character.cpp
    src/character/character_controller.cpp
    src/character/character_body.cpp
    src/character/character_model.cpp
    src/equipment/equipment.cpp
    src/game/combat/combat.cpp
    src/game/world/collision_world.cpp
    src/game/world/terrain.cpp
)
target_include_directories(mmo_character PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mmo_character PUBLIC cxx_std_20)
target_link_libraries(mmo_character PUBLIC glm::glm PRIVATE fastgltf::fastgltf)

add_library(mmo_editor STATIC
    src/editor/asset_database.cpp
    src/editor/editor_camera.cpp
    src/editor/editor_history.cpp
    src/editor/selection.cpp
    src/editor/world_document.cpp
    src/editor/world_editor.cpp)
target_include_directories(mmo_editor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mmo_editor PUBLIC cxx_std_20)
target_link_libraries(mmo_editor PUBLIC glm::glm mmo_character)

add_executable(mmo_client
    src/apps/client/main.cpp
    src/apps/client/application.cpp
    src/apps/client/target_frame.cpp
    src/editor/editor_ui.cpp
    src/platform/window.cpp
    src/renderer/renderer.cpp
    src/renderer/opengl/mesh.cpp
    src/renderer/opengl/shader.cpp
    src/scene/camera.cpp
    src/scene/camera_controller.cpp
)

target_include_directories(mmo_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(mmo_client PRIVATE
    glfw
    glm::glm
    glad
    mmo_character
    mmo_editor
    imgui::imgui)

if(BUILD_TESTING)
    add_executable(mmo_character_tests
        tests/character_equipment_test.cpp)
    target_link_libraries(mmo_character_tests PRIVATE mmo_character)
    add_test(NAME CharacterEquipmentTest COMMAND mmo_character_tests)
    set_tests_properties(CharacterEquipmentTest PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mmo_camera_tests
        tests/camera_tests.cpp
        src/scene/camera.cpp
        src/scene/camera_controller.cpp)
    target_include_directories(mmo_camera_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(mmo_camera_tests PRIVATE glm::glm)
    target_link_libraries(mmo_camera_tests PRIVATE mmo_character)
    target_compile_features(mmo_camera_tests PRIVATE cxx_std_20)
    add_test(NAME ThirdPersonCameraTest COMMAND mmo_camera_tests)

    add_executable(mmo_controller_tests
        tests/character_controller_tests.cpp)
    target_link_libraries(mmo_controller_tests PRIVATE mmo_character)
    add_test(NAME CharacterControllerTest COMMAND mmo_controller_tests)
    set_tests_properties(CharacterControllerTest PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mmo_animation_layers_tests tests/animation_layers_test.cpp)
    target_link_libraries(mmo_animation_layers_tests PRIVATE mmo_character)
    add_test(NAME AnimationLayersTest COMMAND mmo_animation_layers_tests)

    add_executable(mmo_combat_tests tests/combat_test.cpp)
    target_link_libraries(mmo_combat_tests PRIVATE mmo_character)
    add_test(NAME CombatTest COMMAND mmo_combat_tests)

    add_executable(mmo_terrain_tests tests/terrain_test.cpp)
    target_link_libraries(mmo_terrain_tests PRIVATE mmo_character)
    add_test(NAME TerrainTest COMMAND mmo_terrain_tests)

    add_executable(mmo_world_editor_tests tests/world_editor_mode_test.cpp)
    target_link_libraries(mmo_world_editor_tests PRIVATE mmo_editor mmo_character)
    add_test(NAME WorldEditorModeTest COMMAND mmo_world_editor_tests)
endif()

if(WIN32)
    target_link_libraries(mmo_client PRIVATE opengl32)
endif()

if(MSVC)
    target_compile_options(mmo_client PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(mmo_character PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(mmo_editor PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    if(BUILD_TESTING)
        target_compile_options(mmo_character_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_options(mmo_camera_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_options(mmo_controller_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_options(mmo_animation_layers_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_options(mmo_combat_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_options(mmo_terrain_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_options(mmo_world_editor_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    endif()
    target_compile_options(glad PRIVATE /W3 /utf-8)
endif()

set_target_properties(mmo_client PROPERTIES
    OUTPUT_NAME "mmo_client"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
