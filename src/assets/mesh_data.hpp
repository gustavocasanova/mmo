#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <vector>

namespace mmo::assets {
inline constexpr std::size_t kMaxSkinningBones = 48;
struct Vertex {
    float position[3];
    float normal[3];
    float color[3];
    std::array<std::uint32_t, 4> bone_indices{};
    std::array<float, 4> bone_weights{};
};
struct MeshData { std::vector<Vertex> vertices; };
// IDs belong to the catalog that produced them; never serialize them as game IDs.
struct MeshId {
    std::size_t value;
    bool operator==(const MeshId&) const = default;
};
}
