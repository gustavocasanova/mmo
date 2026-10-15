#pragma once

#include "animation/skeleton.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mmo::assets {

struct Texture {
    std::string name;
    std::filesystem::path source;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba8;
};

struct Material {
    std::string name;
    std::array<float, 4> base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    std::optional<std::size_t> base_color_texture;
};

struct ModelVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{0.0f, 1.0f, 0.0f};
    std::array<float, 2> texcoord{};
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<std::uint32_t, 4> bone_indices{};
    std::array<float, 4> bone_weights{};
};

struct Mesh {
    std::string name;
    std::vector<ModelVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::optional<std::size_t> material;
};

struct Node {
    std::string name;
    std::size_t parent_index = animation::kNoBone;
    animation::Transform local_transform{};
    std::vector<std::size_t> meshes;
};

struct Model {
    std::filesystem::path source_path;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Node> nodes;
    animation::Skeleton skeleton;
    std::vector<animation::AnimationClip> animations;

    [[nodiscard]] bool validate(std::string* reason = nullptr) const;
};

class ModelLoader {
public:
    virtual ~ModelLoader() = default;
    [[nodiscard]] virtual Model load(const std::filesystem::path& path) const = 0;
};

// Copies the clips of `source` into `target`; both models must share the same bone names.
// Clips whose name already exists in `target` are skipped.
void append_animations(Model& target, const Model& source);

class AssetSystem {
public:
    explicit AssetSystem(const ModelLoader& model_loader);

    [[nodiscard]] Model load_model(const std::filesystem::path& path) const;

private:
    const ModelLoader& model_loader_;
};

}