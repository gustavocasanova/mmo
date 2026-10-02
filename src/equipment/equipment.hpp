#pragma once

#include "animation/skeleton.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mmo::equipment {

enum class EquipmentSlot : std::size_t {
    Helmet,
    Shoulders,
    Chest,
    Gloves,
    Pants,
    Boots,
    Cloak,
    Bracers,
    Ring1,
    Ring2,
    Ring3,
    Ring4,
    Earring1,
    Earring2,
    MainHand,
    OffHand,
    Count,
};

struct AttachmentTransform {
    animation::Vector3 position_offset{0.0f, 0.0f, 0.0f};
    animation::Quaternion rotation_offset{};
    animation::Vector3 scale{1.0f, 1.0f, 1.0f};
};

struct EquipmentAttachment {
    std::string socket_name;
    std::string bone_name;
    AttachmentTransform local_transform{};
};

struct Equipment {
    std::uint64_t id = 0;
    std::string name;
    EquipmentSlot slot = EquipmentSlot::Helmet;
    std::filesystem::path model_path;
    std::vector<EquipmentAttachment> attachments;
};

struct ResolvedEquipmentAttachment {
    EquipmentSlot slot;
    std::uint64_t equipment_id;
    std::size_t bone_index;
    std::string socket_name;
    AttachmentTransform local_transform;
};

class EquipmentManager {
public:
    explicit EquipmentManager(const animation::Skeleton& skeleton);

    [[nodiscard]] bool equip(const Equipment& item, std::string* reason = nullptr);
    [[nodiscard]] std::optional<Equipment> unequip(EquipmentSlot slot);
    [[nodiscard]] const Equipment* get_equipped(EquipmentSlot slot) const;
    [[nodiscard]] bool has_equipped(EquipmentSlot slot) const;
    [[nodiscard]] bool clear_slot(EquipmentSlot slot);
    void clear();
    [[nodiscard]] std::vector<ResolvedEquipmentAttachment> resolved_attachments() const;

private:
    static constexpr std::size_t kSlotCount = static_cast<std::size_t>(EquipmentSlot::Count);
    [[nodiscard]] static std::optional<std::size_t> slot_index(EquipmentSlot slot);

    const animation::Skeleton& skeleton_;
    std::array<std::optional<Equipment>, kSlotCount> equipped_{};
};

}