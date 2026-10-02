#pragma once
#include "scene/camera.hpp"

namespace mmo::scene {

struct CameraSettings {
    float horizontal_sensitivity = 0.005f;
    float vertical_sensitivity = 0.005f;
    float initial_distance = 11.2f;
    float minimum_distance = 3.0f;
    float maximum_distance = 20.0f;
    float zoom_speed = 1.2f;
    float focus_height = 1.35f;
    float minimum_elevation = -0.15f;
    float maximum_elevation = 1.25f;
    float smoothing = 16.0f;
    float collision_approach_speed = 30.0f;
    float collision_return_speed = 5.0f;
    float ground_clearance = 0.25f;
    bool invert_vertical = false;
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
    CameraPose update(
        float delta_seconds,
        float player_x,
        float player_y,
        float player_z,
        float player_yaw,
        bool align_behind_character);

private:
    CameraSettings settings_;
    float desired_camera_yaw_ = 3.14159265f;
    float elevation_ = 0.402f;
    float target_distance_ = 11.2f;
    float focus_x_ = 0.0f;
    float focus_y_ = 0.0f;
    float focus_z_ = 0.0f;
    float camera_yaw_ = 0.0f;
    float camera_elevation_ = 0.0f;
    float camera_distance_ = 0.0f;
    bool initialized_ = false;
    bool was_ground_colliding_ = false;
};

}
