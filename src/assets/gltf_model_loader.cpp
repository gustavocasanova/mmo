#include "assets/gltf_model_loader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmo::assets {
namespace {

using fastgltf::math::fmat4x4;
using fastgltf::math::fvec3;
using fastgltf::math::fvec4;

animation::Transform to_transform(const fastgltf::TRS& source)
{
    return {
        {source.translation[0], source.translation[1], source.translation[2]},
        {source.rotation[0], source.rotation[1], source.rotation[2], source.rotation[3]},
        {source.scale[0], source.scale[1], source.scale[2]},
    };
}

glm::mat4 node_transform(const fastgltf::Node& node)
{
    if (!std::holds_alternative<fastgltf::TRS>(node.transform)) {
        throw std::runtime_error("glTF node matrices must be decomposed before loading");
    }
    const fastgltf::TRS& trs = std::get<fastgltf::TRS>(node.transform);
    const glm::vec3 translation{trs.translation[0], trs.translation[1], trs.translation[2]};
    const glm::quat rotation{trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]};
    const glm::vec3 scale{trs.scale[0], trs.scale[1], trs.scale[2]};
    return glm::translate(glm::mat4{1.0f}, translation) *
        glm::mat4_cast(rotation) * glm::scale(glm::mat4{1.0f}, scale);
}

template <typename T>
std::vector<T> read_accessor(const fastgltf::Asset& asset, std::size_t index)
{
    if (index >= asset.accessors.size()) {
        throw std::runtime_error("glTF references a missing accessor");
    }
    const auto values = fastgltf::iterateAccessor<T>(asset, asset.accessors[index]);
    return {values.begin(), values.end()};
}

std::vector<std::uint32_t> read_indices(
    const fastgltf::Asset& asset,
    std::size_t accessor_index)
{
    const fastgltf::Accessor& accessor = asset.accessors.at(accessor_index);
    switch (accessor.componentType) {
    case fastgltf::ComponentType::UnsignedByte: {
        const auto values = read_accessor<std::uint8_t>(asset, accessor_index);
        return {values.begin(), values.end()};
    }
    case fastgltf::ComponentType::UnsignedShort: {
        const auto values = read_accessor<std::uint16_t>(asset, accessor_index);
        return {values.begin(), values.end()};
    }
    case fastgltf::ComponentType::UnsignedInt:
        return read_accessor<std::uint32_t>(asset, accessor_index);
    default:
        throw std::runtime_error("glTF mesh indices must use an unsigned integer type");
    }
}

std::vector<std::array<std::uint32_t, 4>> read_joints(
    const fastgltf::Asset& asset,
    std::size_t accessor_index)
{
    const fastgltf::Accessor& accessor = asset.accessors.at(accessor_index);
    std::vector<std::array<std::uint32_t, 4>> result;
    if (accessor.componentType == fastgltf::ComponentType::UnsignedByte) {
        for (const auto value :
            read_accessor<fastgltf::math::u8vec4>(asset, accessor_index)) {
            result.push_back({value[0], value[1], value[2], value[3]});
        }
    } else if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
        for (const auto value :
            read_accessor<fastgltf::math::u16vec4>(asset, accessor_index)) {
            result.push_back({value[0], value[1], value[2], value[3]});
        }
    } else {
        throw std::runtime_error("glTF JOINTS_0 must use unsigned byte or unsigned short");
    }
    return result;
}

std::vector<std::size_t> node_parent_indices(const fastgltf::Asset& asset)
{
    std::vector<std::size_t> parent(asset.nodes.size(), animation::kNoBone);
    for (std::size_t parent_index = 0; parent_index < asset.nodes.size(); ++parent_index) {
        for (std::size_t child : asset.nodes[parent_index].children) {
            if (child >= asset.nodes.size() || parent[child] != animation::kNoBone) {
                throw std::runtime_error("glTF node hierarchy has an invalid or shared child");
            }
            parent[child] = parent_index;
        }
    }
    return parent;
}

void load_skeleton(const fastgltf::Asset& source, Model& destination,
    std::vector<std::size_t>& node_to_bone)
{
    if (source.skins.empty()) {
        return;
    }
    if (source.skins.size() != 1) {
        throw std::runtime_error("this character loader currently supports one skin per model");
    }

    const fastgltf::Skin& skin = source.skins.front();
    if (skin.joints.size() + 1 > animation::kMaxSkinningBones) {
        throw std::runtime_error("character skeleton exceeds the skinning palette limit");
    }

    node_to_bone.assign(source.nodes.size(), animation::kNoBone);
    animation::Bone skeleton_root;
    skeleton_root.name = "__skeleton_root";
    (void)destination.skeleton.add_bone(std::move(skeleton_root));
    for (std::size_t bone_index = 0; bone_index < skin.joints.size(); ++bone_index) {
        const std::size_t node_index = skin.joints[bone_index];
        if (node_index >= source.nodes.size()) {
            throw std::runtime_error("glTF skin references a missing joint node");
        }
        node_to_bone[node_index] = bone_index + 1;
    }

    const std::vector<std::size_t> parents = node_parent_indices(source);
    std::vector<animation::Matrix4> inverse_bind_matrices(skin.joints.size());
    for (animation::Matrix4& matrix : inverse_bind_matrices) {
        matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    }
    if (skin.inverseBindMatrices) {
        const auto values = read_accessor<fmat4x4>(source, *skin.inverseBindMatrices);
        if (values.size() != skin.joints.size()) {
            throw std::runtime_error("glTF inverse bind matrix count does not match its joints");
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            for (std::size_t column = 0; column < 4; ++column) {
                for (std::size_t row = 0; row < 4; ++row) {
                    inverse_bind_matrices[index][column * 4 + row] =
                        values[index][column][row];
                }
            }
        }
    }

    for (std::size_t bone_index = 0; bone_index < skin.joints.size(); ++bone_index) {
        const std::size_t node_index = skin.joints[bone_index];
        const fastgltf::Node& node = source.nodes[node_index];
        std::size_t parent_node = parents[node_index];
        std::vector<std::size_t> intervening_nodes;
        while (parent_node != animation::kNoBone &&
            node_to_bone[parent_node] == animation::kNoBone) {
            intervening_nodes.push_back(parent_node);
            parent_node = parents[parent_node];
        }

        animation::Bone bone;
        bone.name = node.name.empty()
            ? "joint_" + std::to_string(bone_index)
            : std::string(node.name);
        bone.parent_index = parent_node == animation::kNoBone
            ? 0
            : node_to_bone[parent_node];
        glm::mat4 local_transform = node_transform(node);
        for (auto iterator = intervening_nodes.rbegin();
             iterator != intervening_nodes.rend(); ++iterator) {
            local_transform = node_transform(source.nodes[*iterator]) * local_transform;
        }
        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;
        glm::vec3 skew;
        glm::vec4 perspective;
        if (!glm::decompose(local_transform, scale, rotation, translation, skew, perspective)) {
            throw std::runtime_error("could not decompose glTF joint transform");
        }
        bone.bind_local_transform = {
            {translation.x, translation.y, translation.z},
            {rotation.x, rotation.y, rotation.z, rotation.w},
            {scale.x, scale.y, scale.z},
        };
        bone.inverse_bind_matrix = inverse_bind_matrices[bone_index];
        (void)destination.skeleton.add_bone(std::move(bone));
    }
}

std::array<float, 4> primitive_color(
    const fastgltf::Asset& source,
    const fastgltf::Primitive& primitive)
{
    if (!primitive.materialIndex || *primitive.materialIndex >= source.materials.size()) {
        return {0.82f, 0.78f, 0.68f, 1.0f};
    }
    const auto& factor = source.materials[*primitive.materialIndex].pbrData.baseColorFactor;
    return {factor[0], factor[1], factor[2], factor[3]};
}

void load_meshes(const fastgltf::Asset& source, Model& destination,
    const std::vector<std::size_t>& node_to_bone)
{
    for (std::size_t node_index = 0; node_index < source.nodes.size(); ++node_index) {
        const fastgltf::Node& source_node = source.nodes[node_index];
        if (!source_node.meshIndex) {
            continue;
        }
        if (*source_node.meshIndex >= source.meshes.size()) {
            throw std::runtime_error("glTF node references a missing mesh");
        }
        Node node;
        node.name = source_node.name;
        for (const fastgltf::Primitive& primitive :
            source.meshes[*source_node.meshIndex].primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                throw std::runtime_error("character mesh primitives must be triangles");
            }
            const auto position_attribute = primitive.findAttribute("POSITION");
            if (position_attribute == primitive.attributes.end()) {
                throw std::runtime_error("glTF mesh primitive has no POSITION attribute");
            }
            const auto positions = read_accessor<fvec3>(source, position_attribute->accessorIndex);
            const auto normal_attribute = primitive.findAttribute("NORMAL");
            const auto normals = normal_attribute == primitive.attributes.end()
                ? std::vector<fvec3>{}
                : read_accessor<fvec3>(source, normal_attribute->accessorIndex);
            const auto joints_attribute = primitive.findAttribute("JOINTS_0");
            const auto joints = joints_attribute == primitive.attributes.end()
                ? std::vector<std::array<std::uint32_t, 4>>{}
                : read_joints(source, joints_attribute->accessorIndex);
            const auto weights_attribute = primitive.findAttribute("WEIGHTS_0");
            const auto weights = weights_attribute == primitive.attributes.end()
                ? std::vector<fvec4>{}
                : read_accessor<fvec4>(source, weights_attribute->accessorIndex);

            if ((!normals.empty() && normals.size() != positions.size()) ||
                (!joints.empty() && joints.size() != positions.size()) ||
                (!weights.empty() && weights.size() != positions.size()) ||
                (joints.empty() != weights.empty())) {
                throw std::runtime_error("glTF vertex attribute counts do not match");
            }

            Mesh mesh;
            mesh.name = source.meshes[*source_node.meshIndex].name;
            mesh.vertices.reserve(positions.size());
            const auto color = primitive_color(source, primitive);
            for (std::size_t index = 0; index < positions.size(); ++index) {
                ModelVertex vertex;
                vertex.position = {positions[index][0], positions[index][1], positions[index][2]};
                if (!normals.empty()) {
                    vertex.normal = {normals[index][0], normals[index][1], normals[index][2]};
                }
                vertex.color = color;
                if (!joints.empty()) {
                    vertex.bone_indices = joints[index];
                    for (std::uint32_t& joint_index : vertex.bone_indices) {
                        if (source_node.skinIndex) {
                            const fastgltf::Skin& skin =
                                source.skins.at(*source_node.skinIndex);
                            if (joint_index >= skin.joints.size()) {
                                throw std::runtime_error(
                                    "glTF vertex refers to a missing skin joint");
                            }
                            joint_index = static_cast<std::uint32_t>(
                                node_to_bone.at(skin.joints[joint_index]));
                        }
                    }
                    vertex.bone_weights = {
                        weights[index][0], weights[index][1], weights[index][2], weights[index][3]};
                }
                mesh.vertices.push_back(vertex);
            }
            if (primitive.indicesAccessor) {
                mesh.indices = read_indices(source, *primitive.indicesAccessor);
            }
            node.meshes.push_back(destination.meshes.size());
            destination.meshes.push_back(std::move(mesh));
        }
        destination.nodes.push_back(std::move(node));
    }
}

void load_animations(const fastgltf::Asset& source, Model& destination,
    const std::vector<std::size_t>& node_to_bone)
{
    for (const fastgltf::Animation& source_animation : source.animations) {
        animation::AnimationClip clip;
        clip.name = source_animation.name;
        for (const fastgltf::AnimationChannel& source_channel : source_animation.channels) {
            if (!source_channel.nodeIndex ||
                *source_channel.nodeIndex >= node_to_bone.size()) {
                continue;
            }
            const std::size_t bone_index = node_to_bone[*source_channel.nodeIndex];
            if (bone_index == animation::kNoBone) {
                continue;
            }
            const fastgltf::AnimationSampler& sampler =
                source_animation.samplers.at(source_channel.samplerIndex);
            if (sampler.interpolation != fastgltf::AnimationInterpolation::Linear &&
                sampler.interpolation != fastgltf::AnimationInterpolation::Step) {
                throw std::runtime_error(
                    "character animations must use LINEAR or STEP interpolation for now");
            }
            const animation::Interpolation interpolation =
                sampler.interpolation == fastgltf::AnimationInterpolation::Step
                ? animation::Interpolation::Step
                : animation::Interpolation::Linear;
            const std::vector<float> times = read_accessor<float>(source, sampler.inputAccessor);
            if (times.empty()) {
                continue;
            }
            clip.duration = std::max(clip.duration, times.back());
            animation::AnimationChannel channel;
            channel.bone_index = bone_index;
            switch (source_channel.path) {
            case fastgltf::AnimationPath::Translation: {
                const auto values = read_accessor<fvec3>(source, sampler.outputAccessor);
                if (values.size() != times.size()) {
                    throw std::runtime_error("glTF translation animation key count mismatch");
                }
                channel.translations.times = times;
                channel.translations.interpolation = interpolation;
                for (const fvec3& value : values) {
                    channel.translations.values.push_back({value[0], value[1], value[2]});
                }
                break;
            }
            case fastgltf::AnimationPath::Rotation: {
                const auto values = read_accessor<fvec4>(source, sampler.outputAccessor);
                if (values.size() != times.size()) {
                    throw std::runtime_error("glTF rotation animation key count mismatch");
                }
                channel.rotations.times = times;
                channel.rotations.interpolation = interpolation;
                for (const fvec4& value : values) {
                    channel.rotations.values.push_back(
                        {value[0], value[1], value[2], value[3]});
                }
                break;
            }
            case fastgltf::AnimationPath::Scale: {
                const auto values = read_accessor<fvec3>(source, sampler.outputAccessor);
                if (values.size() != times.size()) {
                    throw std::runtime_error("glTF scale animation key count mismatch");
                }
                channel.scales.times = times;
                channel.scales.interpolation = interpolation;
                for (const fvec3& value : values) {
                    channel.scales.values.push_back({value[0], value[1], value[2]});
                }
                break;
            }
            case fastgltf::AnimationPath::Weights:
                continue;
            }
            clip.channels.push_back(std::move(channel));
        }
        if (!clip.channels.empty()) {
            destination.animations.push_back(std::move(clip));
        }
    }
}

}

Model GltfModelLoader::load(const std::filesystem::path& path) const
{
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        throw std::runtime_error("could not read glTF asset: " +
            std::string(fastgltf::getErrorMessage(data.error())));
    }

    fastgltf::Parser parser;
    auto parsed = parser.loadGltf(data.get(), path.parent_path(),
        fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::LoadExternalBuffers);
    if (parsed.error() != fastgltf::Error::None) {
        throw std::runtime_error("could not parse glTF asset: " +
            std::string(fastgltf::getErrorMessage(parsed.error())));
    }

    Model model;
    model.source_path = path;
    const fastgltf::Asset& source = parsed.get();
    std::vector<std::size_t> node_to_bone;
    load_skeleton(source, model, node_to_bone);
    load_meshes(source, model, node_to_bone);
    load_animations(source, model, node_to_bone);
    return model;
}

}
