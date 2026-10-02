#pragma once
#include "assets/mesh_data.hpp"
#include "scene/material.hpp"
#include <span>
namespace mmo::scene {
struct DrawItem {
    assets::MeshId mesh;
    math::Mat4 transform{1.0f};
    Material material;
    // Borrowed until render returns; empty means a static draw.
    std::span<const math::Mat4> skinning_matrices{};
};
}
