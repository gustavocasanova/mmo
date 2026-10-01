#pragma once
#include "math/transform.hpp"
#include "scene/material.hpp"
namespace mmo::renderer {
// Owns a GPU program; construction and destruction require the current context.
class Shader {
public:
    Shader();
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    void bind() const;
    void set_matrices(const math::Mat4&, const math::Mat4&, const math::Mat4&) const;
    void set_tint(math::Vec3) const;
    void set_pattern(scene::SurfacePattern) const;
private:
    unsigned int id_ = 0;
    int projection_location_ = -1;
    int view_location_ = -1;
    int model_location_ = -1;
    int tint_location_ = -1;
    int pattern_location_ = -1;
};
}
