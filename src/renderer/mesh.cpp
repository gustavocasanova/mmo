#include "renderer/mesh.hpp"

#include <glad/gl.h>

#include <cstddef>
#include <stdexcept>
#include <algorithm>

namespace mmo::renderer {

Mesh::Mesh(const std::vector<MeshVertex>& vertices)
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (vao_ == 0 || vbo_ == 0) {
        throw std::runtime_error("failed to allocate mesh buffers");
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
        vertices.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(sizeof(MeshVertex));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
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
    is_skinned_ = std::any_of(vertices.begin(), vertices.end(), [](const MeshVertex& vertex) {
        return std::any_of(vertex.bone_weights.begin(), vertex.bone_weights.end(),
            [](float weight) { return weight > 0.0f; });
    });
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
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
        vertices.data(), GL_STATIC_DRAW);
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
