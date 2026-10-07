#include "character/character_controller.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmo::character {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.0001f;

float wrap_angle(float angle)
{
    return std::remainder(angle, 2.0f * kPi);
}

}

CharacterController::CharacterController(Character& character, MovementSettings settings)
    : character_(character), settings_(settings)
{
    if (!std::isfinite(settings_.walk_speed) ||
        !std::isfinite(settings_.run_speed) ||
        !std::isfinite(settings_.backward_speed) ||
        !std::isfinite(settings_.strafe_speed) ||
        !std::isfinite(settings_.acceleration) ||
        !std::isfinite(settings_.deceleration) ||
        !std::isfinite(settings_.air_control) ||
        !std::isfinite(settings_.jump_speed) ||
        !std::isfinite(settings_.gravity) ||
        !std::isfinite(settings_.turn_speed) ||
        !std::isfinite(settings_.collision_radius) ||
        !std::isfinite(settings_.collision_height) ||
        settings_.walk_speed < 0.0f || settings_.run_speed < 0.0f ||
        settings_.backward_speed < 0.0f || settings_.strafe_speed < 0.0f ||
        settings_.acceleration < 0.0f || settings_.deceleration < 0.0f ||
        settings_.air_control < 0.0f || settings_.jump_speed < 0.0f ||
        settings_.gravity < 0.0f || settings_.turn_speed < 0.0f ||
        settings_.collision_radius < 0.0f ||
        settings_.collision_height <= 0.0f) {
        throw std::invalid_argument("character movement settings must be finite and non-negative");
    }
    settings_.air_control = std::clamp(settings_.air_control, 0.0f, 1.0f);
    const animation::Vector3 position = character_.transform().position;
    const auto& rotation = character_.transform().rotation;
    yaw_ = std::atan2(
        2.0f * (rotation.w * rotation.y + rotation.x * rotation.z),
        1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z));
    if (!std::isfinite(position[0]) || !std::isfinite(position[1]) ||
        !std::isfinite(position[2]) || !std::isfinite(yaw_)) {
        throw std::invalid_argument("character transform must be finite");
    }
}

void CharacterController::update(
    float delta_seconds,
    CharacterControllerInput input,
    float camera_yaw,
    const game::world::CollisionWorld& collision_world,
    const game::world::Terrain* terrain)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f ||
        !std::isfinite(input.forward) || !std::isfinite(input.strafe) ||
        !std::isfinite(camera_yaw)) {
        throw std::invalid_argument("character controller input must be finite");
    }
    const float elapsed = std::min(delta_seconds, 0.1f);
    input.forward = std::clamp(input.forward, -1.0f, 1.0f);
    input.strafe = std::clamp(input.strafe, -1.0f, 1.0f);

    animation::Vector3& character_position = character_.transform().position;
    math::Vec3 position{character_position[0], character_position[1], character_position[2]};
    const float forward_length = std::sqrt(
        input.forward * input.forward + input.strafe * input.strafe);
    const float input_scale = forward_length > 1.0f ? 1.0f / forward_length : 1.0f;
    const float forward_input = input.forward * input_scale;
    const float strafe_input = input.strafe * input_scale;
    const math::Vec3 camera_forward{-std::sin(camera_yaw), 0.0f, -std::cos(camera_yaw)};
    const math::Vec3 camera_right{std::cos(camera_yaw), 0.0f, -std::sin(camera_yaw)};
    math::Vec3 movement_direction =
        camera_forward * forward_input + camera_right * strafe_input;
    const float movement_length = glm::length(movement_direction);
    if (movement_length > kEpsilon) {
        movement_direction /= movement_length;
    } else {
        movement_direction = math::Vec3{0.0f};
    }

    float desired_speed = 0.0f;
    if (movement_length > kEpsilon) {
        if (input.run) {
            desired_speed = settings_.run_speed;
        } else if (forward_input < -kEpsilon && std::abs(strafe_input) < kEpsilon) {
            desired_speed = settings_.backward_speed;
        } else if (std::abs(forward_input) < kEpsilon &&
            std::abs(strafe_input) > kEpsilon) {
            desired_speed = settings_.strafe_speed;
        } else {
            desired_speed = input.run ? settings_.run_speed : settings_.walk_speed;
        }
    }
    const float air_factor = grounded_ ? 1.0f : settings_.air_control;
    const math::Vec3 desired_velocity = movement_direction * desired_speed * air_factor;
    const float response = movement_length > kEpsilon
        ? settings_.acceleration * air_factor * elapsed
        : settings_.deceleration * air_factor * elapsed;
    const math::Vec3 velocity_delta = desired_velocity - horizontal_velocity_;
    const float velocity_delta_length = glm::length(velocity_delta);
    if (velocity_delta_length <= response || velocity_delta_length <= kEpsilon) {
        horizontal_velocity_ = desired_velocity;
    } else {
        horizontal_velocity_ += velocity_delta * (response / velocity_delta_length);
    }
    position = collision_world.move_character(
        position, horizontal_velocity_ * elapsed, settings_.collision_radius,
        settings_.collision_height);
    const float ground_height = terrain != nullptr
        ? terrain->height_at({position.x, position.z})
        : 0.0f;

    const bool jump_started = input.jump_pressed && !previous_jump_pressed_ && grounded_;
    previous_jump_pressed_ = input.jump_pressed;
    if (jump_started) {
        grounded_ = false;
        vertical_velocity_ = settings_.jump_speed;
        jump_start_playing_ = character_.model().animation().play_animation(
            "Jump_Start", false);
        active_animation_ = jump_start_playing_ ? "Jump_Start" : "";
        if (!jump_start_playing_) {
            (void)character_.model().animation().play_animation("Jump_Loop", true);
            active_animation_ = "Jump_Loop";
        }
    }

    if (!grounded_) {
        position.y += vertical_velocity_ * elapsed;
        vertical_velocity_ -= settings_.gravity * elapsed;
        if (position.y <= ground_height && vertical_velocity_ < 0.0f) {
            position.y = ground_height;
            vertical_velocity_ = 0.0f;
            grounded_ = true;
            jump_start_playing_ = false;
            landing_animation_playing_ = character_.model().animation().play_animation(
                "Jump_Land", false);
            active_animation_ = landing_animation_playing_ ? "Jump_Land" : "";
        }
    } else {
        position.y = ground_height;
    }

    if (settings_.rotate_character_to_movement && movement_length > kEpsilon) {
        const float target_yaw = std::atan2(movement_direction.x, movement_direction.z);
        const float yaw_delta = wrap_angle(target_yaw - yaw_);
        const float turn = 1.0f - std::exp(-settings_.turn_speed * elapsed);
        yaw_ = wrap_angle(yaw_ + yaw_delta * turn);
    }

    character_position = {position.x, position.y, position.z};
    character_.transform().rotation = {
        0.0f, std::sin(yaw_ * 0.5f), 0.0f, std::cos(yaw_ * 0.5f)};

    const float horizontal_speed = glm::length(horizontal_velocity_);
    if (!grounded_) {
        state_ = vertical_velocity_ > 0.0f
            ? CharacterMovementState::Jumping
            : CharacterMovementState::Falling;
    } else if (horizontal_speed > 0.05f) {
        state_ = input.run || horizontal_speed > settings_.walk_speed + 0.05f
            ? CharacterMovementState::Running
            : CharacterMovementState::Walking;
    } else {
        state_ = CharacterMovementState::Idle;
    }

    update_animation(input);
    character_.model().update(elapsed);
    if (animation_preview_active_ &&
        !character_.model().animation().animator().playing()) {
        animation_preview_active_ = false;
        active_animation_ = "";
    }
    if (jump_start_playing_ &&
        !character_.model().animation().animator().playing()) {
        jump_start_playing_ = false;
        (void)character_.model().animation().play_animation("Jump_Loop", true);
        active_animation_ = "Jump_Loop";
    }
    if (landing_animation_playing_ &&
        !character_.model().animation().animator().playing()) {
        landing_animation_playing_ = false;
        active_animation_ = "";
    }
}

void CharacterController::update_animation(
    const CharacterControllerInput& input)
{
    if (animation_preview_active_ || jump_start_playing_ ||
        landing_animation_playing_) {
        return;
    }
    if (!grounded_) {
        if (active_animation_ != "Jump_Loop") {
            (void)character_.model().animation().play_animation("Jump_Loop", true);
            active_animation_ = "Jump_Loop";
        }
        return;
    }
    select_locomotion_animation(input);
}

void CharacterController::select_locomotion_animation(
    const CharacterControllerInput& input)
{
    const float speed = glm::length(horizontal_velocity_);
    if (speed <= 0.05f) {
        if (active_animation_ != "Idle_Loop") {
            (void)character_.model().animation().set_state(animation::AnimationState::Idle);
            active_animation_ = "Idle_Loop";
        }
        return;
    }

    const bool backwards = input.forward < -kEpsilon &&
        std::abs(input.strafe) < kEpsilon;
    const std::string_view clip = backwards && !input.run
        ? "Walk_Backward_Loop"
        : (state_ == CharacterMovementState::Running
            ? (input.run ? "Sprint_Loop" : "Jog_Fwd_Loop")
            : "Walk_Loop");
    if (active_animation_ != clip) {
        if (!character_.model().animation().play_animation(clip, true) &&
            !character_.model().animation().set_state(
                state_ == CharacterMovementState::Running
                    ? animation::AnimationState::Run
                    : animation::AnimationState::Walk)) {
            throw std::runtime_error("No compatible locomotion animation is bound");
        }
        active_animation_ = clip;
    }
}

bool CharacterController::preview_animation(std::string_view name)
{
    if (!character_.model().animation().play_animation(name, false)) {
        return false;
    }
    animation_preview_active_ = true;
    active_animation_ = name;
    return true;
}

CharacterMovementState CharacterController::state() const
{
    return state_;
}

bool CharacterController::grounded() const
{
    return grounded_;
}

float CharacterController::vertical_velocity() const
{
    return vertical_velocity_;
}

float CharacterController::yaw() const
{
    return yaw_;
}

const MovementSettings& CharacterController::settings() const
{
    return settings_;
}

}
