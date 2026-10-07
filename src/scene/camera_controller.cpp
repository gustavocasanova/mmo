#include "scene/camera_controller.hpp"

#include "game/world/collision_world.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmo::scene {
namespace {

constexpr float kPi = 3.14159265f;

float wrap_angle(float angle)
{
    return std::remainder(angle, 2.0f * kPi);
}

}

CameraController::CameraController(CameraSettings settings)
    : settings_(settings)
{
    if (!std::isfinite(settings_.minimum_distance) ||
        !std::isfinite(settings_.maximum_distance) ||
        !std::isfinite(settings_.initial_distance) ||
        !std::isfinite(settings_.horizontal_sensitivity) ||
        !std::isfinite(settings_.vertical_sensitivity) ||
        !std::isfinite(settings_.minimum_pitch) ||
        !std::isfinite(settings_.maximum_pitch) ||
        !std::isfinite(settings_.zoom_speed) ||
        !std::isfinite(settings_.distance_smoothing_seconds) ||
        !std::isfinite(settings_.rotation_smoothing_seconds) ||
        !std::isfinite(settings_.collision_radius) ||
        !std::isfinite(settings_.minimum_collision_distance) ||
        !std::isfinite(settings_.target_offset.x) ||
        !std::isfinite(settings_.target_offset.y) ||
        !std::isfinite(settings_.target_offset.z)) {
        throw std::invalid_argument("camera settings must be finite");
    }
    settings_.minimum_distance = std::max(settings_.minimum_distance, 0.1f);
    settings_.maximum_distance = std::max(
        settings_.maximum_distance, settings_.minimum_distance);
    settings_.initial_distance = std::clamp(
        settings_.initial_distance,
        settings_.minimum_distance,
        settings_.maximum_distance);
    settings_.minimum_pitch = std::clamp(
        settings_.minimum_pitch, -1.553343f, 1.553343f);
    settings_.maximum_pitch = std::clamp(
        settings_.maximum_pitch, settings_.minimum_pitch, 1.553343f);
    settings_.horizontal_sensitivity = std::max(settings_.horizontal_sensitivity, 0.0f);
    settings_.vertical_sensitivity = std::max(settings_.vertical_sensitivity, 0.0f);
    settings_.zoom_speed = std::max(settings_.zoom_speed, 0.0f);
    settings_.distance_smoothing_seconds = std::max(
        settings_.distance_smoothing_seconds, 0.0f);
    settings_.rotation_smoothing_seconds = std::max(
        settings_.rotation_smoothing_seconds, 0.0f);
    settings_.collision_radius = std::max(settings_.collision_radius, 0.0f);
    settings_.minimum_collision_distance = std::clamp(
        settings_.minimum_collision_distance, 0.1f, settings_.minimum_distance);
    settings_.field_of_view = std::clamp(settings_.field_of_view, 0.01f, kPi - 0.01f);
    settings_.near_plane = std::max(settings_.near_plane, 0.001f);
    settings_.far_plane = std::max(settings_.far_plane, settings_.near_plane + 0.001f);

    pitch_ = std::clamp(pitch_, settings_.minimum_pitch, settings_.maximum_pitch);
    current_pitch_ = pitch_;
    desired_distance_ = settings_.initial_distance;
    current_distance_ = desired_distance_;
}

float CameraController::apply_input(const CameraInput& input)
{
    float yaw_delta = 0.0f;
    if (input.rotate_camera) {
        yaw_delta = static_cast<float>(input.mouse_delta_x) *
            settings_.horizontal_sensitivity;
        yaw_ = wrap_angle(yaw_ + yaw_delta);

        const float vertical_direction = settings_.invert_vertical ? 1.0f : -1.0f;
        pitch_ = std::clamp(
            pitch_ + static_cast<float>(input.mouse_delta_y) *
                settings_.vertical_sensitivity * vertical_direction,
            settings_.minimum_pitch,
            settings_.maximum_pitch);
    }

    desired_distance_ = std::clamp(
        desired_distance_ - static_cast<float>(input.scroll_delta) * settings_.zoom_speed,
        settings_.minimum_distance,
        settings_.maximum_distance);
    return settings_.rotate_target_with_camera ? yaw_delta : 0.0f;
}

void CameraController::reset_behind_target(float target_yaw)
{
    yaw_ = wrap_angle(target_yaw + kPi);
    current_yaw_ = yaw_;
}

CameraPose CameraController::update(
    float delta_seconds,
    const CameraTarget& target,
    const game::world::CollisionWorld* collision_world)
{
    const float elapsed = std::max(delta_seconds, 0.0f);
    const math::Vec3 desired_focus = target.position + settings_.target_offset;
    if (!initialized_) {
        current_focus_ = desired_focus;
        initialized_ = true;
    } else {
        const float focus_smoothing = settings_.distance_smoothing_seconds;
        const float focus_factor = focus_smoothing <= 0.0f
            ? 1.0f
            : 1.0f - std::exp(-elapsed / focus_smoothing);
        current_focus_ += (desired_focus - current_focus_) * focus_factor;
    }

    const float rotation_factor = settings_.rotation_smoothing_seconds <= 0.0f
        ? 1.0f
        : 1.0f - std::exp(-elapsed / settings_.rotation_smoothing_seconds);
    current_yaw_ = wrap_angle(current_yaw_ +
        wrap_angle(yaw_ - current_yaw_) * rotation_factor);
    current_pitch_ += (pitch_ - current_pitch_) * rotation_factor;

    const math::Vec3 desired_offset{
        std::sin(current_yaw_) * std::cos(current_pitch_) * desired_distance_,
        std::sin(current_pitch_) * desired_distance_,
        std::cos(current_yaw_) * std::cos(current_pitch_) * desired_distance_,
    };
    const math::Vec3 desired_eye = current_focus_ + desired_offset;
    float target_distance = desired_distance_;
    bool obstructed = false;
    if (collision_world != nullptr) {
        const float hit_distance = collision_world->camera_distance(
            current_focus_, desired_eye, settings_.collision_radius);
        if (hit_distance < desired_distance_) {
            target_distance = std::min(
                desired_distance_,
                std::max(settings_.minimum_collision_distance, hit_distance - 0.02f));
            obstructed = true;
        }
    }
    const float distance_factor = settings_.distance_smoothing_seconds <= 0.0f
        ? 1.0f
        : 1.0f - std::exp(-elapsed / settings_.distance_smoothing_seconds);
    if (obstructed && target_distance < current_distance_) {
        current_distance_ = target_distance;
    } else {
        current_distance_ += (target_distance - current_distance_) * distance_factor;
    }

    const math::Vec3 camera_offset{
        std::sin(current_yaw_) * std::cos(current_pitch_) * current_distance_,
        std::sin(current_pitch_) * current_distance_,
        std::cos(current_yaw_) * std::cos(current_pitch_) * current_distance_,
    };
    const math::Vec3 eye = current_focus_ + camera_offset;
    return {
        eye.x, eye.y, eye.z,
        current_focus_.x, current_focus_.y, current_focus_.z,
        settings_.field_of_view, settings_.near_plane, settings_.far_plane,
    };
}

const CameraSettings& CameraController::settings() const
{
    return settings_;
}

float CameraController::yaw() const
{
    return yaw_;
}

float CameraController::pitch() const
{
    return pitch_;
}

float CameraController::desired_distance() const
{
    return desired_distance_;
}

float CameraController::current_distance() const
{
    return current_distance_;
}

float CameraController::actual_distance() const
{
    return current_distance_;
}

}
