#include "assets/mesh_catalog.hpp"
#include <stdexcept>
#include <utility>

namespace mmo::assets {
MeshId MeshCatalog::add(MeshData data)
{
    if (data.vertices.empty() || data.vertices.size() % 3 != 0) {
        throw std::invalid_argument("mesh must contain complete triangles");
    }
    const MeshId id{meshes_.size()};
    meshes_.push_back(std::move(data));
    return id;
}
const MeshData& MeshCatalog::get(MeshId id) const { return meshes_.at(id.value); }
}
