#include "game/combat/combat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

#include <glm/trigonometric.hpp>

namespace mmo::game::combat {
namespace {

float wrap_angle(float angle)
{
    constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
    angle = std::fmod(angle + std::numbers::pi_v<float>, two_pi);
    if (angle < 0.0f) {
        angle += two_pi;
    }
    return angle - std::numbers::pi_v<float>;
}

float horizontal_distance(glm::vec3 a, glm::vec3 b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}

float yaw_to(glm::vec3 from, glm::vec3 to)
{
    return std::atan2(to.x - from.x, to.z - from.z);
}

}

EntityId EntityRegistry::create(CombatEntity entity)
{
    if (entity.max_hp <= 0.0f) {
        throw std::invalid_argument("entity max_hp must be positive");
    }
    entity.id = next_id_++;
    entity.hp = std::clamp(entity.hp, 0.0f, entity.max_hp);
    entities_.push_back(std::move(entity));
    return entities_.back().id;
}

bool EntityRegistry::remove(EntityId id)
{
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [id](const CombatEntity& entity) { return entity.id == id; });
    if (found == entities_.end()) {
        return false;
    }
    entities_.erase(found);
    return true;
}

CombatEntity* EntityRegistry::find(EntityId id)
{
    for (CombatEntity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const CombatEntity* EntityRegistry::find(EntityId id) const
{
    return const_cast<EntityRegistry*>(this)->find(id);
}

bool ray_hits_box(glm::vec3 origin, glm::vec3 direction, glm::vec3 minimum, glm::vec3 maximum,
    float max_distance, float& distance)
{
    float near_t = 0.0f;
    float far_t = max_distance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1e-8f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) {
                return false;
            }
            continue;
        }
        float t0 = (minimum[axis] - origin[axis]) / direction[axis];
        float t1 = (maximum[axis] - origin[axis]) / direction[axis];
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        near_t = std::max(near_t, t0);
        far_t = std::min(far_t, t1);
        if (near_t > far_t) {
            return false;
        }
    }
    distance = near_t;
    return true;
}

bool segment_blocked(glm::vec3 from, glm::vec3 to, std::span<const world::CollisionBox> boxes)
{
    const glm::vec3 delta = to - from;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (length < 1e-5f) {
        return false;
    }
    const glm::vec3 direction = delta / length;
    float distance = 0.0f;
    for (const world::CollisionBox& box : boxes) {
        if (ray_hits_box(from, direction, box.minimum, box.maximum, length, distance)) {
            return true;
        }
    }
    return false;
}

EntityId pick_entity(const EntityRegistry& registry, glm::vec3 origin, glm::vec3 direction,
    float max_distance)
{
    EntityId best = kInvalidEntity;
    float best_distance = std::numeric_limits<float>::max();
    for (const CombatEntity& entity : registry.entities()) {
        if (entity.faction == Faction::Player) {
            continue;
        }
        const glm::vec3 minimum{entity.position.x - entity.hit_radius, entity.position.y,
            entity.position.z - entity.hit_radius};
        const glm::vec3 maximum{entity.position.x + entity.hit_radius,
            entity.position.y + entity.height, entity.position.z + entity.hit_radius};
        float distance = 0.0f;
        if (ray_hits_box(origin, direction, minimum, maximum, max_distance, distance) &&
            distance < best_distance) {
            best_distance = distance;
            best = entity.id;
        }
    }
    return best;
}

TargetSystem::TargetSystem(TargetSettings settings)
    : settings_(settings)
{
}

TargetState TargetSystem::state(const EntityRegistry& registry) const
{
    if (current_ == kInvalidEntity) {
        return TargetState::NoTarget;
    }
    const CombatEntity* target = registry.find(current_);
    if (target == nullptr) {
        return TargetState::TargetInvalid;
    }
    return target->alive() ? TargetState::TargetSelected : TargetState::TargetDead;
}

bool TargetSystem::is_valid_target(const CombatEntity& candidate, const CombatEntity& player)
{
    return candidate.id != kInvalidEntity && candidate.id != player.id &&
        candidate.faction == Faction::Hostile && candidate.alive();
}

bool TargetSystem::select(
    const EntityRegistry& registry, const CombatEntity& player, EntityId id)
{
    const CombatEntity* candidate = registry.find(id);
    // Corpses and friendly NPCs can still be inspected, only the player itself is excluded.
    if (candidate == nullptr || candidate->id == player.id) {
        return false;
    }
    if (id != current_) {
        previous_ = current_;
        current_ = id;
    }
    return true;
}

void TargetSystem::clear()
{
    if (current_ != kInvalidEntity) {
        previous_ = current_;
    }
    current_ = kInvalidEntity;
}

EntityId TargetSystem::cycle(const EntityRegistry& registry, const CombatEntity& player,
    float view_yaw, bool backwards)
{
    struct Candidate {
        EntityId id;
        float distance;
    };
    std::vector<Candidate> candidates;
    const float half_fov = glm::radians(settings_.field_of_view_degrees) * 0.5f;
    for (const CombatEntity& entity : registry.entities()) {
        if (!is_valid_target(entity, player)) {
            continue;
        }
        const float distance = horizontal_distance(entity.position, player.position);
        if (distance > settings_.max_distance) {
            continue;
        }
        const float angle = std::abs(wrap_angle(yaw_to(player.position, entity.position) - view_yaw));
        if (distance > 0.01f && angle > half_fov) {
            continue;
        }
        candidates.push_back({entity.id, distance});
    }
    if (candidates.empty()) {
        return kInvalidEntity;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distance != b.distance ? a.distance < b.distance : a.id < b.id;
    });
    std::size_t index = backwards ? candidates.size() - 1 : 0;
    const auto current = std::find_if(candidates.begin(), candidates.end(),
        [this](const Candidate& candidate) { return candidate.id == current_; });
    if (current != candidates.end()) {
        const std::size_t at = static_cast<std::size_t>(current - candidates.begin());
        index = backwards ? (at + candidates.size() - 1) % candidates.size()
                          : (at + 1) % candidates.size();
    }
    const EntityId chosen = candidates[index].id;
    if (chosen != current_) {
        previous_ = current_;
        current_ = chosen;
    }
    return chosen;
}

void TargetSystem::update(const EntityRegistry& registry)
{
    if (state(registry) == TargetState::TargetInvalid) {
        clear();
    }
    if (previous_ != kInvalidEntity && registry.find(previous_) == nullptr) {
        previous_ = kInvalidEntity;
    }
}

void DamageSystem::add_listener(Listener listener)
{
    listeners_.push_back(std::move(listener));
}

std::optional<DamageEvent> DamageSystem::apply(
    EntityRegistry& registry, EntityId source, EntityId target, float amount)
{
    CombatEntity* entity = registry.find(target);
    if (entity == nullptr || !entity->alive() || amount <= 0.0f) {
        return std::nullopt;
    }
    entity->hp = std::max(0.0f, entity->hp - amount);
    const DamageEvent event{source, target, amount, !entity->alive()};
    for (const Listener& listener : listeners_) {
        listener(event);
    }
    return event;
}

const char* to_string(CombatState state)
{
    switch (state) {
    case CombatState::None: return "None";
    case CombatState::Combat: return "Combat";
    case CombatState::Attacking: return "Attacking";
    case CombatState::Casting: return "Casting";
    case CombatState::Dead: return "Dead";
    }
    return "?";
}

const char* to_string(TargetState state)
{
    switch (state) {
    case TargetState::NoTarget: return "NoTarget";
    case TargetState::TargetSelected: return "TargetSelected";
    case TargetState::TargetInvalid: return "TargetInvalid";
    case TargetState::TargetDead: return "TargetDead";
    }
    return "?";
}

const char* to_string(AttackBlock reason)
{
    switch (reason) {
    case AttackBlock::None: return "Ready";
    case AttackBlock::PlayerDead: return "PlayerDead";
    case AttackBlock::NoTarget: return "NoTarget";
    case AttackBlock::TargetDead: return "TargetDead";
    case AttackBlock::OutOfRange: return "OutOfRange";
    case AttackBlock::NotFacing: return "NotFacing";
    case AttackBlock::NoLineOfSight: return "NoLineOfSight";
    case AttackBlock::OnCooldown: return "OnCooldown";
    }
    return "?";
}

CombatSystem::CombatSystem(CombatSettings settings, TargetSystem& targets, DamageSystem& damage)
    : settings_(settings)
    , targets_(targets)
    , damage_(damage)
{
    if (settings_.attack_range <= 0.0f || settings_.attack_speed <= 0.0f) {
        throw std::invalid_argument("attack range and speed must be positive");
    }
}

void CombatSystem::set_auto_attack(bool enabled)
{
    auto_attack_ = enabled;
}

AttackBlock CombatSystem::evaluate(const EntityRegistry& registry, const CombatContext& context,
    const world::CollisionWorld* world) const
{
    if (!context.player_alive) {
        return AttackBlock::PlayerDead;
    }
    const CombatEntity* target = registry.find(targets_.current());
    if (target == nullptr) {
        return AttackBlock::NoTarget;
    }
    if (!target->alive()) {
        return AttackBlock::TargetDead;
    }
    if (timer_ > 0.0f) {
        return AttackBlock::OnCooldown;
    }
    if (horizontal_distance(target->position, context.player_position) > settings_.attack_range) {
        return AttackBlock::OutOfRange;
    }
    const float angle = std::abs(wrap_angle(
        yaw_to(context.player_position, target->position) - context.player_yaw));
    if (angle > glm::radians(settings_.max_turn_angle_degrees)) {
        return AttackBlock::NotFacing;
    }
    if (settings_.require_line_of_sight && world != nullptr) {
        const glm::vec3 eye = context.player_position + glm::vec3{0.0f, 1.4f, 0.0f};
        const glm::vec3 aim = target->position + glm::vec3{0.0f, 1.0f, 0.0f};
        if (segment_blocked(eye, aim, world->boxes())) {
            return AttackBlock::NoLineOfSight;
        }
    }
    return AttackBlock::None;
}

void CombatSystem::update(float delta_seconds, const EntityRegistry& registry,
    const CombatContext& context, const world::CollisionWorld* world,
    const SwingRequest& request_swing)
{
    timer_ = std::max(0.0f, timer_ - delta_seconds);
    if (!context.player_alive) {
        auto_attack_ = false;
        swinging_ = false;
    }
    // Auto attack follows the target: it ends when the target is cleared, dies or vanishes.
    if (auto_attack_ && targets_.state(registry) != TargetState::TargetSelected) {
        auto_attack_ = false;
    }
    last_block_ = AttackBlock::None;
    if (auto_attack_ && !swinging_) {
        last_block_ = evaluate(registry, context, world);
        if (last_block_ == AttackBlock::None && request_swing && request_swing()) {
            swinging_ = true;
            swing_target_ = targets_.current();
            timer_ = settings_.attack_speed;
        }
    }
    if (!context.player_alive) {
        state_ = CombatState::Dead;
    } else if (swinging_) {
        state_ = CombatState::Attacking;
    } else if (auto_attack_) {
        state_ = CombatState::Combat;
    } else {
        state_ = CombatState::None;
    }
}

std::optional<DamageEvent> CombatSystem::on_attack_hit(
    EntityRegistry& registry, const CombatContext& context)
{
    const CombatEntity* target = registry.find(swing_target_);
    if (!swinging_ || !context.player_alive || target == nullptr || !target->alive()) {
        return std::nullopt;
    }
    if (horizontal_distance(target->position, context.player_position) >
        settings_.attack_range + settings_.hit_range_tolerance) {
        return std::nullopt;
    }
    return damage_.apply(registry, context.player_id, swing_target_, settings_.damage);
}

void CombatSystem::on_swing_end()
{
    swinging_ = false;
    swing_target_ = kInvalidEntity;
}

std::optional<float> CombatSystem::desired_facing(
    const EntityRegistry& registry, const CombatContext& context) const
{
    if (!settings_.combat_facing_enabled || !auto_attack_ || !context.player_alive) {
        return std::nullopt;
    }
    const CombatEntity* target = registry.find(targets_.current());
    if (target == nullptr || !target->alive()) {
        return std::nullopt;
    }
    return yaw_to(context.player_position, target->position);
}

}
