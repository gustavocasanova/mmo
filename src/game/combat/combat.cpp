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
    if (!std::isfinite(entity.position.x) || !std::isfinite(entity.position.y) ||
        !std::isfinite(entity.position.z) || !std::isfinite(entity.hit_radius) ||
        !std::isfinite(entity.height) || !std::isfinite(entity.hp) ||
        !std::isfinite(entity.max_hp) || entity.hit_radius < 0.0f ||
        entity.height <= 0.0f || entity.max_hp <= 0.0f) {
        throw std::invalid_argument("combat entity stats and position must be finite and valid");
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
    if (!std::isfinite(settings_.max_distance) ||
        !std::isfinite(settings_.field_of_view_degrees) ||
        settings_.max_distance <= 0.0f || settings_.field_of_view_degrees <= 0.0f ||
        settings_.field_of_view_degrees > 360.0f) {
        throw std::invalid_argument("target distance and field of view must be finite and positive");
    }
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
    if (!std::isfinite(amount) || amount <= 0.0f) {
        throw std::invalid_argument("damage amount must be finite and positive");
    }
    CombatEntity* entity = registry.find(target);
    if (entity == nullptr || !entity->alive()) {
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
    case AttackBlock::InvalidTarget: return "InvalidTarget";
    case AttackBlock::TargetDead: return "TargetDead";
    case AttackBlock::OutOfRange: return "OutOfRange";
    case AttackBlock::NotFacing: return "NotFacing";
    case AttackBlock::NoLineOfSight: return "NoLineOfSight";
    case AttackBlock::OnCooldown: return "OnCooldown";
    case AttackBlock::TargetChanged: return "TargetChanged";
    }
    return "?";
}

const char* to_string(AbilityBlock reason)
{
    switch (reason) {
    case AbilityBlock::None: return "Ready";
    case AbilityBlock::UnknownAbility: return "UnknownAbility";
    case AbilityBlock::PlayerDead: return "PlayerDead";
    case AbilityBlock::InvalidTarget: return "InvalidTarget";
    case AbilityBlock::TargetDead: return "TargetDead";
    case AbilityBlock::OutOfRange: return "OutOfRange";
    case AbilityBlock::NotFacing: return "NotFacing";
    case AbilityBlock::NoLineOfSight: return "NoLineOfSight";
    case AbilityBlock::InsufficientResource: return "InsufficientResource";
    case AbilityBlock::OnCooldown: return "OnCooldown";
    case AbilityBlock::Casting: return "Casting";
    }
    return "?";
}

const char* to_string(ResourceType resource)
{
    switch (resource) {
    case ResourceType::Rage: return "Rage";
    case ResourceType::Mana: return "Mana";
    case ResourceType::Focus: return "Focus";
    }
    return "?";
}

std::vector<AbilityDefinition> default_abilities(CombatClass combat_class)
{
    switch (combat_class) {
    case CombatClass::Warrior:
        return {
            {"warrior.basic_attack", "Basic Attack", combat_class, ResourceType::Rage,
                AbilityTarget::Hostile, 0.0f, 1.5f, 0.0f, 2.5f, 0.0f, 20.0f},
            {"warrior.power_strike", "Power Strike", combat_class, ResourceType::Rage,
                AbilityTarget::Hostile, 30.0f, 4.0f, 0.0f, 2.5f, 0.0f, 45.0f},
        };
    case CombatClass::Mage:
        return {
            {"mage.arcane_bolt", "Arcane Bolt", combat_class, ResourceType::Mana,
                AbilityTarget::Hostile, 10.0f, 0.5f, 0.0f, 20.0f, 0.0f, 24.0f},
            {"mage.fireball", "Fireball", combat_class, ResourceType::Mana,
                AbilityTarget::Hostile, 25.0f, 3.0f, 0.0f, 25.0f, 0.0f, 60.0f},
        };
    case CombatClass::Archer:
        return {
            {"archer.basic_shot", "Basic Shot", combat_class, ResourceType::Focus,
                AbilityTarget::Hostile, 0.0f, 1.2f, 4.0f, 25.0f, 0.0f, 22.0f},
            {"archer.power_shot", "Power Shot", combat_class, ResourceType::Focus,
                AbilityTarget::Hostile, 30.0f, 4.0f, 4.0f, 30.0f, 0.0f, 55.0f},
        };
    }
    return {};
}

AbilitySystem::AbilitySystem(CombatClass combat_class, TargetSystem& targets,
    DamageSystem& damage, std::vector<AbilityDefinition> abilities)
    : combat_class_(combat_class)
    , resource_type_(combat_class == CombatClass::Warrior ? ResourceType::Rage :
          combat_class == CombatClass::Mage ? ResourceType::Mana : ResourceType::Focus)
    , resource_(resource_type_ == ResourceType::Rage ? 0.0f : 100.0f)
    , targets_(targets)
    , damage_(damage)
    , abilities_(abilities.empty() ? default_abilities(combat_class) : std::move(abilities))
    , cooldowns_(abilities_.size(), 0.0f)
{
    if (abilities_.empty()) {
        throw std::invalid_argument("ability system requires at least one ability");
    }
    for (const AbilityDefinition& ability : abilities_) {
        if (ability.id.empty() || ability.name.empty() ||
            ability.combat_class != combat_class_ ||
            !std::isfinite(ability.resource_cost) || !std::isfinite(ability.cooldown) ||
            !std::isfinite(ability.min_range) || !std::isfinite(ability.max_range) ||
            !std::isfinite(ability.cast_time) || !std::isfinite(ability.damage) ||
            ability.resource_cost < 0.0f || ability.cooldown < 0.0f ||
            ability.min_range < 0.0f || ability.max_range < ability.min_range ||
            ability.cast_time < 0.0f || ability.damage <= 0.0f) {
            throw std::invalid_argument("ability definition is invalid");
        }
    }
}

const AbilityDefinition* AbilitySystem::find(const std::string& ability_id) const
{
    const auto found = std::find_if(abilities_.begin(), abilities_.end(),
        [&ability_id](const AbilityDefinition& ability) { return ability.id == ability_id; });
    return found == abilities_.end() ? nullptr : &*found;
}

float AbilitySystem::cooldown_remaining(const std::string& ability_id) const
{
    const auto found = std::find_if(abilities_.begin(), abilities_.end(),
        [&ability_id](const AbilityDefinition& ability) { return ability.id == ability_id; });
    return found == abilities_.end()
        ? 0.0f
        : cooldowns_[static_cast<std::size_t>(found - abilities_.begin())];
}

void AbilitySystem::set_resource(float value)
{
    if (!std::isfinite(value) || value < 0.0f) {
        throw std::invalid_argument("resource must be finite and non-negative");
    }
    resource_ = std::min(value, max_resource_);
}

void AbilitySystem::gain_resource(float amount)
{
    if (!std::isfinite(amount) || amount < 0.0f) {
        throw std::invalid_argument("resource gain must be finite and non-negative");
    }
    resource_ = std::min(max_resource_, resource_ + amount);
}

AbilityBlock AbilitySystem::evaluate_definition(const EntityRegistry& registry,
    const CombatContext& context, const world::CollisionWorld* world,
    const AbilityDefinition& definition) const
{
    if (!context.player_alive) {
        return AbilityBlock::PlayerDead;
    }
    const CombatEntity* player = registry.find(context.player_id);
    const CombatEntity* target = registry.find(targets_.current());
    if (player == nullptr || !player->alive()) {
        return AbilityBlock::PlayerDead;
    }
    if (definition.target == AbilityTarget::Self) {
        return resource_ < definition.resource_cost
            ? AbilityBlock::InsufficientResource : AbilityBlock::None;
    }
    if (target == nullptr || target->faction != Faction::Hostile) {
        return AbilityBlock::InvalidTarget;
    }
    if (!target->alive()) {
        return AbilityBlock::TargetDead;
    }
    const float distance = horizontal_distance(target->position, context.player_position);
    if (distance < definition.min_range || distance > definition.max_range) {
        return AbilityBlock::OutOfRange;
    }
    const float angle = std::abs(wrap_angle(
        yaw_to(context.player_position, target->position) - context.player_yaw));
    if (angle > glm::radians(definition.max_turn_angle_degrees)) {
        return AbilityBlock::NotFacing;
    }
    if (definition.require_line_of_sight && world != nullptr) {
        const glm::vec3 eye = context.player_position + glm::vec3{0.0f, 1.4f, 0.0f};
        const glm::vec3 aim = target->position + glm::vec3{0.0f, 1.0f, 0.0f};
        if (segment_blocked(eye, aim, world->boxes())) {
            return AbilityBlock::NoLineOfSight;
        }
    }
    return resource_ < definition.resource_cost
        ? AbilityBlock::InsufficientResource : AbilityBlock::None;
}

AbilityBlock AbilitySystem::evaluate(const EntityRegistry& registry,
    const CombatContext& context, const world::CollisionWorld* world,
    const std::string& ability_id) const
{
    const AbilityDefinition* definition = find(ability_id);
    if (definition == nullptr) {
        return AbilityBlock::UnknownAbility;
    }
    const std::size_t index = static_cast<std::size_t>(definition - abilities_.data());
    if (cast_remaining_ > 0.0f) {
        return AbilityBlock::Casting;
    }
    if (cooldowns_[index] > 0.0f) {
        return AbilityBlock::OnCooldown;
    }
    return evaluate_definition(registry, context, world, *definition);
}

void AbilitySystem::update(float delta_seconds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f) {
        throw std::invalid_argument("ability update delta must be finite and non-negative");
    }
    for (float& cooldown : cooldowns_) {
        cooldown = std::max(0.0f, cooldown - delta_seconds);
    }
    cast_remaining_ = std::max(0.0f, cast_remaining_ - delta_seconds);
}

AbilityResult AbilitySystem::cast(EntityRegistry& registry, const CombatContext& context,
    const world::CollisionWorld* world, const std::string& ability_id)
{
    AbilityResult result;
    result.ability_id = ability_id;
    const AbilityDefinition* definition = find(ability_id);
    if (definition == nullptr) {
        result.block = AbilityBlock::UnknownAbility;
        return result;
    }
    result.block = evaluate(registry, context, world, ability_id);
    if (result.block != AbilityBlock::None) {
        return result;
    }
    const std::size_t index = static_cast<std::size_t>(definition - abilities_.data());
    resource_ -= definition->resource_cost;
    cooldowns_[index] = definition->cooldown;
    cast_remaining_ = definition->cast_time;
    if (definition->target == AbilityTarget::Hostile) {
        result.damage = damage_.apply(
            registry, context.player_id, targets_.current(), definition->damage);
    }
    return result;
}

CombatSystem::CombatSystem(CombatSettings settings, TargetSystem& targets, DamageSystem& damage)
    : settings_(settings)
    , targets_(targets)
    , damage_(damage)
{
    if (!std::isfinite(settings_.attack_range) ||
        !std::isfinite(settings_.attack_speed) || !std::isfinite(settings_.damage) ||
        !std::isfinite(settings_.rotation_speed) ||
        !std::isfinite(settings_.max_turn_angle_degrees) ||
        settings_.attack_range <= 0.0f || settings_.attack_speed <= 0.0f ||
        settings_.damage <= 0.0f || settings_.rotation_speed < 0.0f ||
        settings_.max_turn_angle_degrees <= 0.0f ||
        settings_.max_turn_angle_degrees > 180.0f) {
        throw std::invalid_argument("combat settings must be finite and within valid ranges");
    }
}

void CombatSystem::set_auto_attack(bool enabled)
{
    auto_attack_ = enabled;
}

AttackBlock CombatSystem::evaluate(const EntityRegistry& registry, const CombatContext& context,
    const world::CollisionWorld* world) const
{
    return evaluate_target(registry, context, targets_.current(), world, true);
}

AttackBlock CombatSystem::evaluate_target(const EntityRegistry& registry,
    const CombatContext& context, EntityId target_id, const world::CollisionWorld* world,
    bool check_cooldown) const
{
    if (!context.player_alive) {
        return AttackBlock::PlayerDead;
    }
    const CombatEntity* player = registry.find(context.player_id);
    if (player == nullptr || !player->alive()) {
        return AttackBlock::PlayerDead;
    }
    const CombatEntity* target = registry.find(target_id);
    if (target == nullptr) {
        return target_id == kInvalidEntity ? AttackBlock::NoTarget : AttackBlock::InvalidTarget;
    }
    if (!target->alive()) {
        return AttackBlock::TargetDead;
    }
    if (target->faction != Faction::Hostile ||
        (player != nullptr && !TargetSystem::is_valid_target(*target, *player))) {
        return AttackBlock::InvalidTarget;
    }
    if (check_cooldown && timer_ > 0.0f) {
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
    // Auto attack follows a live hostile target; corpse selection remains available for UI.
    if (auto_attack_ && evaluate_target(
            registry, context, targets_.current(), world, false) == AttackBlock::InvalidTarget) {
        auto_attack_ = false;
    }
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
    EntityRegistry& registry, const CombatContext& context, const world::CollisionWorld* world)
{
    if (targets_.current() != swing_target_ && swinging_) {
        last_block_ = AttackBlock::TargetChanged;
        return std::nullopt;
    }
    if (!swinging_ || !auto_attack_) {
        last_block_ = AttackBlock::NoTarget;
        return std::nullopt;
    }
    last_block_ = evaluate_target(registry, context, swing_target_, world, false);
    if (last_block_ != AttackBlock::None) {
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
