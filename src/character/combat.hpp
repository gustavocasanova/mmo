#pragma once

#include "character/character.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mmo::character {

enum class CombatState : std::uint32_t {
    None,
    Attack,
    Attack2,
    Block,
    Cast,
};

[[nodiscard]] std::string_view to_string(CombatState state);

struct CombatActionDefinition {
    CombatState state = CombatState::None;
    std::string clip_name;
    float duration = 0.0f;
    float speed = 1.0f;
    float hit_time = -1.0f;
    float blend_in = 0.1f;
    float blend_out = 0.15f;
};

enum class CombatEventType {
    AttackStart,
    AttackHit,
    AttackEnd,
};

struct CombatEvent {
    CombatEventType type = CombatEventType::AttackStart;
    CombatState state = CombatState::None;
    float time_seconds = 0.0f;
};

[[nodiscard]] std::vector<CombatActionDefinition> default_combat_actions();

// Owns the combat state and drives the upper-body layer. It never touches locomotion.
class CombatController {
public:
    using EventHandler = std::function<void(const CombatEvent&)>;

    CombatController(Character& character, std::vector<CombatActionDefinition> actions);

    void set_event_handler(EventHandler handler);
    // False when the action is unknown, has no clip, or another action is still in progress.
    bool request(CombatState state);
    void update();

    [[nodiscard]] CombatState state() const;
    [[nodiscard]] float attack_time() const;
    [[nodiscard]] float attack_duration() const;

private:
    Character& character_;
    std::vector<CombatActionDefinition> actions_;
    EventHandler handler_;
    CombatState state_ = CombatState::None;
};

// Receives animation-driven events and decides what they mean (damage, effects, sounds...).
class CombatSystem {
public:
    void handle(const CombatEvent& event);

    [[nodiscard]] unsigned attacks_started() const;
    [[nodiscard]] unsigned hits() const;
    [[nodiscard]] unsigned attacks_finished() const;

private:
    unsigned started_ = 0;
    unsigned hits_ = 0;
    unsigned finished_ = 0;
};

}
