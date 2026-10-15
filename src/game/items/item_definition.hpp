#pragma once

#include "game/combat/combat.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mmo::game::items {

// Stable content ID. It is what inventories, the network and the database store.
using ItemId = std::uint32_t;
constexpr ItemId kInvalidItem = 0;

// Gameplay slot. The client maps it to equipment::EquipmentSlot for display;
// this module never depends on skeletons, models or the renderer.
enum class ItemSlot : std::size_t {
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

[[nodiscard]] std::string_view to_string(ItemSlot slot);
[[nodiscard]] std::optional<ItemSlot> parse_item_slot(std::string_view name);

// Rules for one kind of item, loaded from content/items/**/*.mmoitem.
struct ItemDefinition {
    ItemId id = kInvalidItem;
    std::string key;   // readable unique name used by tools and logs, e.g. "wooden_sword"
    std::string name;  // display name
    std::uint32_t max_stack = 1;
    std::optional<ItemSlot> slot;                 // absent: cannot be equipped
    std::optional<combat::WeaponStats> weapon;    // present: changes the auto attack

    [[nodiscard]] bool equippable() const { return slot.has_value(); }
};

}
