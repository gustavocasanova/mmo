#include "assets/model.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmo::assets {
namespace {

bool fail(std::string* reason, const char* message)
{
    if (reason != nullptr) {
        *reason = message;
    }
    return false;
}

bool valid_extension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension == ".glb" || extension == ".gltf";
}

}

bool Model::validate(std::string* reason) const
{
    if (!skeleton.is_valid()) {
        return fail(reason, "model has an invalid skeleton");
    }

    for (const Mesh& mesh : meshes) {
        if (mesh.vertices.empty()) {
            return fail(reason, "model mesh has no vertices");
        }
        if (!mesh.indices.empty() && mesh.indices.size() % 3 != 0) {
            return fail(reason, "triangle mesh index count is not divisible by three");
        }
        for (std::uint32_t index : mesh.indices) {
            if (index >= mesh.vertices.size()) {
                return fail(reason, "mesh index refers to a missing vertex");
            }
        }
        if (mesh.material && *mesh.material >= materials.size()) {
            return fail(reason, "mesh refers to a missing material");
        }
        for (const ModelVertex& vertex : mesh.vertices) {
            float weight_sum = 0.0f;
            for (std::size_t influence = 0; influence < vertex.bone_weights.size(); ++influence) {
                const float weight = vertex.bone_weights[influence];
                if (!std::isfinite(weight) || weight < 0.0f) {
                    return fail(reason, "vertex bone weight is invalid");
                }
                if (weight > 0.0f) {
                    if (vertex.bone_indices[influence] >= skeleton.bones.size()) {
                        return fail(reason, "vertex refers to a missing bone");
                    }
                    weight_sum += weight;
                }
            }
            if (weight_sum > 0.0f && std::abs(weight_sum - 1.0f) > 0.01f) {
                return fail(reason, "vertex bone weights must be normalized");
            }
        }
    }

    for (const Material& material : materials) {
        if (material.base_color_texture && *material.base_color_texture >= textures.size()) {
            return fail(reason, "material refers to a missing texture");
        }
    }
    for (const animation::AnimationClip& clip : animations) {
        if (!clip.is_valid(skeleton.bones.size())) {
            return fail(reason, "model contains an invalid animation clip");
        }
    }

    std::vector<unsigned char> node_state(nodes.size(), 0);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const Node& node = nodes[index];
        if (node.parent_index != animation::kNoBone &&
            (node.parent_index >= nodes.size() || node.parent_index == index)) {
            return fail(reason, "model node has an invalid parent");
        }
        for (std::size_t mesh_index : node.meshes) {
            if (mesh_index >= meshes.size()) {
                return fail(reason, "model node refers to a missing mesh");
            }
        }
    }
    const auto visit_node = [&](auto&& self, std::size_t index) -> bool {
        if (node_state[index] == 1) {
            return false;
        }
        if (node_state[index] == 2) {
            return true;
        }
        node_state[index] = 1;
        const std::size_t parent = nodes[index].parent_index;
        if (parent != animation::kNoBone && !self(self, parent)) {
            return false;
        }
        node_state[index] = 2;
        return true;
    };
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        if (!visit_node(visit_node, index)) {
            return fail(reason, "model node hierarchy contains a cycle");
        }
    }
    return true;
}

AssetSystem::AssetSystem(const ModelLoader& model_loader)
    : model_loader_(model_loader)
{
}

Model AssetSystem::load_model(const std::filesystem::path& path) const
{
    if (!valid_extension(path)) {
        throw std::invalid_argument("model assets must use .glb or .gltf");
    }
    Model model = model_loader_.load(path);
    std::string reason;
    if (!model.validate(&reason)) {
        throw std::runtime_error("invalid model asset: " + reason);
    }
    model.source_path = path;
    return model;
}

}