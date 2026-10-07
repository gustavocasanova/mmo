#pragma once

#include "scene/camera.hpp"

#include <glm/vec3.hpp>

namespace mmo::editor {

struct EditorCameraSettings {
    float camera_speed = 8.0f;
    float camera_fast_speed = 4.0f;
    float camera_slow_speed = 0.2f;
    float camera_rotation_speed = 0.0025f;
    float camera_zoom_speed = 1.2f;
    float camera_focus_distance = 8.0f;
    float minimum_speed = 0.5f;
    float maximum_speed = 80.0f;
    float minimum_pitch = -1.553343f;
    float maximum_pitch = 1.553343f;
    float field_of_view = 1.04719755f;
    float near_plane = 0.05f;
    float far_plane = 1000.0f;
    bool invert_mouse_x = true;
    bool invert_mouse_y = true;
};

struct EditorCameraInput {
    float forward = 0.0f;
    float right = 0.0f;
    float up = 0.0f;
    double mouse_delta_x = 0.0;
    double mouse_delta_y = 0.0;
    double scroll_delta = 0.0;
    bool look = false;
    bool fast = false;
    bool slow = false;
};

class EditorCamera {
public:
    explicit EditorCamera(EditorCameraSettings settings = {});

    void set_pose(const scene::CameraPose& pose);
    void focus_on(glm::vec3 target);
    void update(float delta_seconds, const EditorCameraInput& input);
    [[nodiscard]] scene::CameraPose pose() const;
    [[nodiscard]] glm::vec3 position() const;
    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] float yaw() const;
    [[nodiscard]] float pitch() const;
    [[nodiscard]] float camera_speed() const;
    [[nodiscard]] const EditorCameraSettings& settings() const;

private:
    EditorCameraSettings settings_;
    glm::vec3 position_{0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float camera_speed_ = 8.0f;
};

}
