#pragma once

#include "game/combat/combat.hpp"
#include "game/items/inventory.hpp"
#include "presentation/item_visuals.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace mmo::app {

// Local player's items. Gameplay state (Inventory/Loadout) changes only through the
// game::items rules; apply() then mirrors it to the visual equipment and to combat.
// When the server exists, the requests below become messages and the server's
// answer drives the same Loadout.
class PlayerItems {
public:
    static constexpr std::size_t kBagSize = 16;

    explicit PlayerItems(const std::filesystem::path& content_directory);

    // Throws when the item is unknown or does not fit; meant for starting gear.
    void give(std::string_view key, std::uint32_t quantity = 1);
    [[nodiscard]] bool equip(std::size_t inventory_index, std::string* reason = nullptr);
    [[nodiscard]] bool unequip(game::items::ItemSlot slot, std::string* reason = nullptr);
    // Unequips the main hand, or equips the first main-hand item found in the bag.
    [[nodiscard]] bool toggle_main_hand(std::string* reason = nullptr);

    // Throws if an equipped item cannot be displayed.
    void apply(equipment::EquipmentManager& equipment, game::combat::CombatSystem& combat);

    [[nodiscard]] const game::items::ItemCatalog& catalog() const { return content_.catalog; }
    [[nodiscard]] const game::items::Inventory& inventory() const { return inventory_; }
    [[nodiscard]] const game::items::Loadout& loadout() const { return loadout_; }

private:
    presentation::ItemContent content_;
    game::items::Inventory inventory_{kBagSize};
    game::items::Loadout loadout_;
    presentation::LoadoutPresenter presenter_{content_};
};

}
