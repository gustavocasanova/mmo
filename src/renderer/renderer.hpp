#pragma once

#include "assets/model.hpp"
#include "animation/skeleton.hpp"
#include "world/collision_world.hpp"
#include "world/terrain.hpp"
#include "scene/camera.hpp"

#include <memory>
#include <optional>
#include <span>
#include <cstdint>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace mmo::renderer {

struct RenderBox {
    std::uint64_t id;
    glm::vec3 center;
    glm::vec3 half_extents;
    glm::vec3 rotation_degrees;
    glm::vec3 scale;
    std::string asset_path;
};

struct ModelBounds {
    glm::vec3 minimum;
    glm::vec3 maximum;
};

enum class GizmoMode {
    translate,
    rotate,
    scale,
};

struct Placement {
    float x;
    float y;
    float z;
    float yaw;
};

// Rigid registered static model, positioned in the skinned instance's model space.
struct RigidAttachment {
    std::string asset_path;
    animation::Matrix4 transform;
};

// One drawable copy of the skinned model. The renderer does not know whether it
// is a player, an NPC or an enemy; spans must stay valid until the next set call.
struct SkinnedInstance {
    Placement placement;
    glm::vec3 tint{1.0f};
    std::span<const animation::Matrix4> skin_matrices;
    std::span<const RigidAttachment> attachments;
};

class Renderer {
public:
    Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void set_skinned_model(const assets::Model& model);
    [[nodiscard]] ModelBounds register_static_model(
        const std::string& asset_path,
        const assets::Model& model);
    [[nodiscard]] ModelBounds register_editor_model(
        const std::string& asset_path,
        const assets::Model& model);
    // Copies the instances; attachments must reference registered static models.
    void set_skinned_instances(std::span<const SkinnedInstance> instances);
    // Ground ring under the current target; decoupled from any gameplay system.
    void set_target_indicator(bool visible, glm::vec3 center, float radius, glm::vec3 color);
    void set_terrain(const world::Terrain& terrain);
    void set_brush_cursor(
        const world::Terrain& terrain,
        glm::vec2 center,
        float radius,
        bool visible);

    void render(
        int framebuffer_width,
        int framebuffer_height,
        const scene::CameraPose& camera,
        const world::CollisionWorld& collision_world,
        std::optional<std::uint64_t> selected_object = std::nullopt,
        std::span<const RenderBox> editor_boxes = {},
        bool show_editor_gizmo = false,
        glm::vec3 gizmo_position = glm::vec3{0.0f},
        GizmoMode gizmo_mode = GizmoMode::translate,
        bool gizmo_local_space = false) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
