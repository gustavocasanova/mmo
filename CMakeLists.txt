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

# Layers (lower layers never link or include higher ones; LayeringTest checks includes):
#   engine:       mmo_core, mmo_world, mmo_animation, mmo_assets, mmo_equipment,
#                 mmo_character, mmo_scene, mmo_editor  (+ renderer/platform in mmo_client)
#   game rules:   mmo_game          -> mmo_core, mmo_world, GLM only; what a server links
#   game client:  mmo_presentation  -> mmo_game + engine; item visuals and loadout mirroring
#   composition:  mmo_client

add_library(mmo_core STATIC
    src/core/key_value_file.cpp)
target_include_directories(mmo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mmo_core PUBLIC cxx_std_20)

add_library(mmo_world STATIC
    src/world/collision_world.cpp
    src/world/terrain.cpp)
target_include_directories(mmo_world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mmo_world PUBLIC cxx_std_20)
target_link_libraries(mmo_world PUBLIC glm::glm)

add_library(mmo_animation STATIC
    src/animation/animation_controller.cpp
    src/animation/skeleton.cpp
    src/animation/layered_animation.cpp)
target_include_directories(mmo_animation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(mmo_animation PUBLIC cxx_std_20)
target_link_libraries(mmo_animation PUBLIC glm::glm)

add_library(mmo_assets STATIC
    src/assets/gltf_model_loader.cpp
    src/assets/model.cpp
    src/assets/static_model_geometry.cpp)
target_include_directories(mmo_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_assets PUBLIC mmo_animation PRIVATE fastgltf::fastgltf)

add_library(mmo_equipment STATIC
    src/equipment/equipment_manager.cpp
    src/equipment/attachment_pose.cpp
    src/equipment/weapon.cpp)
target_include_directories(mmo_equipment PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_equipment PUBLIC mmo_animation)

add_library(mmo_character STATIC
    src/character/combat.cpp
    src/character/character.cpp
    src/character/character_controller.cpp
    src/character/character_body.cpp
    src/character/character_model.cpp)
target_include_directories(mmo_character PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_character PUBLIC mmo_assets mmo_equipment mmo_world)

add_library(mmo_scene STATIC
    src/scene/camera.cpp
    src/scene/camera_controller.cpp)
target_include_directories(mmo_scene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_scene PUBLIC mmo_world)

add_library(mmo_editor STATIC
    src/editor/asset_database.cpp
    src/editor/editor_camera.cpp
    src/editor/editor_history.cpp
    src/editor/selection.cpp
    src/editor/world_document.cpp
    src/editor/world_editor.cpp)
target_include_directories(mmo_editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_editor PUBLIC mmo_scene)

add_library(mmo_game STATIC
    src/game/combat/combat.cpp
    src/game/items/item_catalog.cpp
    src/game/items/inventory.cpp)
target_include_directories(mmo_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_game PUBLIC mmo_core mmo_world glm::glm)

add_library(mmo_presentation STATIC
    src/presentation/item_visuals.cpp)
target_include_directories(mmo_presentation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(mmo_presentation PUBLIC mmo_game mmo_equipment)

add_executable(mmo_client
    src/apps/client/main.cpp
    src/apps/client/application.cpp
    src/apps/client/character_setup.cpp
    src/apps/client/demo_enemies.cpp
    src/apps/client/inventory_window.cpp
    src/apps/client/player_items.cpp
    src/apps/client/target_frame.cpp
    src/apps/client/character_animation_setup.cpp
    src/presentation/equipment_presenter.cpp
    src/editor/editor_ui.cpp
    src/platform/window.cpp
    src/renderer/renderer.cpp
    src/renderer/opengl/mesh.cpp
    src/renderer/opengl/shader.cpp
    src/apps/client/combat_camera_controller.cpp
)

target_include_directories(mmo_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(mmo_client PRIVATE
    glfw
    glm::glm
    glad
    mmo_character
    mmo_game
    mmo_presentation
    mmo_scene
    mmo_editor
    imgui::imgui)

set(MMO_WARNING_TARGETS
    mmo_core mmo_world mmo_animation mmo_assets mmo_equipment mmo_character
    mmo_scene mmo_editor mmo_game mmo_presentation mmo_client)

if(BUILD_TESTING)
    function(mmo_add_test name source)
        string(MAKE_C_IDENTIFIER "${name}" target)
        set(target "mmo_test_${target}")
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE ${ARGN})
        add_test(NAME ${name} COMMAND ${target})
        # Tests that read content/ or the UAL rig run from the repository root.
        set_tests_properties(${name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND MMO_WARNING_TARGETS ${target})
        set(MMO_WARNING_TARGETS "${MMO_WARNING_TARGETS}" PARENT_SCOPE)
    endfunction()

    mmo_add_test(WoodenSwordTest tests/wooden_sword_test.cpp mmo_character mmo_presentation)
    mmo_add_test(ItemRulesTest tests/item_rules_test.cpp mmo_game)
    mmo_add_test(CharacterEquipmentTest tests/character_equipment_test.cpp mmo_character)
    mmo_add_test(ThirdPersonCameraTest tests/camera_tests.cpp mmo_scene mmo_game)
    target_sources(mmo_test_ThirdPersonCameraTest PRIVATE
        src/apps/client/combat_camera_controller.cpp)
    mmo_add_test(CharacterControllerTest tests/character_controller_tests.cpp mmo_character)
    mmo_add_test(AnimationLayersTest tests/animation_layers_test.cpp mmo_animation)
    mmo_add_test(CombatTest tests/combat_test.cpp mmo_game)
    mmo_add_test(TerrainTest tests/terrain_test.cpp mmo_world)
    mmo_add_test(WorldEditorModeTest tests/world_editor_mode_test.cpp mmo_editor mmo_assets)

    add_test(NAME LayeringTest
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_layering.cmake)
endif()

if(WIN32)
    target_link_libraries(mmo_client PRIVATE opengl32)
endif()

if(MSVC)
    foreach(target IN LISTS MMO_WARNING_TARGETS)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    endforeach()
    target_compile_options(glad PRIVATE /W3 /utf-8)
endif()

set_target_properties(mmo_client PROPERTIES
    OUTPUT_NAME "mmo_client"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
