#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "game/world/collision_world.hpp"

// Gameplay-side combat: entities, tab-targeting, auto attack and damage.
// Nothing here knows about animation or rendering; the client connects them through events.
namespace mmo::game::combat {

using EntityId = std::uint32_t;
constexpr EntityId kInvalidEntity = 0;

enum class Faction { Player, Hostile, Friendly };

struct CombatEntity {
    EntityId id = kInvalidEntity;
    std::string name;
    int level = 1;
    Faction faction = Faction::Hostile;
    glm::vec3 position{0.0f};
    float hit_radius = 0.5f;
    float height = 1.8f;
    float hp = 100.0f;
    float max_hp = 100.0f;

    [[nodiscard]] bool alive() const { return hp > 0.0f; }
};

class EntityRegistry {
public:
    EntityId create(CombatEntity entity);
    bool remove(EntityId id);
    [[nodiscard]] CombatEntity* find(EntityId id);
    [[nodiscard]] const CombatEntity* find(EntityId id) const;
    [[nodiscard]] const std::vector<CombatEntity>& entities() const { return entities_; }

private:
    std::vector<CombatEntity> entities_;
    EntityId next_id_ = 1;
};

// Ray picking and line of sight helpers.
bool ray_hits_box(glm::vec3 origin, glm::vec3 direction, glm::vec3 minimum, glm::vec3 maximum,
    float max_distance, float& distance);
bool segment_blocked(glm::vec3 from, glm::vec3 to, std::span<const world::CollisionBox> boxes);
EntityId pick_entity(const EntityRegistry& registry, glm::vec3 origin, glm::vec3 direction,
    float max_distance);

enum class TargetState { NoTarget, TargetSelected, TargetInvalid, TargetDead };

struct TargetSettings {
    float max_distance = 40.0f;
    float field_of_view_degrees = 110.0f;
};

// Owns "who is my target". Future slots (focus, mouseover, target-of-target) can be
// added next to current_ without touching the selection rules.
class TargetSystem {
public:
    explicit TargetSystem(TargetSettings settings = {});

    [[nodiscard]] EntityId current() const { return current_; }
    [[nodiscard]] EntityId previous() const { return previous_; }
    [[nodiscard]] TargetState state(const EntityRegistry& registry) const;
    [[nodiscard]] static bool is_valid_target(
        const CombatEntity& candidate, const CombatEntity& player);
    [[nodiscard]] const TargetSettings& settings() const { return settings_; }

    bool select(const EntityRegistry& registry, const CombatEntity& player, EntityId id);
    void clear();
    // TAB / Shift+TAB: hostile, living candidates in range and in front of `view_yaw`, nearest first.
    EntityId cycle(const EntityRegistry& registry, const CombatEntity& player, float view_yaw,
        bool backwards);
    // Drops targets that no longer exist.
    void update(const EntityRegistry& registry);

private:
    TargetSettings settings_;
    EntityId current_ = kInvalidEntity;
    EntityId previous_ = kInvalidEntity;
};

struct DamageEvent {
    EntityId source = kInvalidEntity;
    EntityId target = kInvalidEntity;
    float amount = 0.0f;
    bool killed = false;
};

class DamageSystem {
public:
    using Listener = std::function<void(const DamageEvent&)>;
    void add_listener(Listener listener);
    std::optional<DamageEvent> apply(
        EntityRegistry& registry, EntityId source, EntityId target, float amount);

private:
    std::vector<Listener> listeners_;
};

enum class CombatState { None, Combat, Attacking, Casting, Dead };
enum class AttackBlock {
    None, PlayerDead, NoTarget, TargetDead, OutOfRange, NotFacing, NoLineOfSight, OnCooldown
};

const char* to_string(CombatState state);
const char* to_string(TargetState state);
const char* to_string(AttackBlock reason);

struct CombatSettings {
    float attack_range = 2.5f;
    float attack_speed = 1.5f;  // seconds between swings
    float damage = 20.0f;
    float hit_range_tolerance = 0.75f;  // extra reach allowed when the hit lands (player is moving)
    bool combat_facing_enabled = true;
    float rotation_speed = 7.0f;        // radians per second used to turn toward the target
    float max_turn_angle_degrees = 70.0f;  // largest angle off-target at which a swing may start
    bool require_line_of_sight = true;
};

struct CombatContext {
    EntityId player_id = kInvalidEntity;
    glm::vec3 player_position{0.0f};
    float player_yaw = 0.0f;
    bool player_alive = true;
};

// Auto attack loop. Movement is never an input of this class apart from the position snapshot.
class CombatSystem {
public:
    using SwingRequest = std::function<bool()>;

    CombatSystem(CombatSettings settings, TargetSystem& targets, DamageSystem& damage);

    [[nodiscard]] const CombatSettings& settings() const { return settings_; }
    [[nodiscard]] CombatSettings& settings() { return settings_; }
    [[nodiscard]] bool auto_attack_enabled() const { return auto_attack_; }
    [[nodiscard]] float attack_timer() const { return timer_; }
    [[nodiscard]] CombatState state() const { return state_; }
    [[nodiscard]] AttackBlock last_block() const { return last_block_; }

    void set_auto_attack(bool enabled);
    void toggle_auto_attack() { set_auto_attack(!auto_attack_); }

    [[nodiscard]] AttackBlock evaluate(const EntityRegistry& registry, const CombatContext& context,
        const world::CollisionWorld* world) const;
    // Ticks the swing timer and starts a swing through `request_swing` when everything allows it.
    void update(float delta_seconds, const EntityRegistry& registry, const CombatContext& context,
        const world::CollisionWorld* world, const SwingRequest& request_swing);
    // Called by the animation layer's AttackHit event; validates again and applies damage.
    std::optional<DamageEvent> on_attack_hit(EntityRegistry& registry, const CombatContext& context);
    void on_swing_end();
    // Yaw the character should turn toward, if combat facing is active.
    [[nodiscard]] std::optional<float> desired_facing(
        const EntityRegistry& registry, const CombatContext& context) const;

private:
    CombatSettings settings_;
    TargetSystem& targets_;
    DamageSystem& damage_;
    bool auto_attack_ = false;
    bool swinging_ = false;
    float timer_ = 0.0f;
    EntityId swing_target_ = kInvalidEntity;
    CombatState state_ = CombatState::None;
    AttackBlock last_block_ = AttackBlock::None;
};

}
