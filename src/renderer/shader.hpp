#pragma once
#include "math/transform.hpp"
#include "scene/material.hpp"
#include <span>
#include <array>
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
    void set_skinning(std::span<const math::Mat4> matrices) const;
private:
    unsigned int id_ = 0;
    int projection_location_ = -1;
    int view_location_ = -1;
    int model_location_ = -1;
    int tint_location_ = -1;
    int pattern_location_ = -1;
    std::array<int, 3> normal_locations_{};
    int skinning_location_ = -1;
    int skinned_location_ = -1;
};
}
