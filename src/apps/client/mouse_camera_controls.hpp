#pragma once

#include <optional>

namespace mmo::app {

enum class MouseCameraMode {
    none,
    left,
    right,
    both,
};

[[nodiscard]] constexpr MouseCameraMode mouse_camera_mode(
    bool left_down, bool right_down)
{
    return left_down && right_down ? MouseCameraMode::both
        : left_down ? MouseCameraMode::left
        : right_down ? MouseCameraMode::right
        : MouseCameraMode::none;
}

[[nodiscard]] constexpr bool rotates_camera(MouseCameraMode mode)
{
    return mode != MouseCameraMode::none;
}

[[nodiscard]] constexpr bool controls_character_facing(MouseCameraMode mode)
{
    return mode == MouseCameraMode::right || mode == MouseCameraMode::both;
}

[[nodiscard]] constexpr float movement_yaw(
    MouseCameraMode mode, float camera_yaw, float left_movement_yaw)
{
    return mode == MouseCameraMode::left ? left_movement_yaw : camera_yaw;
}

[[nodiscard]] constexpr std::optional<float> manual_facing_yaw(
    MouseCameraMode mode, float camera_yaw)
{
    if (!controls_character_facing(mode)) {
        return std::nullopt;
    }
    return camera_yaw + 3.14159265f;
}

[[nodiscard]] constexpr float forward_input(float keyboard_forward, MouseCameraMode mode)
{
    return mode == MouseCameraMode::both ? 1.0f : keyboard_forward;
}

[[nodiscard]] constexpr bool rebase_left_look_heading(
    MouseCameraMode mode, float forward, float strafe)
{
    return mode == MouseCameraMode::left && forward == 0.0f && strafe == 0.0f;
}

[[nodiscard]] constexpr bool follows_character(
    MouseCameraMode mode, bool moving, bool manual_orientation_locked)
{
    return mode == MouseCameraMode::none && !moving && !manual_orientation_locked;
}

}