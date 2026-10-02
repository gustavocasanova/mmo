#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace mmo::renderer {

class Camera {
public:
    void set_view(glm::vec3 eye, glm::vec3 target, glm::vec3 up = {0.0f, 1.0f, 0.0f});
    void set_projection(int framebuffer_width, int framebuffer_height,
        float field_of_view = 1.04719755f, float near_plane = 0.1f,
        float far_plane = 160.0f);

    [[nodiscard]] const glm::mat4& view_matrix() const;
    [[nodiscard]] const glm::mat4& projection_matrix() const;

private:
    glm::mat4 view_matrix_{1.0f};
    glm::mat4 projection_matrix_{1.0f};
};

}
