#pragma once

#include "game/items/item_catalog.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mmo::game::items {

// A quantity of one item kind. Per-instance data (durability, persistent IDs) comes
// with persistence; until then two stacks of the same item are interchangeable.
struct ItemStack {
    ItemId item_id = kInvalidItem;
    std::uint32_t quantity = 0;
};

// Fixed-size bag. Operations are all-or-nothing: a failure leaves it unchanged.
class Inventory {
public:
    explicit Inventory(std::size_t capacity);

    // Fills existing stacks first, then empty slots.
    [[nodiscard]] bool add(const ItemCatalog& catalog, ItemId item, std::uint32_t quantity,
        std::string* reason = nullptr);
    // Removes `quantity` of `item` across stacks, last slots first.
    [[nodiscard]] bool remove(ItemId item, std::uint32_t quantity);
    [[nodiscard]] std::uint32_t count(ItemId item) const;

    [[nodiscard]] std::span<const std::optional<ItemStack>> slots() const { return slots_; }
    [[nodiscard]] std::optional<ItemStack> take(std::size_t index);
    // Places a stack in an empty slot.
    [[nodiscard]] bool put(std::size_t index, ItemStack stack);
    [[nodiscard]] std::optional<std::size_t> first_empty() const;
    [[nodiscard]] std::optional<std::size_t> find(ItemId item) const;

private:
    std::vector<std::optional<ItemStack>> slots_;
};

// What the character is wearing. `revision` changes on every equip/unequip so
// presentation can mirror it without diffing every frame.
class Loadout {
public:
    [[nodiscard]] std::optional<ItemId> equipped(ItemSlot slot) const;
    [[nodiscard]] std::uint64_t revision() const { return revision_; }

    // Moves the item at `inventory_index` into its slot; a previously equipped
    // item goes back to the freed inventory slot.
    [[nodiscard]] bool equip_from(Inventory& inventory, std::size_t inventory_index,
        const ItemCatalog& catalog, std::string* reason = nullptr);
    // Moves the item back to the first empty inventory slot.
    [[nodiscard]] bool unequip_to(Inventory& inventory, ItemSlot slot,
        std::string* reason = nullptr);

private:
    static constexpr std::size_t kSlotCount = static_cast<std::size_t>(ItemSlot::Count);
    std::array<std::optional<ItemId>, kSlotCount> slots_{};
    std::uint64_t revision_ = 0;
};

// Weapon stats of the main hand item, if it is a weapon.
[[nodiscard]] std::optional<combat::WeaponStats> main_hand_weapon(
    const Loadout& loadout, const ItemCatalog& catalog);

}
