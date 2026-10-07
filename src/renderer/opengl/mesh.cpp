#include "renderer/mesh.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace mmo::renderer {
namespace {

void validate_vertices(const std::vector<MeshVertex>& vertices)
{
    if (vertices.empty() || vertices.size() % 3 != 0 ||
        vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::invalid_argument("character mesh must contain complete triangles");
    }

    for (const MeshVertex& vertex : vertices) {
        float weight_sum = 0.0f;
        for (std::size_t index = 0; index < vertex.bone_weights.size(); ++index) {
            const float weight = vertex.bone_weights[index];
            if (!std::isfinite(weight) || weight < 0.0f) {
                throw std::invalid_argument("character mesh has an invalid bone weight");
            }
            if (weight > 0.0f) {
                if (vertex.bone_indices[index] >= kMaxSkinningBones) {
                    throw std::length_error("character mesh exceeds the skinning palette limit");
                }
                weight_sum += weight;
            }
        }
        if (weight_sum > 0.0f && std::abs(weight_sum - 1.0f) > 0.01f) {
            throw std::invalid_argument("character mesh bone weights must be normalized");
        }
    }
}

}

Mesh::Mesh(const std::vector<MeshVertex>& vertices)
{
    validate_vertices(vertices);
    is_skinned_ = std::any_of(vertices.begin(), vertices.end(), [](const MeshVertex& vertex) {
        return std::any_of(vertex.bone_weights.begin(), vertex.bone_weights.end(),
            [](float weight) { return weight > 0.0f; });
    });

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (vao_ == 0 || vbo_ == 0) {
        if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
        if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("failed to allocate character mesh buffers");
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
        vertices.data(), GL_DYNAMIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(sizeof(MeshVertex));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(MeshVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_INT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(MeshVertex, bone_indices)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(MeshVertex, bone_weights)));
    glEnableVertexAttribArray(4);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertex_count_ = static_cast<int>(vertices.size());
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

void Mesh::update(const std::vector<MeshVertex>& vertices)
{
    validate_vertices(vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
        vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertex_count_ = static_cast<int>(vertices.size());
    is_skinned_ = std::any_of(vertices.begin(), vertices.end(), [](const MeshVertex& vertex) {
        return std::any_of(vertex.bone_weights.begin(), vertex.bone_weights.end(),
            [](float weight) { return weight > 0.0f; });
    });
}

void Mesh::draw() const
{
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    glBindVertexArray(0);
}

bool Mesh::is_skinned() const
{
    return is_skinned_;
}

}
