#pragma once
#include "assets/mesh_data.hpp"
#include "scene/material.hpp"
namespace mmo::scene {
struct DrawItem {
    assets::MeshId mesh;
    math::Mat4 transform{1.0f};
    Material material;
};
}
