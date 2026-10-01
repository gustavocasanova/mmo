#pragma once

#include <cstddef>
#include <vector>

namespace mmo::assets {
struct Vertex {
    float position[3];
    float normal[3];
    float color[3];
};
struct MeshData { std::vector<Vertex> vertices; };
// IDs belong to the catalog that produced them; never serialize them as game IDs.
struct MeshId {
    std::size_t value;
    bool operator==(const MeshId&) const = default;
};
}
