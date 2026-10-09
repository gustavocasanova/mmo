#pragma once

#include "scene/camera.hpp"

#include <glm/vec3.hpp>

#include <optional>

namespace mmo::game::world {
class CollisionWorld;
}

namespace mmo::scene {

struct CameraTarget {
    math::Vec3 position{0.0f};
    float yaw = 0.0f;
};

struct CameraSettings {
    float initial_distance = 6.0f;
    float minimum_distance = 2.0f;
    float maximum_distance = 15.0f;
    math::Vec3 target_offset{0.0f, 1.5f, 0.0f};
    float horizontal_sensitivity = 0.005f;
    float vertical_sensitivity = 0.005f;
    float minimum_pitch = -1.3962634f;
    float maximum_pitch = 1.3962634f;
    float zoom_speed = 5.0f;
    float distance_smoothing_seconds = 0.12f;
    float rotation_smoothing_seconds = 0.06f;
    float follow_rotation_smoothing_seconds = 0.45f;
    float collision_radius = 0.2f;
    float minimum_collision_distance = 0.65f;
    float field_of_view = 1.04719755f;
    float near_plane = 0.05f;
    float far_plane = 1000.0f;
    bool invert_vertical = false;
    bool rotate_target_with_camera = false;
};

struct CameraInput {
    double mouse_delta_x = 0.0;
    double mouse_delta_y = 0.0;
    double scroll_delta = 0.0;
    bool rotate_camera = false;
};

class CameraController {
public:
    explicit CameraController(CameraSettings settings = {});

    float apply_input(const CameraInput& input);
    void reset_behind_target(float target_yaw);
    void set_follow_target_yaw(std::optional<float> target_yaw);
    [[nodiscard]] CameraPose update(
        float delta_seconds,
        const CameraTarget& target,
        const game::world::CollisionWorld* collision_world = nullptr);
    [[nodiscard]] const CameraSettings& settings() const;
    [[nodiscard]] float yaw() const;
    [[nodiscard]] float pitch() const;
    [[nodiscard]] float desired_distance() const;
    [[nodiscard]] float current_distance() const;
    [[nodiscard]] float actual_distance() const;

private:
    CameraSettings settings_;
    float yaw_ = 3.14159265f;
    float pitch_ = 0.34906585f;
    float current_yaw_ = 3.14159265f;
    float current_pitch_ = 0.34906585f;
    float desired_distance_ = 6.0f;
    float current_distance_ = 6.0f;
    math::Vec3 current_focus_{0.0f};
    std::optional<float> follow_target_yaw_;
    bool initialized_ = false;
};

}
