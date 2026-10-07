#pragma once

#include "assets/model.hpp"
#include "animation/skeleton.hpp"
#include "game/world/collision_world.hpp"
#include "game/world/terrain.hpp"
#include "scene/camera.hpp"

#include <memory>
#include <optional>
#include <span>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace mmo::renderer {

struct RenderBox {
    std::uint64_t id;
    glm::vec3 center;
    glm::vec3 half_extents;
    glm::vec3 rotation_degrees;
    glm::vec3 scale;
};

enum class GizmoMode {
    translate,
    rotate,
    scale,
};

struct CharacterPlacement {
    float x;
    float y;
    float z;
    float yaw;
};

class Renderer {
public:
    Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void set_character_model(const assets::Model& model);
    void set_skinning_matrices(std::span<const animation::Matrix4> matrices);
    void set_terrain(const game::world::Terrain& terrain);
    void set_brush_cursor(
        const game::world::Terrain& terrain,
        glm::vec2 center,
        float radius,
        bool visible);

    void render(
        int framebuffer_width,
        int framebuffer_height,
        const CharacterPlacement& player,
        const scene::CameraPose& camera,
        const game::world::CollisionWorld& collision_world,
        std::optional<std::uint64_t> selected_object = std::nullopt,
        std::span<const RenderBox> editor_boxes = {},
        bool show_editor_gizmo = false,
        glm::vec3 gizmo_position = glm::vec3{0.0f},
        GizmoMode gizmo_mode = GizmoMode::translate) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}