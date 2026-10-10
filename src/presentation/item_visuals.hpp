#pragma once

#include "equipment/equipment.hpp"
#include "equipment/equipment_manager.hpp"
#include "game/items/inventory.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmo::presentation {

// Client-only half of an item file ("visual.*" keys): what it looks like and how it animates.
struct ItemVisual {
    std::filesystem::path model_path;
    std::vector<equipment::EquipmentAttachment> attachments;
    std::optional<equipment::AttackAnimation> attack;
};

// Returns std::nullopt when the file has no visual keys. Throws on malformed values.
[[nodiscard]] std::optional<ItemVisual> parse_item_visual(const core::KeyValueDocument& document);

[[nodiscard]] equipment::EquipmentSlot to_equipment_slot(game::items::ItemSlot slot);

// Builds the engine object for one item: a Weapon when it has an attack animation,
// otherwise BasicEquipment. Throws std::invalid_argument on inconsistent data.
[[nodiscard]] std::shared_ptr<const equipment::Equipment> make_equipment(
    const game::items::ItemDefinition& definition, const ItemVisual& visual);

// Rules and visuals of every item, loaded from the same files.
struct ItemContent {
    game::items::ItemCatalog catalog;
    std::unordered_map<game::items::ItemId, ItemVisual> visuals;
};

[[nodiscard]] ItemContent load_item_content(const std::filesystem::path& directory);

// Mirrors a gameplay Loadout onto a character's EquipmentManager. Equipment objects
// are built once per item and reused.
class LoadoutPresenter {
public:
    explicit LoadoutPresenter(const ItemContent& content);

    // No-op when the loadout revision did not change. False (with reason) if an item
    // cannot be displayed; slots that failed are left empty.
    [[nodiscard]] bool sync(const game::items::Loadout& loadout,
        equipment::EquipmentManager& equipment, std::string* reason = nullptr);

private:
    [[nodiscard]] std::shared_ptr<const equipment::Equipment> equipment_for(
        game::items::ItemId item);

    const ItemContent& content_;
    std::unordered_map<game::items::ItemId, std::shared_ptr<const equipment::Equipment>> built_;
    std::optional<std::uint64_t> synced_revision_;
};

}
