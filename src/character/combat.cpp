#include "character/combat.hpp"

#include <utility>

namespace mmo::character {

std::string_view to_string(CombatState state)
{
    switch (state) {
    case CombatState::None: return "NONE";
    case CombatState::Attack: return "ATTACK_01";
    case CombatState::Attack2: return "ATTACK_02";
    case CombatState::Block: return "BLOCK";
    case CombatState::Cast: return "CAST";
    }
    return "UNKNOWN";
}

std::vector<CombatActionDefinition> default_combat_actions()
{
    return {
        {CombatState::Attack, "Punch_Jab", 0.8f, 1.0f, 0.35f, 0.1f, 0.15f},
        {CombatState::Attack2, "Sword_Attack", 1.2f, 1.0f, 0.55f, 0.1f, 0.2f},
        {CombatState::Cast, "Spell_Simple_Shoot", 0.5f, 1.0f, 0.2f, 0.08f, 0.12f},
    };
}

CombatController::CombatController(Character& character, std::vector<CombatActionDefinition> actions)
    : character_(character)
    , actions_(std::move(actions))
{
}

void CombatController::set_event_handler(EventHandler handler)
{
    handler_ = std::move(handler);
}

bool CombatController::request(CombatState state)
{
    for (const CombatActionDefinition& action : actions_) {
        if (action.state != state || action.clip_name.empty()) {
            continue;
        }
        animation::ActionSettings settings;
        settings.tag = static_cast<std::uint32_t>(state);
        settings.duration = action.duration;
        settings.speed = action.speed;
        settings.hit_time = action.hit_time;
        settings.blend_in = action.blend_in;
        settings.blend_out = action.blend_out;
        if (!character_.model().animation().play_action(action.clip_name, settings)) {
            return false;
        }
        state_ = state;
        return true;
    }
    return false;
}

void CombatController::update()
{
    for (const animation::AnimationEvent& event : character_.model().animation().take_events()) {
        CombatEvent combat_event;
        combat_event.state = static_cast<CombatState>(event.tag);
        combat_event.time_seconds = event.time_seconds;
        switch (event.type) {
        case animation::AnimationEventType::ActionStart:
            combat_event.type = CombatEventType::AttackStart;
            break;
        case animation::AnimationEventType::ActionHit:
            combat_event.type = CombatEventType::AttackHit;
            break;
        case animation::AnimationEventType::ActionEnd:
            combat_event.type = CombatEventType::AttackEnd;
            break;
        }
        if (combat_event.type == CombatEventType::AttackEnd && state_ == combat_event.state) {
            state_ = CombatState::None;
        }
        if (handler_) {
            handler_(combat_event);
        }
    }
}

CombatState CombatController::state() const
{
    return state_;
}

float CombatController::attack_time() const
{
    return character_.model().animation().mixer().action_time();
}

float CombatController::attack_duration() const
{
    return character_.model().animation().mixer().action_duration();
}

void CombatSystem::handle(const CombatEvent& event)
{
    switch (event.type) {
    case CombatEventType::AttackStart: ++started_; break;
    case CombatEventType::AttackHit: ++hits_; break;
    case CombatEventType::AttackEnd: ++finished_; break;
    }
}

unsigned CombatSystem::attacks_started() const { return started_; }
unsigned CombatSystem::hits() const { return hits_; }
unsigned CombatSystem::attacks_finished() const { return finished_; }

}
