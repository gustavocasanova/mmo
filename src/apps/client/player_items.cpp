#include "apps/client/player_items.hpp"

#include <stdexcept>

namespace mmo::app {

PlayerItems::PlayerItems(const std::filesystem::path& content_directory)
    : content_(presentation::load_item_content(content_directory))
{
}

void PlayerItems::give(std::string_view key, std::uint32_t quantity)
{
    const game::items::ItemDefinition* item = content_.catalog.find(key);
    if (item == nullptr) {
        throw std::runtime_error("unknown item key: " + std::string(key));
    }
    std::string reason;
    if (!inventory_.add(content_.catalog, item->id, quantity, &reason)) {
        throw std::runtime_error("cannot give " + item->name + ": " + reason);
    }
}

bool PlayerItems::equip(std::size_t inventory_index, std::string* reason)
{
    return loadout_.equip_from(inventory_, inventory_index, content_.catalog, reason);
}

bool PlayerItems::unequip(game::items::ItemSlot slot, std::string* reason)
{
    return loadout_.unequip_to(inventory_, slot, reason);
}

bool PlayerItems::toggle_main_hand(std::string* reason)
{
    if (loadout_.equipped(game::items::ItemSlot::MainHand)) {
        return unequip(game::items::ItemSlot::MainHand, reason);
    }
    const auto bag = inventory_.slots();
    for (std::size_t index = 0; index < bag.size(); ++index) {
        if (!bag[index]) continue;
        const game::items::ItemDefinition* item = content_.catalog.find(bag[index]->item_id);
        if (item != nullptr && item->slot == game::items::ItemSlot::MainHand) {
            return equip(index, reason);
        }
    }
    if (reason != nullptr) *reason = "no main-hand item in the inventory";
    return false;
}

void PlayerItems::apply(equipment::EquipmentManager& equipment, game::combat::CombatSystem& combat)
{
    std::string reason;
    if (!presenter_.sync(loadout_, equipment, &reason)) {
        throw std::runtime_error("cannot display equipped items: " + reason);
    }
    combat.set_weapon(game::items::main_hand_weapon(loadout_, content_.catalog));
}

}
