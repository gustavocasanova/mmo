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
    const auto side = glm::cross(focus - eye, math::Vec3{0, 1, 0});
    if (glm::dot(side, side) < 1e-8f) {
        throw std::invalid_argument("camera direction must not be zero or parallel to up");
    }
    eye_ = eye;
    focus_ = focus;
}
math::Mat4 Camera::view() const { return glm::lookAtRH(eye_, focus_, math::Vec3{0, 1, 0}); }
math::Mat4 Camera::projection(float aspect_ratio) const
{
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f) {
        throw std::invalid_argument("camera aspect ratio must be positive and finite");
    }
    return glm::perspectiveRH_NO(1.04719755f, aspect_ratio, 0.1f, 160.0f);
}
}
