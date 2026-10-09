#include "assets/gltf_model_loader.hpp"
#include "character/character.hpp"
#include "character/character_controller.hpp"
#include "character/combat.hpp"
#include "game/world/collision_world.hpp"
#include "game/world/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

mmo::character::Character make_player(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    return mmo::character::Character(model);
}

void bind_movement_animations(mmo::character::Character& player)
{
    using namespace mmo;
    auto& animation = player.model().animation();
    const auto& clips = player.model().body().model().animations;
    const auto bind = [&animation, &clips](
        animation::AnimationState state, const char* name) {
        for (std::size_t index = 0; index < clips.size(); ++index) {
            if (clips[index].name == name) {
                return animation.bind_state(state, index);
            }
        }
        return false;
    };
    check(bind(animation::AnimationState::Idle, "Idle_Loop"),
        "Idle animation is missing");
    check(bind(animation::AnimationState::Walk, "Walk_Loop"),
        "Walk animation is missing");
    check(bind(animation::AnimationState::Run, "Jog_Fwd_Loop"),
        "Run animation is missing");
    check(bind(animation::AnimationState::Jump, "Jump_Loop"),
        "Jump animation is missing");
    check(animation.set_state(animation::AnimationState::Idle, 0.0f),
        "Could not start idle animation");
}

void test_camera_relative_and_normalized_movement(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    using namespace mmo;
    game::world::CollisionWorld world;
    auto first = make_player(model);
    auto second = make_player(model);
    auto turning_player = make_player(model);
    bind_movement_animations(first);
    bind_movement_animations(second);
    bind_movement_animations(turning_player);
    character::MovementSettings settings;
    settings.acceleration = 100.0f;
    character::CharacterController straight(first, settings);
    character::CharacterController diagonal(second, settings);
    character::CharacterController turning(turning_player);
    straight.update(0.1f, {1.0f, 0.0f, false}, 3.14159265f, world);
    diagonal.update(0.1f, {1.0f, 1.0f, false}, 3.14159265f, world);
    turning.update(0.1f, {0.0f, -1.0f, false}, 0.0f, world);
    const auto& straight_position = first.transform().position;
    const auto& diagonal_position = second.transform().position;
    const float straight_distance = std::hypot(
        straight_position[0], straight_position[2]);
    const float diagonal_distance = std::hypot(
        diagonal_position[0], diagonal_position[2]);
    check(std::abs(straight_distance - diagonal_distance) < 0.0001f,
        "diagonal movement was faster than forward movement");
    check(straight_position[2] > 0.0f,
        "forward movement did not follow camera direction");
    check(std::abs(straight_position[0]) < 0.0001f,
        "forward movement drifted sideways");
    check(turning.yaw() < 0.0f && turning.yaw() > -1.5707963f,
        "character did not smoothly rotate toward its movement direction");
}

void test_acceleration_jump_and_world_collision(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    using namespace mmo;
    const game::world::CollisionWorld world(
        {},
        {game::world::CollisionBox{
            {-1.0f, 0.0f, -1.0f}, {-0.5f, 3.0f, 1.0f}}});
    auto player = make_player(model);
    bind_movement_animations(player);
    character::MovementSettings settings;
    settings.acceleration = 100.0f;
    settings.turn_speed = 100.0f;
    character::CharacterController controller(player, settings);
    for (int frame = 0; frame < 12; ++frame) {
        controller.update(0.1f, {0.0f, -1.0f, false}, 0.0f, world);
    }
    check(player.transform().position[0] < 0.0f,
        "strafe-left movement did not follow the camera");
    check(player.transform().position[0] > -1.4f,
        "wall collision allowed the character to pass through a collider");

    auto jumper = make_player(model);
    bind_movement_animations(jumper);
    character::CharacterController jumping(jumper, settings);
    jumping.update(0.1f, {0.0f, 0.0f, true}, 3.14159265f, world);
    check(!jumping.grounded() && jumper.transform().position[1] > 0.0f,
        "jump input did not lift the character");
    bool falling_seen = false;
    for (int frame = 0; frame < 12; ++frame) {
        jumping.update(0.1f, {}, 3.14159265f, world);
        falling_seen = falling_seen ||
            jumping.state() == character::CharacterMovementState::Falling;
    }
    check(falling_seen, "the Falling movement state was never entered");
    check(jumping.grounded() && jumper.transform().position[1] == 0.0f,
        "gravity did not return the character to the ground");
    check(jumping.state() == character::CharacterMovementState::Idle,
        "grounded stationary character did not return to Idle");
}

void test_speed_settings_and_animation_pack(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    using namespace mmo;
    game::world::CollisionWorld world;
    auto walker = make_player(model);
    auto runner = make_player(model);
    bind_movement_animations(walker);
    bind_movement_animations(runner);
    character::MovementSettings walk_settings;
    walk_settings.acceleration = 100.0f;
    character::MovementSettings run_settings = walk_settings;
    run_settings.run_speed = 6.0f;
    character::CharacterController walking(walker, walk_settings);
    character::CharacterController running(runner, run_settings);
    walking.update(0.1f, {1.0f, 0.0f, false}, 3.14159265f, world);
    running.update(0.1f, {1.0f, 0.0f, false}, 3.14159265f, world);
    check(runner.transform().position[2] > walker.transform().position[2],
        "run speed setting did not increase forward movement");
    check(running.state() == character::CharacterMovementState::Running,
        "running input did not select the Running state");

    for (const character::CharacterControllerInput direction : {
             character::CharacterControllerInput{0.0f, -1.0f, false},
             character::CharacterControllerInput{1.0f, 1.0f, false},
             character::CharacterControllerInput{0.0f, 1.0f, false}}) {
        auto directional_runner = make_player(model);
        bind_movement_animations(directional_runner);
        character::CharacterController directional_controller(
            directional_runner, run_settings);
        directional_controller.update(0.1f, direction, 3.14159265f, world);
        check(directional_controller.state() == character::CharacterMovementState::Running,
            "Every non-backward direction did not select the Running state for every direction");
        const auto sprint_clip = std::find_if(
            model->animations.begin(), model->animations.end(),
            [](const animation::AnimationClip& clip) {
                return clip.name == "Jog_Fwd_Loop";
            });
        check(sprint_clip != model->animations.end() &&
                directional_runner.model().animation().animator().active_clip() ==
                    static_cast<std::size_t>(
                        std::distance(model->animations.begin(), sprint_clip)),
            "Every non-backward direction did not select the jog animation for every direction");
    }

    auto slow_player = make_player(model);
    bind_movement_animations(slow_player);
    character::MovementSettings slow_settings = walk_settings;
    slow_settings.acceleration = 1.0f;
    character::CharacterController slow(slow_player, slow_settings);
    slow.update(0.1f, {1.0f, 0.0f, false}, 3.14159265f, world);
    check(walker.transform().position[2] > slow_player.transform().position[2],
        "acceleration setting did not affect startup response");

    auto backward_player = make_player(model);
    bind_movement_animations(backward_player);
    character::CharacterController backward(backward_player, walk_settings);
    backward.update(0.1f, {-1.0f, 0.0f, false}, 3.14159265f, world);
    check(backward_player.transform().position[2] < 0.0f,
        "backward movement did not use the camera-relative reverse direction");

    auto preview_character = make_player(model);
    bind_movement_animations(preview_character);
    character::CharacterController preview(preview_character);
    for (const animation::AnimationClip& clip : model->animations) {
        check(preview.preview_animation(clip.name),
            "a loaded animation could not be previewed");
    }
    check(model->animations.size() == 43,
        "Universal Animation Library clip count changed unexpectedly");
}

void test_controller_follows_sculpted_ground(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    using namespace mmo;
    game::world::CollisionWorld collision_world;
    game::world::Terrain terrain;
    check(terrain.apply_brush(
            {0.0f, 0.0f}, 4.0f, 2.0f, 1.0f, game::world::TerrainBrush::raise),
        "terrain setup did not sculpt a hill");
    auto player = make_player(model);
    bind_movement_animations(player);
    character::CharacterController controller(player);

    controller.update(0.016f, {}, 3.14159265f, collision_world, &terrain);
    const float ground_height = terrain.height_at({0.0f, 0.0f});
    check(std::abs(player.transform().position[1] - ground_height) < 0.0001f,
        "grounded character did not follow the sculpted terrain");

    controller.update(0.1f, {0.0f, 0.0f, true},
        3.14159265f, collision_world, &terrain);
    check(!controller.grounded() && player.transform().position[1] > ground_height,
        "character did not jump from the sculpted ground height");
    for (int frame = 0; frame < 20 && !controller.grounded(); ++frame) {
        controller.update(0.1f, {}, 3.14159265f, collision_world, &terrain);
    }
    check(controller.grounded() &&
            std::abs(player.transform().position[1] - ground_height) < 0.0001f,
        "character did not land back on the sculpted terrain");
}

void test_walk_and_attack_are_layered(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    using namespace mmo;
    game::world::CollisionWorld collision_world;
    auto player = make_player(model);
    bind_movement_animations(player);
    character::CharacterController controller(player);
    character::CombatSystem combat_system;
    character::CombatController combat(player, character::default_combat_actions());
    combat.set_event_handler([&combat_system](const character::CombatEvent& event) {
        combat_system.handle(event);
    });

    const character::CharacterControllerInput walk{1.0f, 0.0f, false};
    for (int frame = 0; frame < 30; ++frame) {
        controller.update(0.016f, walk, 0.0f, collision_world);
    }
    check(controller.movement_state() == character::MovementState::Run, "expected Run state");
    const std::size_t walk_clip = player.model().animation().animator().active_clip();
    const float start_z = player.transform().position[2];

    check(combat.request(character::CombatState::Attack), "attack must start while walking");
    bool upper_active_while_walking = false;
    for (int frame = 0; frame < 30; ++frame) {
        controller.update(0.016f, walk, 0.0f, collision_world);
        combat.update();
        upper_active_while_walking = upper_active_while_walking ||
            player.model().animation().mixer().upper_weight() > 0.9f;
        check(player.model().animation().animator().active_clip() == walk_clip,
            "locomotion clip must not change when attacking");
        check(controller.movement_state() == character::MovementState::Run,
            "movement state must not change when attacking");
    }
    check(upper_active_while_walking, "upper layer must reach full weight");
    check(player.transform().position[2] < start_z - 0.1f, "character must keep moving");

    // Change direction/speed mid-attack: the attack must not restart or cancel.
    const character::CharacterControllerInput run{1.0f, 0.0f, false};
    for (int frame = 0; frame < 60; ++frame) {
        controller.update(0.016f, run, 0.0f, collision_world);
        combat.update();
    }
    check(controller.movement_state() == character::MovementState::Run, "expected Run state");
    check(combat_system.attacks_started() == 1 && combat_system.hits() == 1,
        "attack must start and hit exactly once");
    check(combat.state() != character::CombatState::None ||
            combat_system.attacks_finished() == 1,
        "combat state must be consistent");
    for (int frame = 0; frame < 60; ++frame) {
        controller.update(0.016f, {}, 0.0f, collision_world);
        combat.update();
    }
    check(combat_system.attacks_finished() == 1 && combat.state() == character::CombatState::None,
        "attack must end and return to None");
    check(!player.model().animation().mixer().active(), "upper layer must blend out");
    check(controller.movement_state() == character::MovementState::Idle, "expected Idle afterwards");
}

void test_combat_stance_uses_melee_strafe_clips(
    mmo::assets::AssetSystem& asset_system,
    const std::shared_ptr<const mmo::assets::Model>& base_model)
{
    using namespace mmo;
    assets::Model combined = *base_model;
    assets::append_animations(combined, asset_system.load_model(
        "assets/animations/generated/melee_combat_ual.glb"));
    check(combined.animations.size() > base_model->animations.size(),
        "melee combat clips were not merged");
    const auto combined_model = std::make_shared<const assets::Model>(std::move(combined));
    game::world::CollisionWorld world;

    const auto active_clip_name = [&combined_model](character::Character& player) {
        return combined_model->animations.at(
            player.model().animation().animator().active_clip()).name;
    };
    struct Case {
        character::CharacterControllerInput input;
        const char* clip;
    };
    // The target is straight ahead of the character (+Z) and the camera yaw is pi, so the
    // camera looks toward +Z, so strafe +1 (camera right, -X) is the character's right.
    const Case cases[] = {
        {{1.0f, 0.0f, false}, "Melee_Run_Forward"},
        {{-1.0f, 0.0f, false}, "Melee_Run_Backward"},
        {{0.0f, -1.0f, false}, "Melee_StrafeRun_Left"},
        {{0.0f, 1.0f, false}, "Melee_StrafeRun_Right"},
        {{1.0f, -1.0f, false}, "Melee_StrafeRun_ForwardLeft"},
        {{1.0f, 1.0f, false}, "Melee_StrafeRun_ForwardRight"},
        {{-1.0f, -1.0f, false}, "Melee_StrafeRun_BackwardLeft"},
        {{-1.0f, 1.0f, false}, "Melee_StrafeRun_BackwardRight"},
    };
    for (const Case& test_case : cases) {
        auto player = make_player(combined_model);
        bind_movement_animations(player);
        character::CharacterController controller(player);
        controller.set_facing_target(0.0f);
        check(controller.combat_stance(), "facing target did not enable the combat stance");
        for (int frame = 0; frame < 20; ++frame) {
            controller.update(0.016f, test_case.input, 3.14159265f, world);
        }
        check(active_clip_name(player) == test_case.clip,
            (std::string("combat movement selected ") + active_clip_name(player) +
                " instead of " + test_case.clip).c_str());
    }

    auto idle_player = make_player(combined_model);
    bind_movement_animations(idle_player);
    character::CharacterController idle(idle_player);
    idle.set_facing_target(0.0f);
    idle.update(0.016f, {}, 0.0f, world);
    check(active_clip_name(idle_player) == "Melee_CombatIdle",
        "standing in combat did not select the combat idle");
    idle.set_facing_target(std::nullopt);
    idle.update(0.016f, {}, 0.0f, world);
    check(active_clip_name(idle_player) == "Idle_Loop",
        "leaving combat did not restore the normal idle");

    auto speed_player = make_player(combined_model);
    bind_movement_animations(speed_player);
    character::CharacterController speed_controller(speed_player);
    speed_controller.set_facing_target(0.0f);
    for (int frame = 0; frame < 120; ++frame) {
        speed_controller.update(0.016f, {-1.0f, 0.0f, false}, 3.14159265f, world);
    }
    const float travelled = std::abs(speed_player.transform().position[2]);
    check(travelled > 0.0f, "backpedalling in combat did not move the character");
}

}

int main()
{
    try {
        mmo::assets::GltfModelLoader loader;
        mmo::assets::AssetSystem asset_system(loader);
        auto model = std::make_shared<const mmo::assets::Model>(asset_system.load_model(
            "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb"));
        test_camera_relative_and_normalized_movement(model);
        test_acceleration_jump_and_world_collision(model);
        test_speed_settings_and_animation_pack(model);
        test_controller_follows_sculpted_ground(model);
        test_walk_and_attack_are_layered(model);
        test_combat_stance_uses_melee_strafe_clips(asset_system, model);
        std::cout << "CharacterControllerTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CharacterControllerTest failed: " << error.what() << '\n';
        return 1;
    }
}
