#include "assets/gltf_model_loader.hpp"
#include "assets/static_model_geometry.hpp"
#include "equipment/attachment_pose.hpp"
#include "equipment/equipment_manager.hpp"
#include "equipment/basic_equipment.hpp"
#include "equipment/weapon.hpp"
#include "character/combat.hpp"
#include <type_traits>
#include <memory>
#include "presentation/item_visuals.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
bool close(float a, float b) { return std::abs(a - b) < 0.0001f; }
}

int main()
{
    try {
        using namespace mmo;
        assets::GltfModelLoader loader;
        // The sword is data: content/items/wooden_sword/wooden_sword.mmoitem.
        const presentation::ItemContent content = presentation::load_item_content("content/items");
        const game::items::ItemDefinition* item = content.catalog.find("wooden_sword");
        require(item != nullptr && item->id == 1001, "wooden sword definition is missing");
        require(item->slot == game::items::ItemSlot::MainHand && item->weapon.has_value(),
            "wooden sword must be a main-hand weapon");
        require(content.visuals.contains(item->id), "wooden sword has no visual");
        const auto sword = presentation::make_equipment(*item, content.visuals.at(item->id));
        const auto& definition = sword->definition();
        require(definition.attachments.size() == 1 &&
            close(definition.attachments[0].local_transform.position_offset[1], 0.075f) &&
            close(definition.attachments[0].local_transform.rotation_offset.x, 0.70710678f),
            "grip transform was not read from the item file");
        const auto model = loader.load(definition.model_path);
        const auto geometry = assets::build_static_model_geometry(
            model, assets::StaticModelOrigin::AssetPivot);
        require(!geometry.vertices.empty(), "sword has no geometry");
        require(close(geometry.minimum.y, -0.16f) && close(geometry.maximum.y, 0.835f),
            "sword must stay at meter scale with the pivot inside the grip");
        require(close(geometry.minimum.x, -0.145f) && close(geometry.maximum.x, 0.145f),
            "crossguard dimensions changed");
        const auto centered = assets::build_static_model_geometry(model);
        require(close(centered.minimum.y + centered.maximum.y, 0),
            "editor geometry must retain its centered pivot");
        for (const auto& vertex : geometry.vertices) {
            require(vertex.color[0] > vertex.color[1] && vertex.color[1] > vertex.color[2],
                "wood material colors were lost during import");
        }

        // A rotated joint with a deliberately different skin matrix catches the
        // inverse-bind mistake: rigid props use joint globals, never the palette.
        animation::Pose pose;
        pose.global_matrices.push_back({0,1,0,0, -1,0,0,0, 0,0,1,0, 2,3,4,1});
        pose.skin_matrices.push_back({1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
        equipment::ResolvedEquipmentAttachment attachment{
            equipment::EquipmentSlot::MainHand, definition.id, 0, "Grip", {}};
        attachment.local_transform.position_offset = {0, 0.25f, 0};
        attachment.local_transform.scale = {2, 2, 2};
        const auto matrix = equipment::attachment_matrix(attachment, pose);
        require(close(matrix[12], 1.75f) && close(matrix[13], 3) && close(matrix[14], 4),
            "grip offset must rotate with the joint and preserve joint translation");
        require(close(matrix[1], 2) && close(matrix[4], -2), "grip scale/rotation was lost");
        bool rejected = false;
        attachment.bone_index = 1;
        try { (void)equipment::attachment_matrix(attachment, pose); }
        catch (const std::out_of_range&) { rejected = true; }
        require(rejected, "missing joint was not rejected");

        const auto body = loader.load(
            "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb");
        equipment::EquipmentManager equipment(body.skeleton);
        std::string reason;
        const bool equipped = equipment.equip(sword, &reason);
        require(equipped, reason.c_str());
        auto resolved = equipment.resolved_attachments();
        require(resolved.size() == 1, "sword must have one rigid attachment");
        require(body.skeleton.bones[resolved[0].bone_index].name == "hand_r",
            "sword must attach to the right hand");
        animation::Animator animator(body.skeleton, body.animations);
        for (const char* clip_name : {"Idle_Loop", "Walk_Loop", "Sword_Attack"}) {
            auto clip = animator.find_clip(clip_name);
            require(clip.has_value(), "prototype animation is absent");
            animator.play(*clip, true);
            const auto initial = equipment::attachment_matrix(resolved[0], animator.pose());
            animator.update(0.35f);
            const auto animated = equipment::attachment_matrix(resolved[0], animator.pose());
            require(initial != animated, "sword did not follow the animated hand");
            for (float value : animated) require(std::isfinite(value), "invalid attachment pose");
        }
        require(equipment.unequip(definition.slot) != nullptr, "cannot remove sword");
        require(equipment.resolved_attachments().empty(), "removed sword left an attachment behind");
        // The base contract must preserve dynamic behavior and lifetime.
        static_assert(std::is_abstract_v<equipment::Equipment>);
        static_assert(std::has_virtual_destructor_v<equipment::Equipment>);
        std::shared_ptr<const equipment::Equipment> polymorphic = sword;
        require(polymorphic->primary_attack()->clip_name == "Sword_Attack",
            "base interface lost the concrete weapon attack");
        require(!equipment.equip(nullptr), "null equipment must be rejected");
        require(equipment.equip(polymorphic), "cannot equip polymorphic weapon");
        auto invalid = definition;
        invalid.attachments.front().bone_name = "missing";
        require(!equipment.equip(std::make_shared<const equipment::BasicEquipment>(invalid)),
            "invalid replacement should be rejected");
        require(equipment.get_equipped(definition.slot) == polymorphic.get(),
            "failed replacement changed the equipped object");
        invalid = definition;
        invalid.attachments.front().local_transform.rotation_offset = {0,0,0,0};
        require(!equipment.equip(std::make_shared<const equipment::BasicEquipment>(invalid)),
            "zero quaternion must be rejected before rendering");

        // A second, unrelated ID/clip must work without changing application code.
        auto alternative = definition;
        alternative.id = 9002;
        alternative.name = "Test staff";
        auto staff = std::make_shared<const equipment::Weapon>(alternative,
            equipment::AttackAnimation{"Spell_Simple_Shoot", 0.5f, 1.0f, 0.2f, 0.08f, 0.12f});
        std::weak_ptr<const equipment::Equipment> lifetime = staff;
        require(equipment.equip(staff), "generic second weapon was rejected");
        staff.reset();
        require(!lifetime.expired(), "manager did not keep equipped object alive");
        auto removed = equipment.unequip(definition.slot);
        require(removed->primary_attack()->clip_name == "Spell_Simple_Shoot",
            "unequip sliced the derived weapon");
        removed.reset();
        require(lifetime.expired(), "removed weapon is still retained by the manager");

        auto shared_body = std::make_shared<const assets::Model>(body);
        character::Character player(shared_body);
        character::CombatController combat(player, character::default_combat_actions());
        require(player.model().equipment().equip(sword), "player cannot equip sword");
        require(combat.request_primary_attack(), "equipped primary attack failed");
        require(player.model().animation().upper_clip_name() == "Sword_Attack",
            "primary attack did not come from the equipped object");
        player.model().animation().cancel_action();
        require(player.model().equipment().equip(std::make_shared<const equipment::Weapon>(alternative,
            equipment::AttackAnimation{"Spell_Simple_Shoot", 0.5f, 1.0f, 0.2f, 0.08f, 0.12f})),
            "player cannot change weapon");
        require(combat.request_primary_attack(), "second weapon attack failed");
        require(player.model().animation().upper_clip_name() == "Spell_Simple_Shoot",
            "combat still depends on the sword ID instead of its contract");
        player.model().animation().cancel_action();
        (void)player.model().equipment().unequip(definition.slot);
        require(combat.request_primary_attack(), "unarmed fallback failed");
        require(player.model().animation().upper_clip_name() == "Punch_Jab",
            "unarmed fallback no longer uses its configured action");
        bool invalid_attack_rejected = false;
        try {
            equipment::Weapon invalid_weapon(alternative, {"Sword_Attack", -1.0f});
        } catch (const std::invalid_argument&) { invalid_attack_rejected = true; }
        require(invalid_attack_rejected, "weapon accepted invalid attack timing");

        // Full client path: gameplay inventory/loadout -> visual equipment -> combat clip.
        player.model().animation().cancel_action();  // the unarmed jab above is still playing
        game::items::Inventory bag(4);
        game::items::Loadout loadout;
        require(bag.add(content.catalog, item->id, 1), "sword does not fit in the bag");
        presentation::LoadoutPresenter presenter(content);
        require(presenter.sync(loadout, player.model().equipment()), "empty loadout sync failed");
        require(!player.model().equipment().has_equipped(definition.slot),
            "empty loadout left a visual behind");
        require(loadout.equip_from(bag, 0, content.catalog), "loadout cannot equip the sword");
        require(presenter.sync(loadout, player.model().equipment(), &reason), reason.c_str());
        require(player.model().equipment().get_equipped(definition.slot)->definition().id == 1001,
            "loadout did not reach the character");
        require(combat.request_primary_attack() &&
            player.model().animation().upper_clip_name() == "Sword_Attack",
            "sword from the loadout does not drive the attack clip");
        player.model().animation().cancel_action();
        require(loadout.unequip_to(bag, game::items::ItemSlot::MainHand), "cannot unequip sword");
        require(presenter.sync(loadout, player.model().equipment()), "unequip sync failed");
        require(player.model().equipment().resolved_attachments().empty(),
            "unequipped sword is still drawn");
        std::cout << "Wooden sword, equipment polymorphism and combat integration passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
