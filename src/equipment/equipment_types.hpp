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

struct EquipmentDefinition {
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

}
