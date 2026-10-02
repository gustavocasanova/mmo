#pragma once
#include "assets/mesh_data.hpp"
namespace mmo::renderer {
// GPU allocation is separate from CPU asset data.
class Mesh {
public:
    explicit Mesh(const assets::MeshData& data);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    void update(const assets::MeshData& data);
    void draw() const;
    std::size_t required_bones() const { return required_bones_; }
private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int vertex_count_ = 0;
    std::size_t required_bones_ = 0;
};
}
