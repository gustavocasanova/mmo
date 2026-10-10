#pragma once

#include "equipment/equipment.hpp"
#include <utility>

namespace mmo::equipment {

// Data-only equipment (armor/accessories), with no primary attack.
class BasicEquipment final : public Equipment {
public:
    explicit BasicEquipment(EquipmentDefinition definition)
        : definition_(std::move(definition)) {}

    const EquipmentDefinition& definition() const noexcept override { return definition_; }

private:
    const EquipmentDefinition definition_;
};

}
