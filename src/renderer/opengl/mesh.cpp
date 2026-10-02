#include "renderer/mesh.hpp"
#include <glad/gl.h>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace mmo::renderer {
using assets::Vertex;
namespace {
std::size_t validate_mesh(const assets::MeshData& data)
{
    const auto& vertices = data.vertices;
    if (vertices.empty() || vertices.size() % 3 != 0 ||
        vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::invalid_argument("invalid triangle mesh size");
    }
    std::size_t required_bones = 0;
    for (const auto& vertex : vertices) {
        for (std::size_t i = 0; i < vertex.bone_weights.size(); ++i) {
            const auto weight = vertex.bone_weights[i];
            if (!std::isfinite(weight) || weight < 0) throw std::invalid_argument("invalid bone weight");
            if (weight > 0) {
                if (vertex.bone_indices[i] >= assets::kMaxSkinningBones) {
                    throw std::length_error("mesh exceeds the 48-bone skinning limit");
                }
                required_bones = std::max(required_bones, static_cast<std::size_t>(vertex.bone_indices[i]) + 1);
            }
        }
    }
    return required_bones;
}
}

Mesh::Mesh(const assets::MeshData& data)
{
    required_bones_ = validate_mesh(data);
    const auto& vertices = data.vertices;
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (vao_ == 0 || vbo_ == 0) {
        if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
        if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("failed to allocate VAO/VBO");
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_INT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, bone_indices)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, bone_weights)));
    glEnableVertexAttribArray(4);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertex_count_ = static_cast<GLsizei>(vertices.size());
}

Mesh::~Mesh()
{
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
}

void Mesh::update(const assets::MeshData& data)
{
    const auto required_bones = validate_mesh(data);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
        data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertex_count_ = static_cast<GLsizei>(data.vertices.size());
    required_bones_ = required_bones;
}

void Mesh::draw() const
{
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    glBindVertexArray(0);
}

}
