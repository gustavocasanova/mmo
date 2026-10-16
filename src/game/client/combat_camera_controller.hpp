#pragma once

#include "game/combat/combat.hpp"
#include "game/world/terrain.hpp"
#include "scene/camera_controller.hpp"

#include <optional>

namespace mmo::game::client {

using scene::CameraController;
using scene::CameraInput;
using scene::CameraPose;
using scene::CameraSettings;
using scene::CameraTarget;

struct CombatCameraSettings {
    CameraSettings orbit = [] {
        CameraSettings settings;
        settings.initial_distance = 4.5f;
        settings.minimum_distance = 1.8f;
        settings.maximum_distance = 10.0f;
        settings.target_offset = {0.0f, 1.35f, 0.0f};
        settings.minimum_pitch = -0.85f;
        settings.maximum_pitch = 1.05f;
        settings.zoom_speed = 0.12f;
        settings.distance_smoothing_seconds = 0.16f;
        settings.rotation_smoothing_seconds = 0.08f;
        settings.follow_rotation_smoothing_seconds = 0.22f;
        settings.collision_radius = 0.22f;
        settings.minimum_collision_distance = 0.7f;
        settings.rotate_target_with_camera = true;
        return settings;
    }();
    float lock_on_max_distance = 30.0f;
    float lock_on_field_of_view_degrees = 110.0f;
    float target_focus_height_fraction = 0.65f;
    float lock_aim_smoothing_seconds = 0.18f;
    float obstruction_grace_seconds = 0.35f;
    float terrain_clearance = 0.25f;
};

class CombatCameraController {
public:
    explicit CombatCameraController(CombatCameraSettings settings = {});

    void enter(CameraController& free_camera);
    void leave(CameraController& free_camera);
    [[nodiscard]] bool active() const;
    [[nodiscard]] bool locked() const;
    [[nodiscard]] std::optional<game::combat::EntityId> lock_target() const;
    [[nodiscard]] const CombatCameraSettings& settings() const;

    [[nodiscard]] std::optional<game::combat::EntityId> choose_lock_target(
        const game::combat::EntityRegistry& registry,
        const game::combat::TargetSystem& targets,
        const game::combat::CombatEntity& player,
        const CameraPose& view,
        const game::world::CollisionWorld& collision_world) const;
    void set_lock_target(game::combat::EntityId target);
    void clear_lock_target();

    [[nodiscard]] CameraPose update(
        float delta_seconds,
        const CameraTarget& player_view,
        const game::combat::CombatEntity& player_entity,
        const game::combat::CombatEntity* target_entity,
        const CameraInput& input,
        bool player_moving,
        const game::world::CollisionWorld& collision_world,
        const game::world::Terrain& terrain);

private:
    [[nodiscard]] bool in_lock_range(
        const game::combat::CombatEntity& player,
        const game::combat::CombatEntity& target) const;
    void keep_camera_above_terrain(CameraPose& pose,
        const game::world::Terrain& terrain) const;

    CombatCameraSettings settings_;
    CameraController camera_;
    std::optional<game::combat::EntityId> lock_target_;
    glm::vec3 aim_focus_{0.0f};
    float obstruction_time_ = 0.0f;
    bool active_ = false;
    bool aim_focus_initialized_ = false;
    bool manual_orbit_latched_ = false;
};

}