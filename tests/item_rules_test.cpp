// Item rules with no engine, window or asset dependency: this target links only mmo_game,
// which is what a dedicated server will link.
#include "game/items/inventory.hpp"
#include "game/items/item_catalog.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace mmo;
using namespace mmo::game::items;

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

ItemDefinition parse(const std::string& text)
{
    return parse_item_definition(core::parse_key_value(text, "test.mmoitem"));
}

bool parse_fails(const std::string& text)
{
    try {
        (void)parse(text);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

ItemCatalog make_catalog()
{
    ItemCatalog catalog;
    (void)catalog.add(parse("MMO_ITEM 1\nid=1\nkey=sword\nname=Sword\nslot=MainHand\n"
        "weapon.damage=30\nweapon.attack_interval=1.5\nweapon.range=2.5\n"));
    (void)catalog.add(parse("MMO_ITEM 1\nid=2\nkey=axe\nname=Axe\nslot=MainHand\n"
        "weapon.damage=40\nweapon.attack_interval=2\nweapon.range=2.4\n"));
    (void)catalog.add(parse("MMO_ITEM 1\nid=3\nkey=hood\nname=Hood\nslot=Helmet\n"));
    (void)catalog.add(parse("MMO_ITEM 1\nid=4\nkey=berry\nname=Berry\nmax_stack=20\n"));
    return catalog;
}

void test_parsing_and_validation()
{
    const ItemDefinition sword = parse(
        "MMO_ITEM 1\n# comment\nid=7\nkey=sword\nname=Training Sword\nslot=MainHand\n"
        "weapon.damage=25\nweapon.attack_interval=1.5\nweapon.range=2.6\n"
        "visual.model=ignored.gltf\nvisual.attach=anything goes here\n");
    check(sword.id == 7 && sword.name == "Training Sword", "id and name parsed");
    check(sword.slot == ItemSlot::MainHand && sword.weapon && sword.weapon->damage == 25.0f,
        "slot and weapon parsed; visual keys ignored");

    check(parse_fails("MMO_PREFAB 1\nid=1\n"), "wrong header rejected");
    check(parse_fails("MMO_ITEM 1\nid=1\ncolour=red\n"), "unknown key rejected");
    check(parse_fails("MMO_ITEM 1\nid=abc\n"), "non-numeric id rejected");
    check(parse_fails("MMO_ITEM 1\nslot=Head\n"), "unknown slot rejected");
    check(parse_fails("MMO_ITEM 1\nweapon.damage=lots\n"), "non-numeric damage rejected");

    ItemCatalog catalog;
    std::string reason;
    check(catalog.add(sword, &reason), "valid sword accepted");
    check(!catalog.add(sword, &reason), "duplicate id rejected");
    ItemDefinition helmet_weapon = sword;
    helmet_weapon.id = 8;
    helmet_weapon.key = "helmet_weapon";
    helmet_weapon.slot = ItemSlot::Helmet;
    check(!catalog.add(helmet_weapon, &reason), "weapon outside a hand slot rejected");
    ItemDefinition stacking_armor{9, "plate", "Plate", 5, ItemSlot::Chest, std::nullopt};
    check(!catalog.add(stacking_armor, &reason), "equippable items cannot stack");
    check(catalog.find("sword") == catalog.find(7), "lookup by key and id agree");
}

void test_inventory_stacks_atomically()
{
    const ItemCatalog catalog = make_catalog();
    Inventory bag(3);
    check(bag.add(catalog, 4, 25), "25 berries take two slots");
    check(bag.count(4) == 25 && bag.slots()[0]->quantity == 20, "first stack filled to max");
    check(!bag.add(catalog, 4, 40), "overflow rejected");
    check(bag.count(4) == 25 && !bag.slots()[2], "rejected add changed nothing");
    check(bag.add(catalog, 4, 35) && bag.count(4) == 60, "partial stack and empty slot filled");
    check(!bag.add(catalog, 1, 1), "no room for a sword");
    check(bag.remove(4, 30) && bag.count(4) == 30 && !bag.slots()[2] &&
        bag.slots()[1]->quantity == 10, "remove takes from the last stacks");
    check(!bag.remove(4, 31), "cannot remove more than owned");
    check(!bag.add(catalog, 999, 1), "unknown item rejected");
}

void test_loadout_equip_swap_and_weapon_stats()
{
    const ItemCatalog catalog = make_catalog();
    Inventory bag(3);
    Loadout loadout;
    check(bag.add(catalog, 1, 1) && bag.add(catalog, 2, 1) && bag.add(catalog, 4, 3), "bag filled");
    check(!main_hand_weapon(loadout, catalog), "unarmed by default");

    const auto revision = loadout.revision();
    check(loadout.equip_from(bag, 0, catalog), "equip sword");
    check(loadout.equipped(ItemSlot::MainHand) == ItemId{1} && !bag.slots()[0],
        "sword moved from bag to main hand");
    check(loadout.revision() != revision, "revision changes on equip");
    check(main_hand_weapon(loadout, catalog)->damage == 30.0f, "sword stats drive combat");

    check(loadout.equip_from(bag, 1, catalog), "equip axe over sword");
    check(loadout.equipped(ItemSlot::MainHand) == ItemId{2} &&
        bag.slots()[1] && bag.slots()[1]->item_id == 1, "sword swapped back into the freed slot");

    std::string reason;
    check(!loadout.equip_from(bag, 2, catalog, &reason), "berries cannot be equipped");
    check(bag.count(4) == 3, "failed equip kept the berries");

    check(loadout.unequip_to(bag, ItemSlot::MainHand), "unequip axe");
    check(!loadout.equipped(ItemSlot::MainHand) && bag.count(2) == 1, "axe returned to the bag");
    check(!loadout.unequip_to(bag, ItemSlot::MainHand), "nothing left to unequip");

    Inventory full(1);
    Loadout worn;
    check(full.add(catalog, 3, 1) && worn.equip_from(full, 0, catalog), "wear the hood");
    check(full.add(catalog, 4, 1), "fill the only slot");
    check(!worn.unequip_to(full, ItemSlot::Helmet, &reason) && worn.equipped(ItemSlot::Helmet),
        "unequip into a full bag keeps the item equipped");
}

void test_shipped_content()
{
    const ItemCatalog catalog = ItemCatalog::load_directory("content/items");
    const ItemDefinition* sword = catalog.find("wooden_sword");
    check(sword != nullptr && sword->id == 1001, "wooden sword ships in content/items");
    check(sword != nullptr && sword->weapon && sword->weapon->damage > 20.0f,
        "the training sword hits harder than bare hands");
}

}

int main()
{
    try {
        test_parsing_and_validation();
        test_inventory_stacks_atomically();
        test_loadout_equip_swap_and_weapon_stats();
        test_shipped_content();
    } catch (const std::exception& error) {
        std::cerr << "FAILED with exception: " << error.what() << '\n';
        return 1;
    }
    if (failures == 0) {
        std::cout << "item rules tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
