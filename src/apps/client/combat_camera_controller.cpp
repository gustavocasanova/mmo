#include "apps/client/combat_camera_controller.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/geometric.hpp>

namespace mmo::game::client {

using scene::CameraController;
using scene::CameraInput;
using scene::CameraPose;
using scene::CameraTarget;
namespace {

constexpr float kPi = 3.14159265f;

float wrap_angle(float angle)
{
    return std::remainder(angle, 2.0f * kPi);
}

glm::vec3 target_focus(const game::combat::CombatEntity& target, float height_fraction)
{
    return target.position + glm::vec3{0.0f, target.height * height_fraction, 0.0f};
}

float horizontal_distance(glm::vec3 a, glm::vec3 b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}

bool visible_from(
    glm::vec3 origin,
    glm::vec3 destination,
    const world::CollisionWorld& collision_world)
{
    return !game::combat::segment_blocked(
        origin, destination, collision_world.boxes());
}

}

CombatCameraController::CombatCameraController(CombatCameraSettings settings)
    : settings_(std::move(settings))
    , camera_(settings_.orbit)
{
    if (!std::isfinite(settings_.lock_on_max_distance) ||
        !std::isfinite(settings_.lock_on_field_of_view_degrees) ||
        !std::isfinite(settings_.target_focus_height_fraction) ||
        !std::isfinite(settings_.lock_aim_smoothing_seconds) ||
        !std::isfinite(settings_.obstruction_grace_seconds) ||
        !std::isfinite(settings_.terrain_clearance) ||
        settings_.lock_on_max_distance <= 0.0f ||
        settings_.lock_on_field_of_view_degrees <= 0.0f ||
        settings_.lock_on_field_of_view_degrees > 360.0f ||
        settings_.target_focus_height_fraction < 0.0f ||
        settings_.target_focus_height_fraction > 1.0f ||
        settings_.lock_aim_smoothing_seconds < 0.0f ||
        settings_.obstruction_grace_seconds < 0.0f ||
        settings_.terrain_clearance < 0.0f) {
        throw std::invalid_argument("combat camera settings are outside valid ranges");
    }
}

void CombatCameraController::enter(CameraController& free_camera)
{
    if (active_) {
        return;
    }
    camera_.adopt_view_from(free_camera);
    active_ = true;
    camera_.set_follow_target_yaw(std::nullopt);
}

void CombatCameraController::leave(CameraController& free_camera)
{
    if (!active_) {
        return;
    }
    clear_lock_target();
    free_camera.adopt_view_from(camera_);
    active_ = false;
}

bool CombatCameraController::active() const
{
    return active_;
}

bool CombatCameraController::locked() const
{
    return lock_target_.has_value();
}

std::optional<game::combat::EntityId> CombatCameraController::lock_target() const
{
    return lock_target_;
}

const CombatCameraSettings& CombatCameraController::settings() const
{
    return settings_;
}

bool CombatCameraController::in_lock_range(
    const game::combat::CombatEntity& player,
    const game::combat::CombatEntity& target) const
{
    return horizontal_distance(player.position, target.position) <=
        settings_.lock_on_max_distance;
}

std::optional<game::combat::EntityId> CombatCameraController::choose_lock_target(
    const game::combat::EntityRegistry& registry,
    const game::combat::TargetSystem& targets,
    const game::combat::CombatEntity& player,
    const CameraPose& view,
    const world::CollisionWorld& collision_world) const
{
    const float maximum_distance = std::min(
        settings_.lock_on_max_distance, targets.settings().max_distance);
    const glm::vec3 eye{view.eye_x, view.eye_y, view.eye_z};
    const glm::vec3 view_direction{view.focus_x - view.eye_x, 0.0f,
        view.focus_z - view.eye_z};
    const float view_length = glm::length(view_direction);
    if (view_length <= 0.0001f) {
        return std::nullopt;
    }
    const float view_yaw = std::atan2(view_direction.x, view_direction.z);
    const float half_fov = glm::radians(
        settings_.lock_on_field_of_view_degrees * 0.5f);

    const auto is_eligible = [&](const game::combat::CombatEntity& candidate) {
        return game::combat::TargetSystem::is_valid_target(candidate, player) &&
            horizontal_distance(player.position, candidate.position) <= maximum_distance &&
            visible_from(eye, target_focus(candidate,
                settings_.target_focus_height_fraction), collision_world);
    };
    if (const game::combat::CombatEntity* current = registry.find(targets.current());
        current != nullptr && is_eligible(*current)) {
        return current->id;
    }

    std::optional<game::combat::EntityId> best;
    float best_score = std::numeric_limits<float>::max();
    for (const game::combat::CombatEntity& candidate : registry.entities()) {
        if (!is_eligible(candidate)) {
            continue;
        }
        const glm::vec3 direction = target_focus(
            candidate, settings_.target_focus_height_fraction) - eye;
        const float target_yaw = std::atan2(direction.x, direction.z);
        const float angle = std::abs(wrap_angle(target_yaw - view_yaw));
        if (angle > half_fov) {
            continue;
        }
        const float distance = horizontal_distance(player.position, candidate.position);
        const float angle_score = half_fov > 0.0f ? angle / half_fov : 0.0f;
        const float distance_score = maximum_distance > 0.0f
            ? distance / maximum_distance : 1.0f;
        const float score = angle_score * 0.65f + distance_score * 0.35f;
        if (score < best_score) {
            best_score = score;
            best = candidate.id;
        }
    }
    return best;
}

void CombatCameraController::set_lock_target(game::combat::EntityId target)
{
    if (target == game::combat::kInvalidEntity) {
        clear_lock_target();
        return;
    }
    if (lock_target_ == target) {
        return;
    }
    lock_target_ = target;
    obstruction_time_ = 0.0f;
}

void CombatCameraController::clear_lock_target()
{
    if (!lock_target_) {
        return;
    }
    lock_target_.reset();
    obstruction_time_ = 0.0f;
}

CameraPose CombatCameraController::update(
    float delta_seconds,
    const CameraTarget& player_view,
    const game::combat::CombatEntity& player_entity,
    const game::combat::CombatEntity* target_entity,
    const CameraInput& input,
    bool player_moving,
    const world::CollisionWorld& collision_world,
    const world::Terrain& terrain)
{
    if (!active_) {
        throw std::logic_error("combat camera must be active before updating");
    }
    const float elapsed = std::max(delta_seconds, 0.0f);
    (void)camera_.apply_input(input);
    if (input.rotate_camera &&
        (input.mouse_delta_x != 0.0 || input.mouse_delta_y != 0.0)) {
        manual_orbit_latched_ = true;
    }

    const bool target_valid = lock_target_ && target_entity != nullptr &&
        target_entity->id == *lock_target_ &&
        game::combat::TargetSystem::is_valid_target(*target_entity, player_entity) &&
        in_lock_range(player_entity, *target_entity);
    if (lock_target_ && !target_valid) {
        clear_lock_target();
    }

    if (lock_target_) {
        camera_.set_follow_target_yaw(std::nullopt);
    } else {
        camera_.set_follow_target_yaw(
            input.rotate_camera || player_moving || manual_orbit_latched_
            ? std::nullopt
            : std::optional<float>{player_view.yaw + kPi});
    }

    CameraPose pose = camera_.update(elapsed, player_view, &collision_world);
    const glm::vec3 player_focus{pose.focus_x, pose.focus_y, pose.focus_z};
    glm::vec3 desired_focus = player_focus;
    if (lock_target_ && target_entity != nullptr) {
        desired_focus = target_focus(
            *target_entity, settings_.target_focus_height_fraction);
    }
    if (!aim_focus_initialized_) {
        aim_focus_ = player_focus;
        aim_focus_initialized_ = true;
    }
    const float focus_factor = settings_.lock_aim_smoothing_seconds <= 0.0f
        ? 1.0f
        : 1.0f - std::exp(-elapsed / settings_.lock_aim_smoothing_seconds);
    aim_focus_ += (desired_focus - aim_focus_) * focus_factor;
    pose.focus_x = aim_focus_.x;
    pose.focus_y = aim_focus_.y;
    pose.focus_z = aim_focus_.z;
    keep_camera_above_terrain(pose, terrain);

    if (lock_target_ && target_entity != nullptr) {
        const glm::vec3 eye{pose.eye_x, pose.eye_y, pose.eye_z};
        const glm::vec3 aim = target_focus(
            *target_entity, settings_.target_focus_height_fraction);
        if (visible_from(eye, aim, collision_world)) {
            obstruction_time_ = 0.0f;
        } else {
            obstruction_time_ += elapsed;
            if (obstruction_time_ >= settings_.obstruction_grace_seconds) {
                clear_lock_target();
            }
        }
    }
    return pose;
}

void CombatCameraController::keep_camera_above_terrain(
    CameraPose& pose, const world::Terrain& terrain) const
{
    const glm::vec3 focus{pose.focus_x, pose.focus_y, pose.focus_z};
    const glm::vec3 eye{pose.eye_x, pose.eye_y, pose.eye_z};
    constexpr int kSamples = 12;
    for (int sample = 1; sample <= kSamples; ++sample) {
        const float amount = static_cast<float>(sample) / kSamples;
        const glm::vec3 point = focus + (eye - focus) * amount;
        const glm::vec2 ground_position{point.x, point.z};
        if (!terrain.contains(ground_position)) {
            continue;
        }
        if (point.y < terrain.height_at(ground_position) + settings_.terrain_clearance) {
            const float safe_amount = static_cast<float>(sample - 1) / kSamples;
            const glm::vec3 safe_eye = focus + (eye - focus) * safe_amount;
            pose.eye_x = safe_eye.x;
            pose.eye_y = safe_eye.y;
            pose.eye_z = safe_eye.z;
            return;
        }
    }
}

}