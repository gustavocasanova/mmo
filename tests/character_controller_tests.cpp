#include "assets/gltf_model_loader.hpp"

#include "character/character.hpp"
#include "character/character_controller.hpp"
#include "character/combat.hpp"
#include "game/world/collision_world.hpp"
#include "game/world/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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

void test_camera_relative_controls_across_rotations(
    const std::shared_ptr<const mmo::assets::Model>& model)
{
    using namespace mmo;
    game::world::CollisionWorld world;
    constexpr float pi = 3.14159265f;
    constexpr float diagonal = 0.70710678f;
    struct Case {
        float camera_yaw;
        character::CharacterControllerInput input;
        float expected_x;
        float expected_z;
    };
    const Case directions[] = {
        {pi, {1.0f, 0.0f, false}, 0.0f, 1.0f},
        {pi, {0.0f, 1.0f, false}, -1.0f, 0.0f},
        {pi, {-1.0f, 0.0f, false}, 0.0f, -1.0f},
        {pi, {0.0f, -1.0f, false}, 1.0f, 0.0f},
        {pi * 0.5f, {1.0f, 0.0f, false}, -1.0f, 0.0f},
        {pi * 0.5f, {0.0f, 1.0f, false}, 0.0f, -1.0f},
        {pi * 0.5f, {-1.0f, 0.0f, false}, 1.0f, 0.0f},
        {pi * 0.5f, {0.0f, -1.0f, false}, 0.0f, 1.0f},
        {0.0f, {1.0f, 0.0f, false}, 0.0f, -1.0f},
        {-pi * 0.5f, {1.0f, 0.0f, false}, 1.0f, 0.0f},
        {pi * 3.0f, {1.0f, 0.0f, false}, 0.0f, 1.0f},
        {pi, {1.0f, 1.0f, false}, -diagonal, diagonal},
        {pi, {1.0f, -1.0f, false}, diagonal, diagonal},
        {pi, {-1.0f, 1.0f, false}, -diagonal, -diagonal},
        {pi, {-1.0f, -1.0f, false}, diagonal, -diagonal},
        {pi * 0.5f, {1.0f, 1.0f, false}, -diagonal, -diagonal},
        {pi * 0.5f, {1.0f, -1.0f, false}, -diagonal, diagonal},
        {pi * 0.5f, {-1.0f, 1.0f, false}, diagonal, -diagonal},
        {pi * 0.5f, {-1.0f, -1.0f, false}, diagonal, diagonal},
    };

    const auto check_direction = [&](const Case& test_case, bool face_target) {
        auto player = make_player(model);
        bind_movement_animations(player);
        character::MovementSettings settings;
        settings.acceleration = 100.0f;
        character::CharacterController controller(player, settings);
        if (face_target) {
            controller.set_facing_target(0.0f);
        }
        controller.update(0.1f, test_case.input, test_case.camera_yaw, world);

        const float speed = face_target && test_case.input.forward < 0.0f
            ? settings.combat_backward_speed : settings.run_speed;
        const float expected_distance = speed * 0.1f;
        const auto& position = player.transform().position;
        check(std::abs(position[0] - test_case.expected_x * expected_distance) < 0.002f &&
                std::abs(position[2] - test_case.expected_z * expected_distance) < 0.002f,
            "WASD movement did not follow the camera's horizontal yaw");
        if (face_target) {
            check(std::abs(controller.yaw()) < 0.0001f,
                "movement direction incorrectly replaced the combat facing target");
        }
    };

    for (const Case& test_case : directions) {
        check_direction(test_case, false);
    }
    const Case focus_target_directions[] = {
        {pi * 0.5f, {1.0f, 0.0f, false}, -1.0f, 0.0f},
        {pi * 0.5f, {0.0f, 1.0f, false}, 0.0f, -1.0f},
        {pi * 0.5f, {-1.0f, 0.0f, false}, 1.0f, 0.0f},
        {pi * 0.5f, {0.0f, -1.0f, false}, 0.0f, 1.0f},
        {pi * 0.5f, {1.0f, 1.0f, false}, -diagonal, -diagonal},
        {pi * 0.5f, {1.0f, -1.0f, false}, -diagonal, diagonal},
        {pi * 0.5f, {-1.0f, 1.0f, false}, diagonal, -diagonal},
        {pi * 0.5f, {-1.0f, -1.0f, false}, diagonal, diagonal},
    };
    for (const Case& test_case : focus_target_directions) {
        check_direction(test_case, true);
    }
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
    const auto forward_clip = std::find_if(
        model->animations.begin(), model->animations.end(),
        [](const animation::AnimationClip& clip) {
            return clip.name == "Jog_Fwd_Loop";
        });
    check(forward_clip != model->animations.end() &&
            backward_player.model().animation().animator().active_clip() ==
                static_cast<std::size_t>(
                    std::distance(model->animations.begin(), forward_clip)),
        "non-combat reverse input must use the normal forward animation");

    auto forward_player = make_player(model);
    bind_movement_animations(forward_player);
    character::CharacterController forward(forward_player, walk_settings);
    forward.update(0.1f, {1.0f, 0.0f, false}, 3.14159265f, world);
    check(backward_player.model().animation().animator().active_clip() ==
            forward_player.model().animation().animator().active_clip(),
        "non-combat reverse input must use the same animation as forward input");
    check(std::abs(backward_player.transform().position[2]) ==
            std::abs(forward_player.transform().position[2]),
        "non-combat reverse input must use the same speed as forward input");

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

    const character::CharacterControllerInput forward{1.0f, 0.0f, false};
    for (int frame = 0; frame < 10; ++frame) {
        controller.update(0.016f, forward, 0.0f, collision_world);
    }
    check(controller.movement_state() == character::MovementState::Run, "expected Run state");
    const auto& clips = player.model().body().model().animations;
    const float start_x = player.transform().position[0];
    const float start_z = player.transform().position[2];

    check(combat.request(character::CombatState::Attack), "attack must start while walking");
    bool upper_active_while_walking = false;
    const character::CharacterControllerInput directions[] = {
        {1.0f, 0.0f, false},
        {0.0f, -1.0f, false},
        {0.0f, 1.0f, false},
        {-1.0f, 0.0f, false},
    };
    for (const auto& direction : directions) {
        for (int frame = 0; frame < 10; ++frame) {
            controller.update(0.016f, direction, 0.0f, collision_world);
            combat.update();
            upper_active_while_walking = upper_active_while_walking ||
                player.model().animation().mixer().upper_weight() > 0.9f;
            const std::string_view expected_locomotion = "Jog_Fwd_Loop";
            const std::string_view active_locomotion =
                clips[player.model().animation().animator().active_clip()].name;
            check(active_locomotion == expected_locomotion ||
                    (expected_locomotion == "Jog_Fwd_Loop" &&
                        active_locomotion == "Walk_Loop"),
                (std::string("locomotion layer selected ") +
                    std::string(active_locomotion) + " instead of " +
                    std::string(expected_locomotion) +
                    " while attacking").c_str());
            check(controller.movement_state() != character::MovementState::Idle,
                "movement input must continue during an attack");
            check(combat.state() == character::CombatState::Attack ||
                    combat_system.attacks_finished() == 1,
                "changing movement direction must not cancel the attack");
        }
    }
    check(upper_active_while_walking, "upper layer must reach full weight");
    check(std::hypot(player.transform().position[0] - start_x,
            player.transform().position[2] - start_z) > 0.1f,
        "character must keep moving while changing directions during an attack");

    for (int frame = 0; frame < 60; ++frame) {
        controller.update(0.016f, {}, 0.0f, collision_world);
        combat.update();
    }
    check(controller.movement_state() == character::MovementState::Idle, "expected Idle state");
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
    std::filesystem::path melee_clips{
        "assets/animations/generated/melee_combat_ual.glb"};
    if (!std::filesystem::exists(melee_clips)) {
        melee_clips = "assets/animations/generated/combat_strafe_ual.glb";
    }
    if (!std::filesystem::exists(melee_clips)) {
        return;
    }
    assets::Model combined = *base_model;
    assets::append_animations(combined, asset_system.load_model(melee_clips));
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
        {{-1.0f, -1.0f, false}, "Melee_StrafeRun_Left"},
        {{-1.0f, 1.0f, false}, "Melee_StrafeRun_Right"},
    };
    for (const Case& test_case : cases) {
        if (std::none_of(combined_model->animations.begin(), combined_model->animations.end(),
                [&test_case](const animation::AnimationClip& clip) {
                    return clip.name == test_case.clip;
                })) {
            continue;
        }
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

    auto strafing_player = make_player(combined_model);
    bind_movement_animations(strafing_player);
    character::CharacterController strafing_controller(strafing_player);
    strafing_controller.set_facing_target(0.0f);
    character::CombatController attack(
        strafing_player, character::default_combat_actions());
    const character::CharacterControllerInput left_strafe{0.0f, -1.0f, false};
    strafing_controller.update(0.016f, left_strafe, 3.14159265f, world);
    check(strafing_player.model().animation().lower_clip_name() == "Melee_StrafeRun_Left",
        "real melee left strafe clip must drive the lower-body layer");
    const float start_x = strafing_player.transform().position[0];
    const float start_z = strafing_player.transform().position[2];
    check(attack.request(character::CombatState::Attack),
        "attack must start while using the real melee strafe clip");
    for (int frame = 0; frame < 12; ++frame) {
        strafing_controller.update(0.016f, left_strafe, 3.14159265f, world);
        attack.update();
    }
    check(strafing_player.model().animation().lower_clip_name() == "Melee_StrafeRun_Left",
        "attack must preserve the real left strafe lower-body clip");
    check(strafing_player.model().animation().upper_clip_name() == "Punch_Jab" &&
            strafing_player.model().animation().mixer().upper_weight() > 0.9f,
        "attack must blend in the upper-body layer over the real strafe");
    check(std::hypot(strafing_player.transform().position[0] - start_x,
            strafing_player.transform().position[2] - start_z) > 0.1f,
        "player must keep moving while the real strafe and attack are blended");

    const character::CharacterControllerInput right_strafe{0.0f, 1.0f, false};
    for (int frame = 0; frame < 6; ++frame) {
        strafing_controller.update(0.016f, right_strafe, 3.14159265f, world);
        attack.update();
    }
    check(strafing_player.model().animation().lower_clip_name() == "Melee_StrafeRun_Right",
        "changing direction during an attack must select the real right strafe clip");
    check(attack.state() == character::CombatState::Attack &&
            strafing_player.model().animation().upper_clip_name() == "Punch_Jab",
        "changing strafe direction must not interrupt the upper-body attack");

    auto idle_player = make_player(combined_model);
    bind_movement_animations(idle_player);
    character::CharacterController idle(idle_player);
    idle.set_facing_target(0.0f);
    idle.update(0.016f, {}, 0.0f, world);
    const bool has_combat_idle = std::any_of(combined_model->animations.begin(),
        combined_model->animations.end(), [](const animation::AnimationClip& clip) {
            return clip.name == "Melee_CombatIdle";
        });
    check(active_clip_name(idle_player) ==
            (has_combat_idle ? "Melee_CombatIdle" : "Idle_Loop"),
        "combat idle selection did not gracefully handle a partial animation pack");
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

    auto diagonal_player = make_player(combined_model);
    bind_movement_animations(diagonal_player);
    character::CharacterController diagonal_controller(diagonal_player);
    diagonal_controller.set_facing_target(0.0f);
    for (int frame = 0; frame < 120; ++frame) {
        diagonal_controller.update(0.016f, {-1.0f, 1.0f, false},
            3.14159265f, world);
    }
    check(std::hypot(diagonal_player.transform().position[0],
            diagonal_player.transform().position[2]) <
            travelled * 1.1f,
        "combat diagonal retreat must keep the slow combat speed");
}

void test_imported_weapon_animations(
    mmo::assets::AssetSystem& asset_system,
    const std::shared_ptr<const mmo::assets::Model>& base_model)
{
    using namespace mmo;
    const std::filesystem::path weapon_clips{
        "assets/animations/generated/weapon_animations_ual.glb"};
    if (!std::filesystem::exists(weapon_clips)) {
        return;
    }
    assets::Model combined = *base_model;
    assets::append_animations(combined, asset_system.load_model(weapon_clips));
    const auto combined_model = std::make_shared<const assets::Model>(std::move(combined));
    game::world::CollisionWorld world;

    auto archer = make_player(combined_model);
    bind_movement_animations(archer);
    character::CharacterController archer_controller(archer);
    archer_controller.set_locomotion_animation_set(character::LocomotionAnimationSet{
        "Longbow_Idle", "Longbow_Walk_Forward", "Longbow_Walk_Backward",
        "Longbow_Walk_Left", "Longbow_Walk_Right", "Longbow_Run_Forward",
        "Longbow_Run_Backward", "Longbow_Run_Left", "Longbow_Run_Right"});
    const character::CharacterControllerInput running{1.0f, 0.0f, false};
    archer_controller.update(0.016f, running, 3.14159265f, world);
    for (int frame = 0; frame < 30; ++frame) {
        archer_controller.update(0.1f, running, 3.14159265f, world);
    }
    check(archer.model().animation().lower_clip_name() == "Longbow_Run_Forward",
        "archer did not use the longbow run animation");

    auto attacker = make_player(combined_model);
    bind_movement_animations(attacker);
    character::CharacterController attacker_controller(attacker);
    character::CombatController combat(attacker, {});
    std::vector<character::CombatActionDefinition> attacks;
    for (const std::string_view name : {
             "Greatsword_Slash_01", "Greatsword_Slash_02"}) {
        const auto clip = std::find_if(combined_model->animations.begin(),
            combined_model->animations.end(), [name](const animation::AnimationClip& item) {
                return item.name == name;
            });
        check(clip != combined_model->animations.end(),
            "greatsword attack variation is missing from imported clips");
        if (clip != combined_model->animations.end()) {
            attacks.push_back({character::CombatState::Attack, std::string(name),
                clip->duration, 1.0f, clip->duration * 0.5f, 0.1f, 0.15f});
        }
    }
    combat.set_actions(std::move(attacks));
    for (const std::string_view expected : {
             "Greatsword_Slash_01", "Greatsword_Slash_02"}) {
        check(combat.request(character::CombatState::Attack),
            "next attack variation did not start");
        check(attacker.model().animation().upper_clip_name() == expected,
            "attack variations did not advance in order");
        for (int frame = 0; frame < 600 &&
                combat.state() != character::CombatState::None; ++frame) {
            attacker_controller.update(0.016f, {}, 3.14159265f, world);
            combat.update();
        }
        check(combat.state() == character::CombatState::None,
            "attack variation did not complete");
    }
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
        test_camera_relative_controls_across_rotations(model);
        test_acceleration_jump_and_world_collision(model);
        test_speed_settings_and_animation_pack(model);
        test_controller_follows_sculpted_ground(model);
        test_walk_and_attack_are_layered(model);
        test_combat_stance_uses_melee_strafe_clips(asset_system, model);
        test_imported_weapon_animations(asset_system, model);
        std::cout << "CharacterControllerTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CharacterControllerTest failed: " << error.what() << '\n';
        return 1;
    }
}
