#pragma once

#include "character/character.hpp"
#include "world/collision_world.hpp"
#include "world/terrain.hpp"

#include <optional>
#include <string_view>

namespace mmo::character {

enum class CharacterMovementState {
    Idle,
    Walking,
    Running,
    Jumping,
    Falling,
};

// Drives the lower-body animation layer; independent of the combat state.
enum class MovementState {
    Idle,
    Walk,
    Run,
    StrafeLeft,
    StrafeRight,
    Backward,
};

[[nodiscard]] std::string_view to_string(MovementState state);

struct MovementSettings {
    float walk_speed = 2.2f;
    float run_speed = 4.2f;
    float backward_speed = 1.8f;
    float strafe_speed = 2.0f;
    float combat_backward_speed = 3.0f;
    float acceleration = 14.0f;
    float deceleration = 18.0f;
    float air_control = 0.25f;
    float jump_speed = 5.0f;
    float gravity = 12.0f;
    float turn_speed = 9.0f;
    float collision_radius = 0.35f;
    float collision_height = 1.8f;
    bool rotate_character_to_movement = true;
};

struct CharacterControllerInput {
    float forward = 0.0f;
    float strafe = 0.0f;
    bool jump_pressed = false;
};

struct LocomotionAnimationSet {
    std::string_view idle;
    std::string_view walk_forward;
    std::string_view walk_backward;
    std::string_view walk_left;
    std::string_view walk_right;
    std::string_view run_forward;
    std::string_view run_backward;
    std::string_view run_left;
    std::string_view run_right;
};

class CharacterController {
public:
    explicit CharacterController(
        Character& character,
        MovementSettings settings = {});

    void update(
        float delta_seconds,
        CharacterControllerInput input,
        float camera_yaw,
        const world::CollisionWorld& collision_world,
        const world::Terrain* terrain = nullptr);
    [[nodiscard]] bool preview_animation(std::string_view name);
    void set_locomotion_animation_set(
        std::optional<LocomotionAnimationSet> animations);
    [[nodiscard]] CharacterMovementState state() const;
    // Optional yaw the character keeps facing (e.g. a combat target) instead of the move direction.
    void set_facing_target(std::optional<float> yaw);
    // Radians per second used to turn toward the facing target; 0 keeps the smooth default turn.
    void set_facing_rotation_speed(float radians_per_second);
    // True while a facing target is set (combat): movement uses combat-aware locomotion clips.
    [[nodiscard]] bool combat_stance() const;
    [[nodiscard]] MovementState movement_state() const;
    [[nodiscard]] bool grounded() const;
    [[nodiscard]] float vertical_velocity() const;
    [[nodiscard]] float yaw() const;
    [[nodiscard]] const MovementSettings& settings() const;

private:
    void update_animation(const CharacterControllerInput& input);
    void select_locomotion_animation(const CharacterControllerInput& input);
    bool select_combat_animation(const CharacterControllerInput& input, bool moving);
    bool play_loop(std::string_view clip, bool keep_phase = false);

    Character& character_;
    MovementSettings settings_;
    CharacterMovementState state_ = CharacterMovementState::Idle;
    MovementState movement_state_ = MovementState::Idle;
    std::optional<float> facing_target_;
    std::optional<LocomotionAnimationSet> locomotion_animations_;
    float facing_rotation_speed_ = 0.0f;
    math::Vec3 horizontal_velocity_{0.0f};
    float vertical_velocity_ = 0.0f;
    float yaw_ = 0.0f;
    bool grounded_ = true;
    bool previous_jump_pressed_ = false;
    bool jump_start_playing_ = false;
    bool landing_animation_playing_ = false;
    // Hysteresis for the 8-way combat clip choice, so a direction on a sector edge does not flicker.
    int combat_sector_ = 0;
    bool combat_sector_valid_ = false;
    bool animation_preview_active_ = false;
    std::string_view active_animation_;
};

}
