#include "scene/camera.hpp"
#include <cmath>
#include <stdexcept>

namespace mmo::scene {
void Camera::set_pose(CameraPose pose)
{
    const math::Vec3 eye{pose.eye_x, pose.eye_y, pose.eye_z};
    const math::Vec3 focus{pose.focus_x, pose.focus_y, pose.focus_z};
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(eye[i]) || !std::isfinite(focus[i])) {
            throw std::invalid_argument("camera pose must be finite");
        }
    }
    const math::Vec3 direction = focus - eye;
    if (glm::dot(direction, direction) < 1e-8f) {
        throw std::invalid_argument("camera eye and focus must not coincide");
    }
    eye_ = eye;
    focus_ = focus;
    field_of_view_ = pose.field_of_view;
    near_plane_ = pose.near_plane;
    far_plane_ = pose.far_plane;
}
math::Mat4 Camera::view() const
{
    const math::Vec3 direction = glm::normalize(focus_ - eye_);
    const math::Vec3 world_up{0.0f, 1.0f, 0.0f};
    const math::Vec3 up = std::abs(glm::dot(direction, world_up)) > 0.999f
        ? math::Vec3{0.0f, 0.0f, 1.0f}
        : world_up;
    return glm::lookAtRH(eye_, focus_, up);
}
math::Mat4 Camera::projection(float aspect_ratio) const
{
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f ||
        !std::isfinite(field_of_view_) || field_of_view_ <= 0.0f ||
        field_of_view_ >= 3.14159265f ||
        !std::isfinite(near_plane_) || near_plane_ <= 0.0f ||
        !std::isfinite(far_plane_) || far_plane_ <= near_plane_) {
        throw std::invalid_argument("camera projection settings are invalid");
    }
    return glm::perspectiveRH_NO(field_of_view_, aspect_ratio, near_plane_, far_plane_);
}
}
