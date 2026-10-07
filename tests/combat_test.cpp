#include "game/combat/combat.hpp"

#include <cmath>
#include <iostream>
#include <vector>

using namespace mmo::game::combat;
using mmo::game::world::CollisionBox;
using mmo::game::world::CollisionWorld;

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

struct Fixture {
    EntityRegistry registry;
    DamageSystem damage;
    TargetSystem targets;
    CombatSystem combat{CombatSettings{}, targets, damage};
    EntityId player_id = kInvalidEntity;

    EntityId add_enemy(const char* name, glm::vec3 position)
    {
        CombatEntity enemy;
        enemy.name = name;
        enemy.position = position;
        return registry.create(enemy);
    }

    Fixture()
    {
        CombatEntity player;
        player.name = "Player";
        player.faction = Faction::Player;
        player_id = registry.create(player);
    }

    const CombatEntity& player() const { return *registry.find(player_id); }
    CombatContext context(glm::vec3 position, float yaw = 0.0f) const
    {
        return {player_id, position, yaw, true};
    }
};

void test_tab_cycles_nearest_first_and_respects_fov_and_distance()
{
    Fixture f;
    const EntityId near_enemy = f.add_enemy("near", {0, 0, 5});
    const EntityId far_enemy = f.add_enemy("far", {0, 0, 12});
    const EntityId behind = f.add_enemy("behind", {0, 0, -3});
    const EntityId distant = f.add_enemy("distant", {0, 0, 100});
    check(f.targets.cycle(f.registry, f.player(), 0.0f, false) == near_enemy, "tab picks nearest");
    check(f.targets.cycle(f.registry, f.player(), 0.0f, false) == far_enemy, "tab goes next");
    check(f.targets.cycle(f.registry, f.player(), 0.0f, false) == near_enemy, "tab wraps");
    check(f.targets.previous() == far_enemy, "previous target tracked");
    check(f.targets.cycle(f.registry, f.player(), 0.0f, true) == far_enemy, "shift tab goes back");
    check(f.targets.current() != behind && f.targets.current() != distant, "fov/distance filter");
    check(f.targets.cycle(f.registry, f.player(), 3.14159f, false) == behind, "turning camera exposes enemy behind");
}

void test_select_clear_dead_and_invalid()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 2});
    check(f.targets.state(f.registry) == TargetState::NoTarget, "starts without target");
    check(f.targets.select(f.registry, f.player(), enemy), "select enemy");
    check(f.targets.state(f.registry) == TargetState::TargetSelected, "selected");
    check(!f.targets.select(f.registry, f.player(), f.player_id), "cannot target self");
    f.registry.find(enemy)->hp = 0.0f;
    check(f.targets.state(f.registry) == TargetState::TargetDead, "dead target state");
    check(f.targets.cycle(f.registry, f.player(), 0.0f, false) == kInvalidEntity, "dead not cycled");
    f.registry.remove(enemy);
    check(f.targets.state(f.registry) == TargetState::TargetInvalid, "invalid target state");
    f.targets.update(f.registry);
    check(f.targets.current() == kInvalidEntity, "invalid target cleaned up");
    f.targets.clear();
    check(f.targets.state(f.registry) == TargetState::NoTarget, "cleared");
}

void test_pick_entity()
{
    Fixture f;
    const EntityId front = f.add_enemy("front", {0, 0, 5});
    f.add_enemy("back", {0, 0, 9});
    check(pick_entity(f.registry, {0, 1, 0}, {0, 0, 1}, 50.0f) == front, "ray picks closest");
    check(pick_entity(f.registry, {0, 1, 0}, {1, 0, 0}, 50.0f) == kInvalidEntity, "ray misses");
}

void test_auto_attack_range_cooldown_and_damage_event()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 6});
    f.targets.select(f.registry, f.player(), enemy);
    f.combat.set_auto_attack(true);
    int swings = 0;
    const auto swing = [&] { ++swings; return true; };
    CombatContext context = f.context({0, 0, 0});
    f.combat.update(0.016f, f.registry, context, nullptr, swing);
    check(swings == 0 && f.combat.last_block() == AttackBlock::OutOfRange, "no swing out of range");
    check(f.combat.state() == CombatState::Combat, "combat state while out of range");
    check(f.combat.on_attack_hit(f.registry, context) == std::nullopt, "no damage without swing");

    context = f.context({0, 0, 4.0f});  // player walked into range, no teleport involved
    f.combat.update(0.016f, f.registry, context, nullptr, swing);
    check(swings == 1 && f.combat.state() == CombatState::Attacking, "swing in range");
    f.combat.update(0.5f, f.registry, context, nullptr, swing);
    check(swings == 1, "no second swing while swinging/cooldown");

    float damage_seen = 0.0f;
    f.damage.add_listener([&](const DamageEvent& event) { damage_seen = event.amount; });
    const auto hit = f.combat.on_attack_hit(f.registry, context);
    check(hit && damage_seen == 20.0f && f.registry.find(enemy)->hp == 80.0f, "damage applied on hit event");
    f.combat.on_swing_end();
    f.combat.update(0.016f, f.registry, context, nullptr, swing);
    check(f.combat.last_block() == AttackBlock::OnCooldown, "cooldown blocks next swing");
    f.combat.update(1.2f, f.registry, context, nullptr, swing);
    check(swings == 2, "swing again after attack speed");
}

void test_los_facing_and_death_cleanup()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 2});
    f.targets.select(f.registry, f.player(), enemy);
    f.combat.set_auto_attack(true);
    const CollisionWorld wall({}, {CollisionBox{{-1, 0, 0.8f}, {1, 3, 1.2f}}});
    int swings = 0;
    const auto swing = [&] { ++swings; return true; };
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), &wall, swing);
    check(f.combat.last_block() == AttackBlock::NoLineOfSight && swings == 0, "wall blocks line of sight");
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}, 3.0f), nullptr, swing);
    check(f.combat.last_block() == AttackBlock::NotFacing && swings == 0, "must face target to swing");
    const auto facing = f.combat.desired_facing(f.registry, f.context({0, 0, 0}, 3.0f));
    check(facing && std::abs(*facing) < 0.01f, "desired facing points at target");

    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), nullptr, swing);
    check(swings == 1, "swing starts");
    f.registry.find(enemy)->hp = 5.0f;
    const auto kill = f.combat.on_attack_hit(f.registry, f.context({0, 0, 0}));
    check(kill && kill->killed, "hit kills target");
    f.combat.on_swing_end();
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), nullptr, swing);
    check(!f.combat.auto_attack_enabled() && f.combat.state() == CombatState::None, "auto attack stops on dead target");
    check(f.targets.state(f.registry) == TargetState::TargetDead, "target reported dead");
}

}

int main()
{
    test_tab_cycles_nearest_first_and_respects_fov_and_distance();
    test_select_clear_dead_and_invalid();
    test_pick_entity();
    test_auto_attack_range_cooldown_and_damage_event();
    test_los_facing_and_death_cleanup();
    if (failures == 0) {
        std::cout << "combat tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
