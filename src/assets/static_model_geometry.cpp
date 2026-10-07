#include "assets/static_model_geometry.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace mmo::assets {
namespace {

bool is_finite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::mat4 node_transform(const animation::Transform& transform)
{
    const animation::Quaternion& rotation = transform.rotation;
    const glm::quat quaternion{
        rotation.w, rotation.x, rotation.y, rotation.z};
    const float quaternion_length_squared = glm::dot(quaternion, quaternion);
    const glm::vec3 translation{
        transform.translation[0], transform.translation[1], transform.translation[2]};
    const glm::vec3 scale{
        transform.scale[0], transform.scale[1], transform.scale[2]};
    if (!std::isfinite(quaternion_length_squared) ||
        quaternion_length_squared <= 0.000001f ||
        !is_finite(translation) || !is_finite(scale) ||
        std::abs(scale.x) <= 0.000001f ||
        std::abs(scale.y) <= 0.000001f ||
        std::abs(scale.z) <= 0.000001f) {
        throw std::invalid_argument("static model has a degenerate node transform");
    }
    glm::mat4 matrix = glm::translate(glm::mat4{1.0f},
        translation);
    matrix *= glm::mat4_cast(glm::normalize(quaternion));
    matrix = glm::scale(matrix, scale);
    return matrix;
}

bool is_skinned(const Model& model)
{
    if (!model.skeleton.bones.empty()) {
        return true;
    }
    return std::any_of(model.meshes.begin(), model.meshes.end(),
        [](const Mesh& mesh) {
            return std::any_of(mesh.vertices.begin(), mesh.vertices.end(),
                [](const ModelVertex& vertex) {
                    return std::any_of(vertex.bone_weights.begin(),
                        vertex.bone_weights.end(),
                        [](float weight) { return weight > 0.0f; });
                });
        });
}

void append_mesh(
    const Mesh& mesh,
    const glm::mat4& transform,
    std::vector<ModelVertex>& vertices)
{
    const glm::mat3 normal_transform =
        glm::transpose(glm::inverse(glm::mat3{transform}));
    const auto append = [&](std::size_t index) {
        if (index >= mesh.vertices.size()) {
            throw std::invalid_argument("static model mesh contains an invalid index");
        }
        ModelVertex vertex = mesh.vertices[index];
        const glm::vec3 source_position{
            vertex.position[0], vertex.position[1], vertex.position[2]};
        const glm::vec3 source_normal{
            vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        if (!is_finite(source_position) || !is_finite(source_normal)) {
            throw std::invalid_argument("static model has non-finite vertex data");
        }
        if (!std::all_of(vertex.color.begin(), vertex.color.end(),
                [](float component) { return std::isfinite(component); })) {
            throw std::invalid_argument("static model has non-finite vertex colors");
        }
        const glm::vec3 position = glm::vec3{
            transform * glm::vec4{source_position, 1.0f}};
        glm::vec3 transformed_normal = normal_transform * source_normal;
        const float normal_length_squared = glm::dot(
            transformed_normal, transformed_normal);
        const glm::vec3 normal = normal_length_squared <= 0.000001f
            ? glm::vec3{0.0f, 1.0f, 0.0f}
            : glm::normalize(transformed_normal);
        if (!is_finite(position) || !is_finite(normal)) {
            throw std::invalid_argument("static model transform produced non-finite vertices");
        }
        vertex.position = {position.x, position.y, position.z};
        vertex.normal = {normal.x, normal.y, normal.z};
        vertex.bone_indices = {};
        vertex.bone_weights = {};
        vertices.push_back(vertex);
    };
    if (mesh.indices.empty()) {
        for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
            append(index);
        }
    } else {
        for (const std::uint32_t index : mesh.indices) {
            append(index);
        }
    }
}

}

StaticModelGeometry build_static_model_geometry(const Model& model)
{
    std::string reason;
    if (!model.validate(&reason)) {
        throw std::invalid_argument("cannot build geometry for invalid model: " + reason);
    }
    if (is_skinned(model)) {
        throw std::invalid_argument("skinned models are not supported as editor assets yet");
    }

    StaticModelGeometry geometry;
    std::vector<bool> visited_meshes(model.meshes.size(), false);
    if (model.nodes.empty()) {
        for (const Mesh& mesh : model.meshes) {
            append_mesh(mesh, glm::mat4{1.0f}, geometry.vertices);
        }
    } else {
        std::vector<glm::mat4> global_transforms(model.nodes.size());
        std::vector<bool> transforms_ready(model.nodes.size(), false);
        std::function<const glm::mat4&(std::size_t)> get_global_transform =
            [&](std::size_t index) -> const glm::mat4& {
                if (transforms_ready[index]) {
                    return global_transforms[index];
                }
                const Node& node = model.nodes[index];
                const glm::mat4 local = node_transform(node.local_transform);
                global_transforms[index] = node.parent_index == animation::kNoBone
                    ? local
                    : get_global_transform(node.parent_index) * local;
                transforms_ready[index] = true;
                return global_transforms[index];
            };

        for (std::size_t node_index = 0; node_index < model.nodes.size(); ++node_index) {
            for (const std::size_t mesh_index : model.nodes[node_index].meshes) {
                append_mesh(model.meshes[mesh_index],
                    get_global_transform(node_index), geometry.vertices);
                visited_meshes[mesh_index] = true;
            }
        }
        for (std::size_t index = 0; index < model.meshes.size(); ++index) {
            if (!visited_meshes[index]) {
                append_mesh(model.meshes[index], glm::mat4{1.0f}, geometry.vertices);
            }
        }
    }
    if (geometry.vertices.empty()) {
        throw std::invalid_argument("static model contains no renderable geometry");
    }

    const auto to_vec3 = [](const ModelVertex& vertex) {
        return glm::vec3{vertex.position[0], vertex.position[1], vertex.position[2]};
    };
    geometry.minimum = to_vec3(geometry.vertices.front());
    geometry.maximum = geometry.minimum;
    for (const ModelVertex& vertex : geometry.vertices) {
        geometry.minimum = glm::min(geometry.minimum, to_vec3(vertex));
        geometry.maximum = glm::max(geometry.maximum, to_vec3(vertex));
    }
    const glm::vec3 center = (geometry.minimum + geometry.maximum) * 0.5f;
    geometry.minimum -= center;
    geometry.maximum -= center;
    for (ModelVertex& vertex : geometry.vertices) {
        const glm::vec3 position = to_vec3(vertex) - center;
        vertex.position = {position.x, position.y, position.z};
    }
    return geometry;
}

}
