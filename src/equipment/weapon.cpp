#include "equipment/weapon.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace mmo::equipment {

Weapon::Weapon(EquipmentDefinition definition, AttackAnimation attack)
    : definition_(std::move(definition)), attack_(std::move(attack))
{
    if (definition_.slot != EquipmentSlot::MainHand && definition_.slot != EquipmentSlot::OffHand) {
        throw std::invalid_argument("weapons must use a hand slot");
    }
    if (attack_.clip_name.empty() || !std::isfinite(attack_.duration) || attack_.duration <= 0.0f ||
        !std::isfinite(attack_.speed) || attack_.speed <= 0.0f ||
        !std::isfinite(attack_.hit_time) || attack_.hit_time < 0.0f ||
        attack_.hit_time > attack_.duration || !std::isfinite(attack_.blend_in) ||
        attack_.blend_in < 0.0f || !std::isfinite(attack_.blend_out) || attack_.blend_out < 0.0f) {
        throw std::invalid_argument("weapon attack animation has invalid timing");
    }
}

const EquipmentDefinition& Weapon::definition() const noexcept { return definition_; }
const AttackAnimation* Weapon::primary_attack() const noexcept { return &attack_; }

}
