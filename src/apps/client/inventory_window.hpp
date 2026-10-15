#pragma once

#include "apps/client/player_items.hpp"

#include <cstddef>
#include <optional>

namespace mmo::ui {

struct InventoryActions {
    std::optional<std::size_t> equip_index;
    std::optional<game::items::ItemSlot> unequip_slot;
};

// Bag and equipped items with ImGui; call between begin_frame() and render().
// It only reports clicks: the caller applies them through PlayerItems.
[[nodiscard]] InventoryActions draw_inventory_window(
    const app::PlayerItems& items, bool* open);

}
