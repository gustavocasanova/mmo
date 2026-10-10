#pragma once

#include "equipment/equipment_types.hpp"

namespace mmo::equipment {

// Presentation of a primary attack. Damage remains in game/combat.
struct AttackAnimation {
    std::string clip_name;
    float duration = 0.0f;
    float speed = 1.0f;
    float hit_time = -1.0f;
    float blend_in = 0.1f;
    float blend_out = 0.15f;
};

// C++ interface: implementations must keep their definition stable while equipped.
class Equipment {
public:
    virtual ~Equipment() = default;
    [[nodiscard]] virtual const EquipmentDefinition& definition() const noexcept = 0;
    [[nodiscard]] virtual const AttackAnimation* primary_attack() const noexcept
    {
        return nullptr;
    }
};

}
