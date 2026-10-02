#include "renderer/material.hpp"

namespace mmo::renderer {

void Material::apply(const Shader& shader) const
{
    shader.set_vec3("u_tint", tint_);
    shader.set_int("u_is_ground", ground_ ? 1 : 0);
}

}
