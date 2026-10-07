#pragma once

#include "assets/model.hpp"

#include <glm/vec3.hpp>

#include <vector>

namespace mmo::assets {

struct StaticModelGeometry {
    std::vector<ModelVertex> vertices;
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
};

[[nodiscard]] StaticModelGeometry build_static_model_geometry(const Model& model);

}
