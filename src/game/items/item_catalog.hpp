#pragma once

#include "core/key_value_file.hpp"
#include "game/items/item_definition.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mmo::game::items {

// Every item definition known to the game, indexed by ID and key.
class ItemCatalog {
public:
    // Validates and stores the definition; false (with reason) on invalid data or duplicates.
    [[nodiscard]] bool add(ItemDefinition definition, std::string* reason = nullptr);
    [[nodiscard]] const ItemDefinition* find(ItemId id) const;
    [[nodiscard]] const ItemDefinition* find(std::string_view key) const;
    [[nodiscard]] const std::vector<ItemDefinition>& definitions() const { return definitions_; }

    // Loads every *.mmoitem under `directory` (recursively, in path order). Throws on any error.
    [[nodiscard]] static ItemCatalog load_directory(const std::filesystem::path& directory);

private:
    std::vector<ItemDefinition> definitions_;
    std::unordered_map<ItemId, std::size_t> by_id_;
    std::unordered_map<std::string, std::size_t> by_key_;
};

constexpr std::string_view kItemFileFormat = "MMO_ITEM";
constexpr int kItemFileVersion = 1;
constexpr std::string_view kItemFileExtension = ".mmoitem";

// Reads the gameplay keys of an item file. Keys starting with "visual." belong to the
// client presentation and are skipped; any other unknown key is an error.
[[nodiscard]] ItemDefinition parse_item_definition(const core::KeyValueDocument& document);

// Sorted list of item files under `directory`.
[[nodiscard]] std::vector<std::filesystem::path> find_item_files(
    const std::filesystem::path& directory);

}
