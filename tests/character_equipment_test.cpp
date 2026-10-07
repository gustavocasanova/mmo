#include "animation/animation_controller.hpp"
#include "assets/gltf_model_loader.hpp"
#include "assets/model.hpp"
#include "character/character.hpp"
#include "equipment/equipment.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::shared_ptr<mmo::assets::Model> make_body_model()
{
    using namespace mmo;
    auto model = std::make_shared<assets::Model>();
    auto add_bone = [&model](std::string name, std::size_t parent) {
        animation::Bone bone;
        bone.name = std::move(name);
        bone.parent_index = parent;
        return model->skeleton.add_bone(std::move(bone));
    };

    const std::size_t root = add_bone("Root", animation::kNoBone);
    const std::size_t pelvis = add_bone("Pelvis", root);
    const std::size_t spine = add_bone("Spine", pelvis);
    const std::size_t chest = add_bone("Chest", spine);
    const std::size_t neck = add_bone("Neck", chest);
    const std::size_t head = add_bone("Head", neck);
    const std::size_t back = add_bone("Back", chest);
    const std::size_t shoulder_left = add_bone("Shoulder_L", chest);
    const std::size_t shoulder_right = add_bone("Shoulder_R", chest);
    const std::size_t forearm_left = add_bone("Forearm_L", shoulder_left);
    const std::size_t forearm_right = add_bone("Forearm_R", shoulder_right);
    const std::size_t hand_left = add_bone("Hand_L", forearm_left);
    const std::size_t hand_right = add_bone("Hand_R", forearm_right);
    add_bone("Foot_L", pelvis);
    add_bone("Foot_R", pelvis);
    add_bone("Ear_L", head);
    add_bone("Ear_R", head);
    add_bone("Finger_L1", hand_left);
    add_bone("Finger_L2", hand_left);
    add_bone("Finger_R1", hand_right);
    add_bone("Finger_R2", hand_right);
    (void)back;

    assets::Mesh mesh;
    mesh.name = "SyntheticBodyMesh";
    mesh.vertices.resize(3);
    for (assets::ModelVertex& vertex : mesh.vertices) {
        vertex.bone_indices[0] = static_cast<std::uint32_t>(pelvis);
        vertex.bone_weights[0] = 1.0f;
    }
    mesh.indices = {0, 1, 2};
    mesh.material = 0;
    model->meshes.push_back(std::move(mesh));
    model->textures.push_back({"BodyColor", "body.png", 1, 1, {255, 255, 255, 255}});
    assets::Material material;
    material.name = "BodyMaterial";
    material.base_color_texture = 0;
    model->materials.push_back(material);

    animation::AnimationClip idle;
    idle.name = "Idle";
    idle.duration = 1.0f;
    model->animations.push_back(idle);

    animation::AnimationClip walk;
    walk.name = "Walk";
    walk.duration = 1.0f;
    animation::AnimationChannel pelvis_channel;
    pelvis_channel.bone_index = pelvis;
    pelvis_channel.translations.times = {0.0f, 1.0f};
    pelvis_channel.translations.values = {{{0.0f, 0.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}}};
    walk.channels.push_back(pelvis_channel);
    model->animations.push_back(walk);
    return model;
}

class SyntheticModelLoader final : public mmo::assets::ModelLoader {
public:
    explicit SyntheticModelLoader(mmo::assets::Model model)
        : model_(std::move(model))
    {
    }

    mmo::assets::Model load(const std::filesystem::path&) const override
    {
        return model_;
    }

private:
    mmo::assets::Model model_;
};

mmo::equipment::Equipment make_equipment(
    std::uint64_t id,
    const char* name,
    mmo::equipment::EquipmentSlot slot,
    std::initializer_list<const char*> bones)
{
    mmo::equipment::Equipment item;
    item.id = id;
    item.name = name;
    item.slot = slot;
    item.model_path = "assets/equipment/test_item.glb";
    for (const char* bone : bones) {
        item.attachments.push_back({bone, bone, {}});
    }
    return item;
}

void test_skeleton_animation_and_model()
{
    using namespace mmo;
    const auto body_model = make_body_model();
    require(body_model->skeleton.is_valid(), "synthetic body skeleton should be valid");
    require(body_model->skeleton.root_bone_index() == 0, "root bone should be discoverable");
    const auto pelvis_index = body_model->skeleton.find_bone("Pelvis");
    require(pelvis_index.has_value(), "skeleton should find arbitrary bone names");
    require(body_model->validate(), "synthetic skinned model should validate");

    SyntheticModelLoader loader(*body_model);
    assets::AssetSystem asset_system(loader);
    const assets::Model loaded = asset_system.load_model("assets/characters/player/body/player_body.glb");
    require(loaded.source_path.extension() == ".glb", "asset system should preserve model path");
    bool rejected_extension = false;
    try {
        (void)asset_system.load_model("body.obj");
    } catch (const std::invalid_argument&) {
        rejected_extension = true;
    }
    require(rejected_extension, "asset system should reject unsupported model extensions");

    animation::AnimationController controller(body_model->skeleton, body_model->animations);
    require(controller.bind_state(animation::AnimationState::Idle, 0), "bind Idle state");
    require(controller.bind_state(animation::AnimationState::Walk, 1), "bind Walk state");
    require(controller.set_state(animation::AnimationState::Idle), "enter Idle state");
    require(controller.set_state(animation::AnimationState::Walk, 1.0f), "cross-fade into Walk");
    controller.update(0.5f);
    require(controller.state() == animation::AnimationState::Walk, "controller should report Walk state");
    const float pelvis_y = controller.pose().local_transforms[*pelvis_index].translation[1];
    require(std::abs(pelvis_y - 0.25f) < 0.001f, "cross-fade should interpolate bone translation");
    require(std::abs(controller.pose().skin_matrices[*pelvis_index][13] - pelvis_y) < 0.001f,
        "pose should expose a prepared skin matrix");

    auto invalid_skeleton = body_model->skeleton;
    invalid_skeleton.bones[0].parent_index = 1;
    require(!invalid_skeleton.is_valid(), "skeleton validation should reject cycles");
}

void test_character_equipment()
{
    using namespace mmo;
    auto model = make_body_model();
    character::CharacterModel character_model(model);
    equipment::EquipmentManager& manager = character_model.equipment();

    const std::array<equipment::Equipment, 16> items{
        make_equipment(1, "Iron Helm", equipment::EquipmentSlot::Helmet, {"Head"}),
        make_equipment(2, "Twin Pauldrons", equipment::EquipmentSlot::Shoulders,
            {"Shoulder_L", "Shoulder_R"}),
        make_equipment(3, "Cuirass", equipment::EquipmentSlot::Chest, {"Chest"}),
        make_equipment(4, "Gloves", equipment::EquipmentSlot::Gloves, {"Hand_L", "Hand_R"}),
        make_equipment(5, "Pants", equipment::EquipmentSlot::Pants, {"Pelvis"}),
        make_equipment(6, "Boots", equipment::EquipmentSlot::Boots, {"Foot_L", "Foot_R"}),
        make_equipment(7, "Cloak", equipment::EquipmentSlot::Cloak, {"Back"}),
        make_equipment(8, "Bracers", equipment::EquipmentSlot::Bracers,
            {"Forearm_L", "Forearm_R"}),
        make_equipment(9, "Ring One", equipment::EquipmentSlot::Ring1, {"Finger_L1"}),
        make_equipment(10, "Ring Two", equipment::EquipmentSlot::Ring2, {"Finger_L2"}),
        make_equipment(11, "Ring Three", equipment::EquipmentSlot::Ring3, {"Finger_R1"}),
        make_equipment(12, "Ring Four", equipment::EquipmentSlot::Ring4, {"Finger_R2"}),
        make_equipment(13, "Left Earring", equipment::EquipmentSlot::Earring1, {"Ear_L"}),
        make_equipment(14, "Right Earring", equipment::EquipmentSlot::Earring2, {"Ear_R"}),
        make_equipment(15, "Sword", equipment::EquipmentSlot::MainHand, {"Hand_R"}),
        make_equipment(16, "Shield", equipment::EquipmentSlot::OffHand, {"Hand_L"}),
    };

    for (const equipment::Equipment& item : items) {
        require(manager.equip(item), "all equipment slots should accept valid attachments");
    }
    require(manager.resolved_attachments().size() == 20,
        "multi-bone armor and all accessories should resolve independently");

    equipment::Equipment replacement = make_equipment(101, "Silver Helm",
        equipment::EquipmentSlot::Helmet, {"Head"});
    require(manager.equip(replacement), "equipment should replace the current item in its slot");
    require(manager.get_equipped(equipment::EquipmentSlot::Helmet)->id == 101,
        "replacement item should be queryable without rebuilding CharacterModel");

    const equipment::Equipment invalid = make_equipment(102, "Broken Helm",
        equipment::EquipmentSlot::Helmet, {"MissingBone"});
    require(!manager.equip(invalid), "missing attachment bone should reject equipment");
    require(manager.get_equipped(equipment::EquipmentSlot::Helmet)->id == 101,
        "failed equip must leave the existing item unchanged");

    require(manager.unequip(equipment::EquipmentSlot::Helmet).has_value(),
        "unequip should return removed equipment data");
    require(!manager.has_equipped(equipment::EquipmentSlot::Helmet), "unequip should clear slot");
    require(manager.clear_slot(equipment::EquipmentSlot::Ring1), "clear_slot should remove an item");
    manager.clear();
    require(manager.resolved_attachments().empty(), "clear should remove all visual attachments");

    character::Character character(model);
    character.set_stats({150.0f, 100.0f, 0});
    require(character.stats().health == 100.0f && character.stats().level == 1,
        "gameplay stats should remain separate and clamp basic values");
}

void test_male_character_run_glb()
{
    mmo::assets::GltfModelLoader loader;
    const mmo::assets::Model model = loader.load("personagem/characterRIGGED.glb");
    std::string validation_reason;
    require(model.validate(&validation_reason),
        validation_reason.c_str());
    require(model.meshes.size() == 3,
        "male character GLB should load the face, eyes, and body meshes");
    require(model.skeleton.bones.size() == mmo::animation::kMaxSkinningBones,
        "male character GLB should load all 65 joints and the shared root");
    require(model.skeleton.find_bone("pelvis").has_value(),
        "male character GLB should preserve humanoid joint names");
    require(model.animations.size() == 1,
        "male character GLB should contain the retargeted run clip");
    require(model.animations.front().name == "Slow Run",
        "male character GLB should preserve the run clip name");
    require(model.animations.front().channels.size() >= 20,
        "retargeted run clip should animate the character skeleton");

    mmo::animation::AnimationController controller(model.skeleton, model.animations);
    require(controller.bind_state(mmo::animation::AnimationState::Walk, 0),
        "run clip should be bindable to the movement state");
    require(controller.set_state(mmo::animation::AnimationState::Walk, 0.0f),
        "run clip should start when entering the movement state");
    const auto initial_pose = controller.pose().skin_matrices;
    controller.update(model.animations.front().duration * 0.5f);
    bool pose_changed = false;
    for (std::size_t bone = 0; bone < initial_pose.size(); ++bone) {
        for (std::size_t component = 0; component < initial_pose[bone].size(); ++component) {
            if (std::abs(initial_pose[bone][component] -
                    controller.pose().skin_matrices[bone][component]) > 0.001f) {
                pose_changed = true;
                break;
            }
        }
        if (pose_changed) {
            break;
        }
    }
    require(pose_changed, "run clip should change the rendered skinning pose");
}

void test_universal_animation_pack()
{
    mmo::assets::GltfModelLoader loader;
    const mmo::assets::Model model = loader.load(
        "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb");
    std::string validation_reason;
    require(model.validate(&validation_reason), validation_reason.c_str());

    const auto has_clip = [&model](const std::string& name) {
        for (const mmo::animation::AnimationClip& clip : model.animations) {
            if (clip.name == name) {
                return true;
            }
        }
        return false;
    };
    float jump_duration = 0.0f;
    for (const char* name : {"Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop",
             "Sprint_Loop", "Jump_Start", "Jump_Loop", "Jump_Land",
             "Sword_Attack", "Hit_Chest", "Death01"}) {
        require(has_clip(name), "Universal Animation Library is missing a required action clip");
    }
    for (const mmo::animation::AnimationClip& clip : model.animations) {
        if (clip.name == "Jump_Loop") {
            jump_duration = clip.duration;
            break;
        }
    }
    require(jump_duration > 0.0f, "Jump_Loop should have a positive duration");

    mmo::animation::AnimationController controller(model.skeleton, model.animations);
    const auto bind_named_state = [&model, &controller](
        mmo::animation::AnimationState state, const std::string& name) {
        for (std::size_t index = 0; index < model.animations.size(); ++index) {
            if (model.animations[index].name == name) {
                return controller.bind_state(state, index);
            }
        }
        return false;
    };
    require(bind_named_state(mmo::animation::AnimationState::Idle, "Idle_Loop"),
        "Idle_Loop should bind to the Idle state");
    require(bind_named_state(mmo::animation::AnimationState::Walk, "Walk_Loop"),
        "Walk_Loop should bind to the Walk state");
    require(bind_named_state(mmo::animation::AnimationState::Run, "Jog_Fwd_Loop"),
        "Jog_Fwd_Loop should bind to the Run state");
    require(bind_named_state(mmo::animation::AnimationState::Jump, "Jump_Loop"),
        "Jump_Loop should bind to the Jump state");
    require(controller.set_state(mmo::animation::AnimationState::Jump, 0.0f),
        "Jump state should start");
    const auto initial_pose = controller.pose().skin_matrices;
    controller.update(jump_duration * 0.5f);
    bool pose_changed = false;
    for (std::size_t bone = 0; bone < initial_pose.size(); ++bone) {
        for (std::size_t component = 0; component < initial_pose[bone].size(); ++component) {
            if (std::abs(initial_pose[bone][component] -
                    controller.pose().skin_matrices[bone][component]) > 0.001f) {
                pose_changed = true;
                break;
            }
        }
        if (pose_changed) {
            break;
        }
    }
    require(pose_changed, "Universal jump clip should change the rendered skinning pose");
}

}

int main()
{
    try {
        test_skeleton_animation_and_model();
        test_character_equipment();
        test_male_character_run_glb();
        test_universal_animation_pack();
        std::cout << "CharacterEquipmentTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CharacterEquipmentTest failed: " << error.what() << '\n';
        return 1;
    }
}