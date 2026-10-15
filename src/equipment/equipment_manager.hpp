#pragma once

#include "equipment/equipment.hpp"

#include <memory>

namespace mmo::equipment {

class EquipmentManager {
public:
    explicit EquipmentManager(const animation::Skeleton& skeleton);

    [[nodiscard]] bool equip(std::shared_ptr<const Equipment> item, std::string* reason = nullptr);
    [[nodiscard]] std::shared_ptr<const Equipment> unequip(EquipmentSlot slot);
    [[nodiscard]] const Equipment* get_equipped(EquipmentSlot slot) const;
    [[nodiscard]] bool has_equipped(EquipmentSlot slot) const;
    [[nodiscard]] bool clear_slot(EquipmentSlot slot);
    void clear();
    [[nodiscard]] std::vector<ResolvedEquipmentAttachment> resolved_attachments() const;

private:
    static constexpr std::size_t kSlotCount = static_cast<std::size_t>(EquipmentSlot::Count);
    [[nodiscard]] static std::optional<std::size_t> slot_index(EquipmentSlot slot);

    const animation::Skeleton& skeleton_;
    std::array<std::shared_ptr<const Equipment>, kSlotCount> equipped_{};
};

}
