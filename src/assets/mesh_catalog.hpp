#pragma once

#include "assets/mesh_data.hpp"
#include <span>

namespace mmo::assets {
// CPU-owned mesh data. No graphics context is required.
class MeshCatalog {
public:
    MeshId add(MeshData data);
    const MeshData& get(MeshId id) const;
    std::span<const MeshData> meshes() const { return meshes_; }
private:
    std::vector<MeshData> meshes_;
};
}
