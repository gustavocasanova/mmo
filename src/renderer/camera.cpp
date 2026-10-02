#include "renderer/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace mmo::renderer {

void Camera::set_view(glm::vec3 eye, glm::vec3 target, glm::vec3 up)
{
    view_matrix_ = glm::lookAt(eye, target, up);
}

void Camera::set_projection(
    int framebuffer_width,
    int framebuffer_height,
    float field_of_view,
    float near_plane,
    float far_plane)
{
    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }
    const float aspect_ratio = static_cast<float>(framebuffer_width) /
        static_cast<float>(framebuffer_height);
    projection_matrix_ = glm::perspective(
        field_of_view, aspect_ratio, near_plane, far_plane);
}

const glm::mat4& Camera::view_matrix() const
{
    return view_matrix_;
}

const glm::mat4& Camera::projection_matrix() const
{
    return projection_matrix_;
}

}
