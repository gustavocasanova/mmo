#pragma once

#include "renderer/shader.hpp"

#include <glm/vec3.hpp>

namespace mmo::renderer {

class Material {
public:
    explicit Material(glm::vec3 tint = {1.0f, 1.0f, 1.0f}, bool ground = false)
        : tint_(tint), ground_(ground) {}

    void apply(const Shader& shader) const;

private:
    glm::vec3 tint_;
    bool ground_;
};

}
