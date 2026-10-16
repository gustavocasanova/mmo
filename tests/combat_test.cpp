#include "game/combat/combat.hpp"
#include "game/items/weapons.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace mmo::game::combat;
using mmo::world::CollisionBox;
using mmo::world::CollisionWorld;

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

void test_friendly_targets_cannot_be_auto_attacked()
{
    Fixture f;
    CombatEntity friendly;
    friendly.name = "Friendly";
    friendly.faction = Faction::Friendly;
    friendly.position = {0, 0, 2};
    const EntityId friendly_id = f.registry.create(friendly);
    check(f.targets.select(f.registry, f.player(), friendly_id),
        "friendly entities may be selected for future friendly-target interactions");
    f.combat.set_auto_attack(true);
    int swings = 0;
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), nullptr,
        [&] { ++swings; return true; });
    check(swings == 0, "auto attack must not start against a friendly");
    check(!f.combat.auto_attack_enabled(), "auto attack must stop for a friendly target");
    check(f.combat.evaluate(f.registry, f.context({0, 0, 0}), nullptr) ==
            AttackBlock::InvalidTarget,
        "friendly target must report an invalid attack target");
}

void test_invalid_combat_configuration_is_rejected()
{
    bool rejected = false;
    try {
        TargetSystem targets(TargetSettings{-1.0f, 110.0f});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid target distance must be rejected");

    rejected = false;
    try {
        Fixture f;
        CombatSettings settings;
        settings.attack_range = std::numeric_limits<float>::infinity();
        CombatSystem invalid(settings, f.targets, f.damage);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "non-finite combat configuration must be rejected");

    rejected = false;
    try {
        DamageSystem damage;
        Fixture f;
        (void)damage.apply(f.registry, f.player_id,
            f.add_enemy("orc", {0, 0, 2}), std::numeric_limits<float>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "non-finite damage must be rejected");
}

void test_pick_entity()
{
    Fixture f;
    const EntityId front = f.add_enemy("front", {0, 0, 5});
    f.add_enemy("back", {0, 0, 9});
    check(pick_entity(f.registry, {0, 1, 0}, {0, 0, 1}, 50.0f) == front, "ray picks closest");
    check(pick_entity(f.registry, {0, 1, 0}, {1, 0, 0}, 50.0f) == kInvalidEntity, "ray misses");
}

void test_attack_hit_revalidates_range_visibility_and_current_target()
{
    Fixture f;
    const EntityId first = f.add_enemy("first", {0, 0, 2});
    const EntityId second = f.add_enemy("second", {1, 0, 2});
    f.targets.select(f.registry, f.player(), first);
    f.combat.set_auto_attack(true);
    int swings = 0;
    const auto start_swing = [&] { ++swings; return true; };

    const CombatContext in_range = f.context({0, 0, 0});
    f.combat.update(0.016f, f.registry, in_range, nullptr, start_swing);
    check(swings == 1, "attack should start when target is in range");
    const CombatContext out_of_range = f.context({0, 0, -1.0f});
    check(!f.combat.on_attack_hit(f.registry, out_of_range),
        "attack hit must be rejected after moving out of range");
    check(f.combat.last_block() == AttackBlock::OutOfRange,
        "out-of-range impact should report its reason");
    check(f.registry.find(first)->hp == 100.0f, "out-of-range impact must not deal damage");
    f.combat.on_swing_end();

    f.combat.update(2.0f, f.registry, in_range, nullptr, start_swing);
    check(swings == 2, "next attack should start after cooldown");
    const CollisionWorld wall({}, {CollisionBox{{-1, 0, 0.8f}, {1, 3, 1.2f}}});
    check(!f.combat.on_attack_hit(f.registry, in_range, &wall),
        "attack hit must be rejected when line of sight becomes blocked");
    check(f.combat.last_block() == AttackBlock::NoLineOfSight,
        "blocked impact should report line of sight failure");
    check(f.registry.find(first)->hp == 100.0f, "blocked impact must not deal damage");
    f.combat.on_swing_end();

    f.combat.update(2.0f, f.registry, in_range, nullptr, start_swing);
    check(swings == 3, "attack should start again after the next cooldown");
    check(f.targets.select(f.registry, f.player(), second), "switch to a new target");
    check(!f.combat.on_attack_hit(f.registry, in_range),
        "an in-flight swing must not damage the previously selected target");
    check(f.combat.last_block() == AttackBlock::TargetChanged,
        "switched target should report why the pending hit was discarded");
    check(f.registry.find(first)->hp == 100.0f, "switched-away target must take no damage");
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

void test_weapon_replaces_unarmed_stats()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 2.8f});
    f.targets.select(f.registry, f.player(), enemy);
    f.combat.set_auto_attack(true);
    int swings = 0;
    const auto swing = [&] { ++swings; return true; };
    const CombatContext context = f.context({0, 0, 0});
    f.combat.update(0.016f, f.registry, context, nullptr, swing);
    check(swings == 0 && f.combat.last_block() == AttackBlock::OutOfRange,
        "unarmed reach is 2.5m");

    f.combat.set_weapon(WeaponStats{35.0f, 2.0f, 3.0f});
    f.combat.update(0.016f, f.registry, context, nullptr, swing);
    check(swings == 1, "weapon range allows the swing");
    const auto hit = f.combat.on_attack_hit(f.registry, context);
    check(hit && hit->amount == 35.0f, "weapon damage applied");
    f.combat.on_swing_end();
    f.combat.update(1.6f, f.registry, context, nullptr, swing);
    check(swings == 1, "weapon attack interval is used for the cooldown");

    f.combat.set_weapon(std::nullopt);
    check(f.combat.active_stats().damage == 20.0f, "unequipping returns to unarmed damage");
    bool rejected = false;
    try {
        f.combat.set_weapon(WeaponStats{10.0f, 0.0f, 2.0f});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && !f.combat.weapon(), "invalid weapon stats rejected without changing state");
}

void test_ability_loadouts_resources_and_cooldowns()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 2});
    f.targets.select(f.registry, f.player(), enemy);
    AbilitySystem warrior(CombatClass::Warrior, f.targets, f.damage);
    const CombatContext context = f.context({0, 0, 0});

    check(warrior.evaluate(f.registry, context, nullptr, "warrior.power_strike") ==
            AbilityBlock::InsufficientResource,
        "warrior power strike requires rage");
    warrior.set_resource(30.0f);
    const AbilityResult strike = warrior.cast(
        f.registry, context, nullptr, "warrior.power_strike");
    check(strike.block == AbilityBlock::None && strike.damage &&
            f.registry.find(enemy)->hp == 55.0f,
        "warrior power strike spends rage and deals configured damage");
    check(warrior.resource() == 0.0f &&
            warrior.evaluate(f.registry, context, nullptr, "warrior.power_strike") ==
                AbilityBlock::OnCooldown,
        "ability resource and cooldown are tracked centrally");

    AbilitySystem mage(CombatClass::Mage, f.targets, f.damage);
    check(mage.resource_type() == ResourceType::Mana &&
            mage.resource() == mage.max_resource(),
        "mage starts with a full mana pool");
    check(default_abilities(CombatClass::Archer).front().min_range > 0.0f,
        "archer loadout enforces ranged minimum distance");
}

void test_weapon_catalog_animations_and_abilities()
{
    using namespace mmo::game;
    const auto& weapons = items::weapon_catalog();
    check(weapons.size() == 5, "weapon catalog exposes five selectable items");
    check(weapons[0].type == items::WeaponType::Greatsword &&
            weapons[0].attack_animations[0] == "Greatsword_Slash_01" &&
            weapons[0].attack_animations[1] == "Greatsword_Slash_02",
        "greatsword cycles through two-handed attack variations");
    check(weapons[1].type == items::WeaponType::Archer &&
            weapons[1].attack_animations[0].empty(),
        "archer remains selectable without inventing a bow attack animation");
    check(weapons[2].combat_class == combat::CombatClass::Mage &&
            weapons[2].attack_animations[0] == "Mage_Attack_OneHand" &&
            weapons[2].attack_animations[1] == "Mage_Attack_Area",
        "mage has multiple attack animations and the mage ability set");
    check(weapons[3].type == items::WeaponType::SwordAndShield &&
            weapons[3].attack_animations[0] == "SwordShield_Slash_01" &&
            weapons[3].attack_animations[1] == "SwordShield_Slash_02",
        "sword and shield cycle through shield attack variations");
    check(weapons[4].type == items::WeaponType::Axe &&
            weapons[4].attack_animations[0] == "Axe_Attack_Backhand",
        "axe exposes its imported attack variations");
}

void test_ability_revalidates_range_and_line_of_sight()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 2});
    f.targets.select(f.registry, f.player(), enemy);
    AbilitySystem mage(CombatClass::Mage, f.targets, f.damage);
    const CollisionWorld wall({}, {CollisionBox{{-1, 0, 0.8f}, {1, 3, 1.2f}}});
    check(mage.evaluate(f.registry, f.context({0, 0, 0}), &wall, "mage.arcane_bolt") ==
            AbilityBlock::NoLineOfSight,
        "ability validation checks line of sight");
    check(mage.cast(f.registry, f.context({0, 0, 0}), &wall, "mage.arcane_bolt").damage ==
            std::nullopt,
        "blocked ability does not deal damage");
    check(mage.evaluate(f.registry, f.context({0, 0, -30}), nullptr, "mage.arcane_bolt") ==
            AbilityBlock::OutOfRange,
        "ability validation checks maximum range");
}

void test_combat_engagement_is_independent_of_auto_attack()
{
    Fixture f;
    const EntityId enemy = f.add_enemy("orc", {0, 0, 6});
    check(f.targets.select(f.registry, f.player(), enemy), "select combat target");
    f.combat.set_combat_engaged(true);
    int swings = 0;
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), nullptr,
        [&] { ++swings; return true; });
    check(f.combat.state() == CombatState::Combat,
        "explicit combat engagement did not activate combat state");
    check(!f.combat.auto_attack_enabled() && swings == 0,
        "combat engagement unexpectedly enabled auto attack");

    f.combat.set_combat_engaged(false);
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), nullptr, {});
    check(f.combat.state() == CombatState::None,
        "combat state did not end after explicit disengagement");

    f.combat.set_combat_engaged(true);
    f.registry.find(enemy)->hp = 0.0f;
    f.combat.update(0.016f, f.registry, f.context({0, 0, 0}), nullptr, {});
    check(f.combat.state() == CombatState::None,
        "combat engagement persisted after its target died");
}

}

int main()
{
    test_tab_cycles_nearest_first_and_respects_fov_and_distance();
    test_select_clear_dead_and_invalid();
    test_friendly_targets_cannot_be_auto_attacked();
    test_invalid_combat_configuration_is_rejected();
    test_pick_entity();
    test_attack_hit_revalidates_range_visibility_and_current_target();
    test_auto_attack_range_cooldown_and_damage_event();
    test_combat_engagement_is_independent_of_auto_attack();
    test_los_facing_and_death_cleanup();
    test_weapon_replaces_unarmed_stats();
    test_ability_loadouts_resources_and_cooldowns();
    test_weapon_catalog_animations_and_abilities();
    test_ability_revalidates_range_and_line_of_sight();
    if (failures == 0) {
        std::cout << "combat tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
