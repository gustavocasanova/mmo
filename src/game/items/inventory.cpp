#include "game/items/inventory.hpp"

#include <algorithm>
#include <limits>

namespace mmo::game::items {
namespace {

bool fail(std::string* reason, std::string message)
{
    if (reason != nullptr) {
        *reason = std::move(message);
    }
    return false;
}

}

Inventory::Inventory(std::size_t capacity)
    : slots_(capacity)
{
}

bool Inventory::add(const ItemCatalog& catalog, ItemId item, std::uint32_t quantity,
    std::string* reason)
{
    const ItemDefinition* definition = catalog.find(item);
    if (definition == nullptr) return fail(reason, "unknown item " + std::to_string(item));
    if (quantity == 0) return fail(reason, "quantity must be positive");

    // Plan first, then apply, so a full bag never receives part of the quantity.
    std::uint64_t room = 0;
    for (const auto& slot : slots_) {
        if (!slot) {
            room += definition->max_stack;
        } else if (slot->item_id == item) {
            room += definition->max_stack - slot->quantity;
        }
    }
    if (room < quantity) return fail(reason, "inventory is full");

    std::uint32_t left = quantity;
    for (auto& slot : slots_) {
        if (slot && slot->item_id == item && slot->quantity < definition->max_stack) {
            const std::uint32_t moved = std::min(left, definition->max_stack - slot->quantity);
            slot->quantity += moved;
            left -= moved;
        }
    }
    for (auto& slot : slots_) {
        if (left == 0) break;
        if (!slot) {
            const std::uint32_t moved = std::min(left, definition->max_stack);
            slot = ItemStack{item, moved};
            left -= moved;
        }
    }
    if (reason != nullptr) reason->clear();
    return true;
}

bool Inventory::remove(ItemId item, std::uint32_t quantity)
{
    if (quantity == 0 || count(item) < quantity) {
        return false;
    }
    for (auto slot = slots_.rbegin(); slot != slots_.rend() && quantity > 0; ++slot) {
        if (*slot && (*slot)->item_id == item) {
            const std::uint32_t moved = std::min(quantity, (*slot)->quantity);
            (*slot)->quantity -= moved;
            quantity -= moved;
            if ((*slot)->quantity == 0) {
                slot->reset();
            }
        }
    }
    return true;
}

std::uint32_t Inventory::count(ItemId item) const
{
    std::uint64_t total = 0;
    for (const auto& slot : slots_) {
        if (slot && slot->item_id == item) total += slot->quantity;
    }
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(total, std::numeric_limits<std::uint32_t>::max()));
}

std::optional<ItemStack> Inventory::take(std::size_t index)
{
    if (index >= slots_.size()) return std::nullopt;
    std::optional<ItemStack> taken = slots_[index];
    slots_[index].reset();
    return taken;
}

bool Inventory::put(std::size_t index, ItemStack stack)
{
    if (index >= slots_.size() || slots_[index] || stack.item_id == kInvalidItem ||
        stack.quantity == 0) {
        return false;
    }
    slots_[index] = stack;
    return true;
}

std::optional<std::size_t> Inventory::first_empty() const
{
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (!slots_[index]) return index;
    }
    return std::nullopt;
}

std::optional<std::size_t> Inventory::find(ItemId item) const
{
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (slots_[index] && slots_[index]->item_id == item) return index;
    }
    return std::nullopt;
}

std::optional<ItemId> Loadout::equipped(ItemSlot slot) const
{
    const auto index = static_cast<std::size_t>(slot);
    return index < kSlotCount ? slots_[index] : std::nullopt;
}

bool Loadout::equip_from(Inventory& inventory, std::size_t inventory_index,
    const ItemCatalog& catalog, std::string* reason)
{
    const auto bag = inventory.slots();
    if (inventory_index >= bag.size() || !bag[inventory_index]) {
        return fail(reason, "inventory slot is empty");
    }
    const ItemDefinition* definition = catalog.find(bag[inventory_index]->item_id);
    if (definition == nullptr) return fail(reason, "unknown item in inventory");
    if (!definition->equippable()) return fail(reason, definition->name + " cannot be equipped");

    const auto slot_index = static_cast<std::size_t>(*definition->slot);
    const std::optional<ItemId> previous = slots_[slot_index];
    (void)inventory.take(inventory_index);
    if (previous) {
        (void)inventory.put(inventory_index, {*previous, 1});
    }
    slots_[slot_index] = definition->id;
    ++revision_;
    if (reason != nullptr) reason->clear();
    return true;
}

bool Loadout::unequip_to(Inventory& inventory, ItemSlot slot, std::string* reason)
{
    const auto index = static_cast<std::size_t>(slot);
    if (index >= kSlotCount || !slots_[index]) return fail(reason, "nothing equipped in slot");
    const std::optional<std::size_t> free_slot = inventory.first_empty();
    if (!free_slot) return fail(reason, "inventory is full");
    (void)inventory.put(*free_slot, {*slots_[index], 1});
    slots_[index].reset();
    ++revision_;
    if (reason != nullptr) reason->clear();
    return true;
}

std::optional<combat::WeaponStats> main_hand_weapon(
    const Loadout& loadout, const ItemCatalog& catalog)
{
    const std::optional<ItemId> item = loadout.equipped(ItemSlot::MainHand);
    if (!item) return std::nullopt;
    const ItemDefinition* definition = catalog.find(*item);
    return definition != nullptr ? definition->weapon : std::nullopt;
}

}
