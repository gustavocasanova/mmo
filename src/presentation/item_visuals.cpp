#include "presentation/item_visuals.hpp"

#include "equipment/basic_equipment.hpp"
#include "equipment/weapon.hpp"

#include <stdexcept>

namespace mmo::presentation {
namespace {

equipment::EquipmentAttachment parse_attachment(
    const core::KeyValueDocument& document, const core::KeyValueEntry& entry)
{
    // socket bone px py pz qx qy qz qw [sx sy sz]
    const std::vector<std::string> words = core::split_words(entry.value);
    if (words.size() != 9 && words.size() != 12) {
        throw std::runtime_error(document.where(entry.line) +
            "visual.attach expects: socket bone px py pz qx qy qz qw [sx sy sz]");
    }
    std::vector<float> numbers;
    for (std::size_t index = 2; index < words.size(); ++index) {
        numbers.push_back(core::parse_float(document, {entry.key, words[index], entry.line}));
    }
    equipment::EquipmentAttachment attachment;
    attachment.socket_name = words[0];
    attachment.bone_name = words[1];
    attachment.local_transform.position_offset = {numbers[0], numbers[1], numbers[2]};
    attachment.local_transform.rotation_offset = {numbers[3], numbers[4], numbers[5], numbers[6]};
    if (numbers.size() == 10) {
        attachment.local_transform.scale = {numbers[7], numbers[8], numbers[9]};
    }
    return attachment;
}

}

std::optional<ItemVisual> parse_item_visual(const core::KeyValueDocument& document)
{
    ItemVisual visual;
    equipment::AttackAnimation attack;
    bool has_visual = false;
    bool has_attack = false;
    for (const core::KeyValueEntry& entry : document.entries) {
        if (!entry.key.starts_with("visual.")) {
            continue;
        }
        has_visual = true;
        const std::string key = entry.key.substr(7);
        if (key == "model") {
            visual.model_path = entry.value;
        } else if (key == "attach") {
            visual.attachments.push_back(parse_attachment(document, entry));
        } else if (key == "attack_clip") {
            attack.clip_name = entry.value;
            has_attack = true;
        } else if (key == "attack_duration") {
            attack.duration = core::parse_float(document, entry);
        } else if (key == "attack_speed") {
            attack.speed = core::parse_float(document, entry);
        } else if (key == "attack_hit_time") {
            attack.hit_time = core::parse_float(document, entry);
        } else if (key == "attack_blend_in") {
            attack.blend_in = core::parse_float(document, entry);
        } else if (key == "attack_blend_out") {
            attack.blend_out = core::parse_float(document, entry);
        } else {
            throw std::runtime_error(
                document.where(entry.line) + "unknown key '" + entry.key + "'");
        }
    }
    if (!has_visual) {
        return std::nullopt;
    }
    if (has_attack) {
        visual.attack = attack;
    }
    return visual;
}

equipment::EquipmentSlot to_equipment_slot(game::items::ItemSlot slot)
{
    // Both enums list the same slots in the same order; the static_asserts keep them aligned.
    static_assert(static_cast<std::size_t>(game::items::ItemSlot::Count) ==
        static_cast<std::size_t>(equipment::EquipmentSlot::Count));
    static_assert(static_cast<std::size_t>(game::items::ItemSlot::MainHand) ==
        static_cast<std::size_t>(equipment::EquipmentSlot::MainHand));
    static_assert(static_cast<std::size_t>(game::items::ItemSlot::OffHand) ==
        static_cast<std::size_t>(equipment::EquipmentSlot::OffHand));
    return static_cast<equipment::EquipmentSlot>(static_cast<std::size_t>(slot));
}

std::shared_ptr<const equipment::Equipment> make_equipment(
    const game::items::ItemDefinition& definition, const ItemVisual& visual)
{
    if (!definition.slot) {
        throw std::invalid_argument(definition.key + " has a visual but no equipment slot");
    }
    equipment::EquipmentDefinition engine;
    engine.id = definition.id;
    engine.name = definition.name;
    engine.slot = to_equipment_slot(*definition.slot);
    engine.model_path = visual.model_path;
    engine.attachments = visual.attachments;
    if (visual.attack) {
        return std::make_shared<const equipment::Weapon>(std::move(engine), *visual.attack);
    }
    return std::make_shared<const equipment::BasicEquipment>(std::move(engine));
}

ItemContent load_item_content(const std::filesystem::path& directory)
{
    ItemContent content;
    for (const std::filesystem::path& path : game::items::find_item_files(directory)) {
        const core::KeyValueDocument document = core::read_key_value_file(path);
        game::items::ItemDefinition definition = game::items::parse_item_definition(document);
        const game::items::ItemId id = definition.id;
        std::optional<ItemVisual> visual = parse_item_visual(document);
        std::string reason;
        if (!content.catalog.add(std::move(definition), &reason)) {
            throw std::runtime_error(path.generic_string() + ": " + reason);
        }
        if (visual) {
            // Fail at load time, not when the player first equips the item.
            try {
                (void)make_equipment(*content.catalog.find(id), *visual);
            } catch (const std::exception& error) {
                throw std::runtime_error(path.generic_string() + ": " + error.what());
            }
            content.visuals.emplace(id, std::move(*visual));
        }
    }
    return content;
}

LoadoutPresenter::LoadoutPresenter(const ItemContent& content)
    : content_(content)
{
}

std::shared_ptr<const equipment::Equipment> LoadoutPresenter::equipment_for(
    game::items::ItemId item)
{
    if (const auto built = built_.find(item); built != built_.end()) {
        return built->second;
    }
    const game::items::ItemDefinition* definition = content_.catalog.find(item);
    const auto visual = content_.visuals.find(item);
    if (definition == nullptr || visual == content_.visuals.end()) {
        return nullptr;
    }
    auto equipment = make_equipment(*definition, visual->second);
    built_.emplace(item, equipment);
    return equipment;
}

bool LoadoutPresenter::sync(const game::items::Loadout& loadout,
    equipment::EquipmentManager& equipment, std::string* reason)
{
    if (synced_revision_ == loadout.revision()) {
        return true;
    }
    synced_revision_ = loadout.revision();
    bool ok = true;
    std::string failures;
    for (std::size_t index = 0; index < static_cast<std::size_t>(game::items::ItemSlot::Count);
         ++index) {
        const auto slot = static_cast<game::items::ItemSlot>(index);
        const auto engine_slot = to_equipment_slot(slot);
        const std::optional<game::items::ItemId> item = loadout.equipped(slot);
        const equipment::Equipment* shown = equipment.get_equipped(engine_slot);
        if (!item) {
            if (shown != nullptr) (void)equipment.unequip(engine_slot);
            continue;
        }
        if (shown != nullptr && shown->definition().id == *item) {
            continue;
        }
        std::string why;
        const auto piece = equipment_for(*item);
        if (!piece) {
            why = "item " + std::to_string(*item) + " has no visual";
        } else if (equipment.equip(piece, &why)) {
            continue;
        }
        (void)equipment.unequip(engine_slot);
        ok = false;
        failures += (failures.empty() ? "" : "; ") + std::string(game::items::to_string(slot)) +
            ": " + why;
    }
    if (reason != nullptr) *reason = failures;
    return ok;
}

}
