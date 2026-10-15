#pragma once

#include "game/combat/combat.hpp"

#include <array>
#include <string_view>

namespace mmo::game::items {

enum class WeaponType {
    Greatsword,
    Archer,
    Mage,
    SwordAndShield,
    Axe,
};

struct WeaponDefinition {
    WeaponType type;
    std::string_view id;
    std::string_view name;
    ::mmo::game::combat::CombatClass combat_class;
    std::string_view basic_ability_id;
    std::string_view power_ability_id;
    std::array<std::string_view, 12> attack_animations;
};

inline constexpr std::array<WeaponDefinition, 5> kWeaponCatalog{{
    {WeaponType::Greatsword, "weapon.greatsword", "Espada grande",
        ::mmo::game::combat::CombatClass::Warrior,
        "warrior.basic_attack", "warrior.power_strike",
        {"Greatsword_Slash_01", "Greatsword_Slash_02", "Greatsword_Slash_03",
         "Greatsword_Slash_04", "Greatsword_Slash_05", "Greatsword_Jump_Attack",
         "Greatsword_Slide_Attack", "Greatsword_Spin_Attack", "Greatsword_Attack",
         "Melee_Greatsword_Attack", "Sword_Attack"}},
    {WeaponType::Archer, "weapon.bow", "Arco de arqueiro",
        ::mmo::game::combat::CombatClass::Archer,
        "archer.basic_shot", "archer.power_shot", {"", ""}},
    {WeaponType::Mage, "weapon.staff", "Cajado de mago",
        ::mmo::game::combat::CombatClass::Mage, "mage.arcane_bolt", "mage.fireball",
        {"Mage_Attack_OneHand", "Mage_Attack_Area", "Spell_Simple_Shoot"}},
    {WeaponType::SwordAndShield, "weapon.sword_shield", "Espada e escudo",
        ::mmo::game::combat::CombatClass::Warrior,
        "warrior.basic_attack", "warrior.power_strike",
        {"SwordShield_Slash_01", "SwordShield_Slash_02", "SwordShield_Slash_03",
         "SwordShield_Slash_04", "SwordShield_Slash_05", "SwordShield_Attack_01",
         "SwordShield_Attack_02", "SwordShield_Attack_03", "SwordShield_Attack_04",
         "Melee_SwordShield_Attack", "Sword_Attack"}},
    {WeaponType::Axe, "weapon.axe", "Machado",
        ::mmo::game::combat::CombatClass::Warrior,
        "warrior.basic_attack", "warrior.power_strike",
        {"Axe_Attack_Backhand", "Axe_Attack_Downward", "Axe_Attack_Horizontal",
         "Axe_Attack_Spin_High", "Axe_Attack_Spin_Low", "Axe_Combo_01",
         "Axe_Combo_02", "Axe_Combo_03", "Axe_Jump_Run_Attack", "Sword_Attack"}},
}};

inline constexpr const std::array<WeaponDefinition, 5>& weapon_catalog()
{
    return kWeaponCatalog;
}

}
