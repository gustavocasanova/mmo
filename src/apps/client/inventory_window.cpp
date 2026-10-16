#include "apps/client/inventory_window.hpp"

#include <imgui.h>

#include <string>

namespace mmo::ui {
namespace {

std::string item_label(const game::items::ItemCatalog& catalog, game::items::ItemId id)
{
    const game::items::ItemDefinition* item = catalog.find(id);
    return item != nullptr ? item->name : "Unknown item " + std::to_string(id);
}

void item_tooltip(const game::items::ItemCatalog& catalog, game::items::ItemId id)
{
    const game::items::ItemDefinition* item = catalog.find(id);
    if (item == nullptr || !ImGui::IsItemHovered()) {
        return;
    }
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(item->name.c_str());
    if (item->slot) {
        ImGui::TextDisabled("%s", std::string(game::items::to_string(*item->slot)).c_str());
    }
    if (item->weapon) {
        ImGui::Text("Damage %.0f", item->weapon->damage);
        ImGui::Text("Speed %.2fs  Range %.1fm",
            item->weapon->attack_interval, item->weapon->attack_range);
    }
    ImGui::EndTooltip();
}

}

InventoryActions draw_inventory_window(const app::PlayerItems& items, bool* open)
{
    InventoryActions actions;
    if (open == nullptr || !*open) {
        return actions;
    }
    ImGui::SetNextWindowSize({300.0f, 0.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inventory (I)", open)) {
        ImGui::End();
        return actions;
    }
    const auto& catalog = items.catalog();

    ImGui::SeparatorText("Equipped");
    bool any_equipped = false;
    for (std::size_t index = 0; index < static_cast<std::size_t>(game::items::ItemSlot::Count);
         ++index) {
        const auto slot = static_cast<game::items::ItemSlot>(index);
        const auto equipped = items.loadout().equipped(slot);
        if (!equipped) continue;
        any_equipped = true;
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::SmallButton("Unequip")) {
            actions.unequip_slot = slot;
        }
        ImGui::SameLine();
        ImGui::Text("%s: %s", std::string(game::items::to_string(slot)).c_str(),
            item_label(catalog, *equipped).c_str());
        item_tooltip(catalog, *equipped);
        ImGui::PopID();
    }
    if (!any_equipped) {
        ImGui::TextDisabled("Nothing equipped");
    }

    ImGui::SeparatorText("Bag");
    const auto bag = items.inventory().slots();
    bool any_item = false;
    for (std::size_t index = 0; index < bag.size(); ++index) {
        if (!bag[index]) continue;
        any_item = true;
        const game::items::ItemDefinition* item = catalog.find(bag[index]->item_id);
        ImGui::PushID(1000 + static_cast<int>(index));
        if (item != nullptr && item->equippable()) {
            if (ImGui::SmallButton("Equip")) {
                actions.equip_index = index;
            }
            ImGui::SameLine();
        }
        if (bag[index]->quantity > 1) {
            ImGui::Text("%s x%u", item_label(catalog, bag[index]->item_id).c_str(),
                bag[index]->quantity);
        } else {
            ImGui::TextUnformatted(item_label(catalog, bag[index]->item_id).c_str());
        }
        item_tooltip(catalog, bag[index]->item_id);
        ImGui::PopID();
    }
    if (!any_item) {
        ImGui::TextDisabled("Empty");
    }
    ImGui::End();
    return actions;
}

}
