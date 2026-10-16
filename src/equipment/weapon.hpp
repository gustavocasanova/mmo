#pragma once

#include "equipment/equipment.hpp"

namespace mmo::equipment {

// New weapons can provide different definitions/animations without changing combat.
class Weapon : public Equipment {
public:
    Weapon(EquipmentDefinition definition, AttackAnimation attack);
    const EquipmentDefinition& definition() const noexcept override;
    const AttackAnimation* primary_attack() const noexcept override;

private:
    const EquipmentDefinition definition_;
    const AttackAnimation attack_;
};

}
