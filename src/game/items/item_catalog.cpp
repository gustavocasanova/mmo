#include "game/items/item_catalog.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace mmo::game::items {
namespace {

constexpr std::array<std::string_view, static_cast<std::size_t>(ItemSlot::Count)> kSlotNames{
    "Helmet", "Shoulders", "Chest", "Gloves", "Pants", "Boots", "Cloak", "Bracers",
    "Ring1", "Ring2", "Ring3", "Ring4", "Earring1", "Earring2", "MainHand", "OffHand",
};

bool fail(std::string* reason, std::string message)
{
    if (reason != nullptr) {
        *reason = std::move(message);
    }
    return false;
}

bool is_hand(ItemSlot slot)
{
    return slot == ItemSlot::MainHand || slot == ItemSlot::OffHand;
}

}

std::string_view to_string(ItemSlot slot)
{
    const auto index = static_cast<std::size_t>(slot);
    return index < kSlotNames.size() ? kSlotNames[index] : "?";
}

std::optional<ItemSlot> parse_item_slot(std::string_view name)
{
    for (std::size_t index = 0; index < kSlotNames.size(); ++index) {
        if (kSlotNames[index] == name) {
            return static_cast<ItemSlot>(index);
        }
    }
    return std::nullopt;
}

bool ItemCatalog::add(ItemDefinition definition, std::string* reason)
{
    const std::string label = "item " + std::to_string(definition.id) + " (" + definition.key + ")";
    if (definition.id == kInvalidItem) return fail(reason, "item id must be positive");
    if (definition.key.empty() || definition.name.empty()) {
        return fail(reason, label + " needs a key and a name");
    }
    if (definition.max_stack == 0) return fail(reason, label + " max_stack must be positive");
    if (definition.equippable() && definition.max_stack != 1) {
        return fail(reason, label + " is equippable and therefore cannot stack");
    }
    if (definition.weapon) {
        if (!definition.weapon->valid()) return fail(reason, label + " has invalid weapon stats");
        if (!definition.slot || !is_hand(*definition.slot)) {
            return fail(reason, label + " is a weapon and must use MainHand or OffHand");
        }
    }
    if (by_id_.contains(definition.id)) return fail(reason, label + " reuses an existing id");
    if (by_key_.contains(definition.key)) return fail(reason, label + " reuses an existing key");

    by_id_.emplace(definition.id, definitions_.size());
    by_key_.emplace(definition.key, definitions_.size());
    definitions_.push_back(std::move(definition));
    if (reason != nullptr) reason->clear();
    return true;
}

const ItemDefinition* ItemCatalog::find(ItemId id) const
{
    const auto found = by_id_.find(id);
    return found == by_id_.end() ? nullptr : &definitions_[found->second];
}

const ItemDefinition* ItemCatalog::find(std::string_view key) const
{
    const auto found = by_key_.find(std::string(key));
    return found == by_key_.end() ? nullptr : &definitions_[found->second];
}

ItemDefinition parse_item_definition(const core::KeyValueDocument& document)
{
    if (document.format != kItemFileFormat || document.version != kItemFileVersion) {
        throw std::runtime_error(document.source + ": expected header " +
            std::string(kItemFileFormat) + " " + std::to_string(kItemFileVersion));
    }
    ItemDefinition item;
    combat::WeaponStats weapon;
    bool has_weapon = false;
    for (const core::KeyValueEntry& entry : document.entries) {
        const auto where = [&] { return document.where(entry.line); };
        if (entry.key.starts_with("visual.")) {
            continue;
        }
        if (entry.key == "id") {
            const auto id = core::parse_unsigned(document, entry);
            if (id == 0 || id > 0xffffffffull) throw std::runtime_error(where() + "id out of range");
            item.id = static_cast<ItemId>(id);
        } else if (entry.key == "key") {
            item.key = entry.value;
        } else if (entry.key == "name") {
            item.name = entry.value;
        } else if (entry.key == "max_stack") {
            const auto stack = core::parse_unsigned(document, entry);
            if (stack == 0 || stack > 0xffffffffull) {
                throw std::runtime_error(where() + "max_stack out of range");
            }
            item.max_stack = static_cast<std::uint32_t>(stack);
        } else if (entry.key == "slot") {
            item.slot = parse_item_slot(entry.value);
            if (!item.slot) throw std::runtime_error(where() + "unknown slot '" + entry.value + "'");
        } else if (entry.key == "weapon.damage") {
            weapon.damage = core::parse_float(document, entry);
            has_weapon = true;
        } else if (entry.key == "weapon.attack_interval") {
            weapon.attack_interval = core::parse_float(document, entry);
            has_weapon = true;
        } else if (entry.key == "weapon.range") {
            weapon.attack_range = core::parse_float(document, entry);
            has_weapon = true;
        } else {
            throw std::runtime_error(where() + "unknown key '" + entry.key + "'");
        }
    }
    if (has_weapon) {
        item.weapon = weapon;
    }
    return item;
}

std::vector<std::filesystem::path> find_item_files(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == kItemFileExtension) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

ItemCatalog ItemCatalog::load_directory(const std::filesystem::path& directory)
{
    ItemCatalog catalog;
    for (const std::filesystem::path& path : find_item_files(directory)) {
        std::string reason;
        if (!catalog.add(parse_item_definition(core::read_key_value_file(path)), &reason)) {
            throw std::runtime_error(path.generic_string() + ": " + reason);
        }
    }
    return catalog;
}

}
