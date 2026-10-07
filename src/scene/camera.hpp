#pragma once

#include "math/transform.hpp"

namespace mmo::scene {
struct CameraPose {
    float eye_x, eye_y, eye_z;
    float focus_x, focus_y, focus_z;
    float field_of_view = 1.04719755f;
    float near_plane = 0.05f;
    float far_plane = 1000.0f;
};
class Camera {
public:
    void set_pose(CameraPose pose);
    math::Mat4 view() const;
    math::Mat4 projection(float aspect_ratio) const;
private:
    math::Vec3 eye_{0.0f, 4.0f, -10.0f};
    math::Vec3 focus_{0.0f, 1.35f, 0.0f};
    float field_of_view_ = 1.04719755f;
    float near_plane_ = 0.05f;
    float far_plane_ = 1000.0f;
};
}
