#pragma once

#include "animation/skeleton.hpp"

#include <glm/vec3.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mmo::renderer {

inline constexpr std::size_t kMaxSkinningBones = animation::kMaxSkinningBones;

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    std::array<std::uint32_t, 4> bone_indices{};
    std::array<float, 4> bone_weights{};
};

class Mesh {
public:
    explicit Mesh(const std::vector<MeshVertex>& vertices);
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh();

    void update(const std::vector<MeshVertex>& vertices);
    void draw() const;
    [[nodiscard]] bool is_skinned() const;

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int vertex_count_ = 0;
    bool is_skinned_ = false;
};

}
