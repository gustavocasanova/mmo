#include "scene/camera_controller.hpp"

#include <algorithm>
#include <cmath>

namespace mmo::scene {
namespace {

constexpr float kPi = 3.14159265f;
constexpr float kGroundHeight = 0.0f;

float wrap_angle(float angle)
{
    return std::remainder(angle, 2.0f * kPi);
}

float smoothing_factor(float speed, float delta_seconds)
{
    if (speed <= 0.0f) {
        return 1.0f;
    }
    return 1.0f - std::exp(-speed * std::max(delta_seconds, 0.0f));
}

float smooth_angle(float current, float target, float factor)
{
    return current + wrap_angle(target - current) * factor;
}

}

CameraController::CameraController(CameraSettings settings)
    : settings_(settings)
{
    settings_.minimum_distance = std::max(settings_.minimum_distance, 0.1f);
    settings_.maximum_distance = std::max(settings_.maximum_distance, settings_.minimum_distance);
    settings_.initial_distance = std::clamp(settings_.initial_distance,
        settings_.minimum_distance, settings_.maximum_distance);
    settings_.minimum_elevation = std::clamp(settings_.minimum_elevation, -1.45f, 1.45f);
    settings_.maximum_elevation = std::clamp(settings_.maximum_elevation,
        settings_.minimum_elevation, 1.45f);
    settings_.horizontal_sensitivity = std::max(settings_.horizontal_sensitivity, 0.0f);
    settings_.vertical_sensitivity = std::max(settings_.vertical_sensitivity, 0.0f);
    settings_.zoom_speed = std::max(settings_.zoom_speed, 0.0f);
    settings_.smoothing = std::max(settings_.smoothing, 0.0f);
    settings_.collision_approach_speed = std::max(settings_.collision_approach_speed, 0.0f);
    settings_.collision_return_speed = std::max(settings_.collision_return_speed, 0.0f);
    settings_.ground_clearance = std::max(settings_.ground_clearance, 0.0f);
    desired_camera_yaw_ = kPi;
    elevation_ = std::clamp(elevation_, settings_.minimum_elevation, settings_.maximum_elevation);
    target_distance_ = settings_.initial_distance;
}

float CameraController::apply_input(const CameraInput& input)
{
    float yaw_delta = 0.0f;
    if (input.rotate_camera) {
        yaw_delta = -static_cast<float>(input.mouse_delta_x) *
            settings_.horizontal_sensitivity;
        desired_camera_yaw_ = wrap_angle(desired_camera_yaw_ + yaw_delta);

        const float vertical_direction = settings_.invert_vertical ? 1.0f : -1.0f;
        elevation_ = std::clamp(
            elevation_ + static_cast<float>(input.mouse_delta_y) *
                settings_.vertical_sensitivity * vertical_direction,
            settings_.minimum_elevation,
            settings_.maximum_elevation);
    }

    target_distance_ = std::clamp(
        target_distance_ - static_cast<float>(input.scroll_delta) * settings_.zoom_speed,
        settings_.minimum_distance,
        settings_.maximum_distance);
    return yaw_delta;
}

CameraPose CameraController::update(
    float delta_seconds,
    float player_x,
    float player_y,
    float player_z,
    float player_yaw,
    bool align_behind_character)
{
    if (align_behind_character) {
        desired_camera_yaw_ = wrap_angle(player_yaw + kPi);
    }

    const float target_focus_x = player_x;
    const float target_focus_y = player_y + settings_.focus_height;
    const float target_focus_z = player_z;
    const float target_yaw = desired_camera_yaw_;
    float target_elevation = elevation_;
    float target_distance = target_distance_;
    bool ground_collision = false;

    const float vertical_direction = std::sin(target_elevation);
    if (vertical_direction < 0.0f) {
        const float available_height = target_focus_y -
            (kGroundHeight + settings_.ground_clearance);
        const float safe_distance = available_height / -vertical_direction;
        if (target_distance > safe_distance) {
            ground_collision = true;
            if (safe_distance >= settings_.minimum_distance) {
                target_distance = safe_distance;
            } else if (target_distance > 0.0f) {
                const float safe_sine = std::clamp(
                    -available_height / target_distance, -1.0f, 1.0f);
                target_elevation = std::asin(safe_sine);
            }
        }
    }

    float distance_speed = settings_.smoothing;
    if (ground_collision) {
        distance_speed = settings_.collision_approach_speed;
    } else if (was_ground_colliding_) {
        distance_speed = settings_.collision_return_speed;
    }
    const float follow_factor = smoothing_factor(settings_.smoothing, delta_seconds);
    const float distance_factor = smoothing_factor(distance_speed, delta_seconds);

    if (!initialized_) {
        focus_x_ = target_focus_x;
        focus_y_ = target_focus_y;
        focus_z_ = target_focus_z;
        camera_yaw_ = target_yaw;
        camera_elevation_ = target_elevation;
        camera_distance_ = target_distance;
        initialized_ = true;
    } else {
        focus_x_ += (target_focus_x - focus_x_) * follow_factor;
        focus_y_ += (target_focus_y - focus_y_) * follow_factor;
        focus_z_ += (target_focus_z - focus_z_) * follow_factor;
        camera_yaw_ = smooth_angle(camera_yaw_, target_yaw, follow_factor);
        camera_elevation_ += (target_elevation - camera_elevation_) * follow_factor;
        camera_distance_ += (target_distance - camera_distance_) * distance_factor;
    }
    was_ground_colliding_ = ground_collision;

    const float current_vertical_direction = std::sin(camera_elevation_);
    if (current_vertical_direction < 0.0f &&
        focus_y_ + current_vertical_direction * camera_distance_ <
            kGroundHeight + settings_.ground_clearance) {
        const float minimum_safe_sine = std::clamp(
            (kGroundHeight + settings_.ground_clearance - focus_y_) /
                std::max(camera_distance_, settings_.minimum_distance),
            -1.0f,
            1.0f);
        camera_elevation_ = std::max(camera_elevation_, std::asin(minimum_safe_sine));
    }

    const float horizontal_distance = camera_distance_ * std::cos(camera_elevation_);
    return {
        focus_x_ + std::sin(camera_yaw_) * horizontal_distance,
        focus_y_ + std::sin(camera_elevation_) * camera_distance_,
        focus_z_ + std::cos(camera_yaw_) * horizontal_distance,
        focus_x_,
        focus_y_,
        focus_z_,
    };
}

}
