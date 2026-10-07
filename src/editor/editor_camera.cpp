#include "editor/editor_camera.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmo::editor {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.0001f;

bool finite_vec3(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

}

EditorCamera::EditorCamera(EditorCameraSettings settings)
    : settings_(settings), camera_speed_(settings.camera_speed)
{
    if (!std::isfinite(settings_.camera_speed) ||
        !std::isfinite(settings_.camera_fast_speed) ||
        !std::isfinite(settings_.camera_slow_speed) ||
        !std::isfinite(settings_.camera_rotation_speed) ||
        !std::isfinite(settings_.camera_zoom_speed) ||
        !std::isfinite(settings_.camera_focus_distance) ||
        !std::isfinite(settings_.minimum_speed) ||
        !std::isfinite(settings_.maximum_speed) ||
        !std::isfinite(settings_.minimum_pitch) ||
        !std::isfinite(settings_.maximum_pitch) ||
        !std::isfinite(settings_.field_of_view) ||
        !std::isfinite(settings_.near_plane) ||
        !std::isfinite(settings_.far_plane) ||
        settings_.camera_speed <= 0.0f ||
        settings_.camera_fast_speed < 1.0f ||
        settings_.camera_slow_speed <= 0.0f ||
        settings_.camera_slow_speed > 1.0f ||
        settings_.camera_rotation_speed < 0.0f ||
        settings_.camera_zoom_speed <= 0.0f ||
        settings_.camera_focus_distance <= 0.0f ||
        settings_.minimum_speed <= 0.0f ||
        settings_.maximum_speed < settings_.minimum_speed ||
        settings_.minimum_pitch < -kPi * 0.5f ||
        settings_.maximum_pitch > kPi * 0.5f ||
        settings_.maximum_pitch <= settings_.minimum_pitch ||
        settings_.field_of_view <= 0.0f || settings_.field_of_view >= kPi ||
        settings_.near_plane <= 0.0f ||
        settings_.far_plane <= settings_.near_plane) {
        throw std::invalid_argument("editor camera settings are invalid");
    }
    camera_speed_ = std::clamp(
        camera_speed_, settings_.minimum_speed, settings_.maximum_speed);
}

void EditorCamera::set_pose(const scene::CameraPose& pose)
{
    const glm::vec3 eye{pose.eye_x, pose.eye_y, pose.eye_z};
    const glm::vec3 focus{pose.focus_x, pose.focus_y, pose.focus_z};
    const glm::vec3 direction = focus - eye;
    if (!finite_vec3(eye) || !finite_vec3(focus) ||
        !std::isfinite(glm::length(direction)) || glm::length(direction) <= kEpsilon) {
        throw std::invalid_argument("editor camera pose must have finite eye and focus");
    }

    position_ = eye;
    const glm::vec3 normalized = glm::normalize(direction);
    yaw_ = std::atan2(normalized.x, normalized.z);
    pitch_ = std::clamp(
        std::asin(std::clamp(normalized.y, -1.0f, 1.0f)),
        settings_.minimum_pitch, settings_.maximum_pitch);
}

void EditorCamera::focus_on(glm::vec3 target)
{
    if (!finite_vec3(target)) {
        throw std::invalid_argument("editor camera focus target must be finite");
    }
    position_ = target - forward() * settings_.camera_focus_distance;
}

void EditorCamera::update(float delta_seconds, const EditorCameraInput& input)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f ||
        !std::isfinite(input.forward) || !std::isfinite(input.right) ||
        !std::isfinite(input.up) || !std::isfinite(input.mouse_delta_x) ||
        !std::isfinite(input.mouse_delta_y) || !std::isfinite(input.scroll_delta)) {
        throw std::invalid_argument("editor camera input must be finite");
    }

    if (input.look) {
        yaw_ = std::remainder(yaw_ +
            static_cast<float>(input.mouse_delta_x) *
                settings_.camera_rotation_speed,
            2.0f * kPi);
        pitch_ = std::clamp(
            pitch_ - static_cast<float>(input.mouse_delta_y) *
                settings_.camera_rotation_speed,
            settings_.minimum_pitch, settings_.maximum_pitch);
    }

    if (input.scroll_delta != 0.0) {
        camera_speed_ *= std::pow(
            settings_.camera_zoom_speed, static_cast<float>(input.scroll_delta));
        camera_speed_ = std::clamp(
            camera_speed_, settings_.minimum_speed, settings_.maximum_speed);
    }

    const float speed_multiplier = input.fast
        ? settings_.camera_fast_speed
        : input.slow ? settings_.camera_slow_speed : 1.0f;
    const glm::vec3 camera_forward = forward();
    const glm::vec3 world_up{0.0f, 1.0f, 0.0f};
    const glm::vec3 camera_right = glm::normalize(glm::cross(camera_forward, world_up));
    const glm::vec3 movement =
        camera_forward * input.forward + camera_right * input.right + world_up * input.up;
    const float movement_length = glm::length(movement);
    if (movement_length > 1.0f) {
        position_ += movement / movement_length *
            (camera_speed_ * speed_multiplier * delta_seconds);
    } else if (movement_length > kEpsilon) {
        position_ += movement *
            (camera_speed_ * speed_multiplier * delta_seconds);
    }
}

scene::CameraPose EditorCamera::pose() const
{
    const glm::vec3 direction = forward();
    const glm::vec3 focus = position_ + direction;
    return {
        position_.x, position_.y, position_.z,
        focus.x, focus.y, focus.z,
        settings_.field_of_view, settings_.near_plane, settings_.far_plane,
    };
}

glm::vec3 EditorCamera::position() const
{
    return position_;
}

glm::vec3 EditorCamera::forward() const
{
    const float horizontal = std::cos(pitch_);
    return {
        std::sin(yaw_) * horizontal,
        std::sin(pitch_),
        std::cos(yaw_) * horizontal,
    };
}

float EditorCamera::yaw() const
{
    return yaw_;
}

float EditorCamera::pitch() const
{
    return pitch_;
}

float EditorCamera::camera_speed() const
{
    return camera_speed_;
}

const EditorCameraSettings& EditorCamera::settings() const
{
    return settings_;
}

}
