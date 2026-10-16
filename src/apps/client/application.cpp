#include "apps/client/application.hpp"
#include "apps/client/mouse_camera_controls.hpp"

#include "animation/layered_animation.hpp"
#include "assets/gltf_model_loader.hpp"
#include "character/character.hpp"
#include "character/character_controller.hpp"
#include "character/combat.hpp"
#include "editor/editor_camera.hpp"
#include "editor/asset_database.hpp"
#include "editor/editor_history.hpp"
#include "editor/editor_ui.hpp"
#include "editor/selection.hpp"
#include "editor/world_document.hpp"
#include "editor/world_editor.hpp"
#include "apps/client/character_setup.hpp"
#include "apps/client/demo_enemies.hpp"
#include "apps/client/inventory_window.hpp"
#include "apps/client/player_items.hpp"
#include "apps/client/target_frame.hpp"
#include "game/combat/combat.hpp"
#include "apps/client/character_animation_setup.hpp"
#include "game/items/weapons.hpp"
#include "presentation/equipment_presenter.hpp"
#include "world/collision_world.hpp"
#include "world/terrain.hpp"
#include "platform/input_settings.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"
#include "scene/camera_controller.hpp"
#include "apps/client/combat_camera_controller.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

namespace mmo::app {
namespace {

constexpr char kTerrainPath[] = "content/worlds/demo.mmoterrain";
constexpr char kWorldPath[] = "content/worlds/demo.mmoworld";
constexpr float kPi = 3.14159265f;
constexpr char kItemsDirectory[] = "content/items";

glm::vec3 cursor_ray(
    const scene::CameraPose& camera,
    double cursor_x,
    double cursor_y,
    int window_width,
    int window_height)
{
    if (window_width <= 0 || window_height <= 0) {
        throw std::invalid_argument("window dimensions must be positive for terrain editing");
    }
    const glm::vec3 eye{camera.eye_x, camera.eye_y, camera.eye_z};
    const glm::vec3 focus{camera.focus_x, camera.focus_y, camera.focus_z};
    const glm::vec3 forward = glm::normalize(focus - eye);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0.0f, 1.0f, 0.0f}));
    const glm::vec3 up = glm::normalize(glm::cross(right, forward));
    const float aspect = static_cast<float>(window_width) /
        static_cast<float>(window_height);
    const float tangent = std::tan(camera.field_of_view * 0.5f);
    const float screen_x = static_cast<float>(2.0 * cursor_x / window_width - 1.0);
    const float screen_y = static_cast<float>(1.0 - 2.0 * cursor_y / window_height);
    return glm::normalize(
        forward + right * (screen_x * aspect * tangent) + up * (screen_y * tangent));
}

world::CollisionWorld make_demo_collision_world()
{
    using world::CollisionBox;
    const float half_extent = world::Terrain::kHalfExtent;
    return world::CollisionWorld(
        {-half_extent, half_extent, -half_extent, half_extent},
        {
            CollisionBox{{4.0f, 0.0f, -2.5f}, {4.6f, 3.0f, 2.5f}},
            CollisionBox{{-5.0f, 0.0f, -2.0f}, {-4.6f, 3.0f, 2.0f}},
        });
}

std::vector<editor::SelectableObject> make_demo_selectables(
    const world::CollisionWorld& collision_world)
{
    std::vector<editor::SelectableObject> objects;
    objects.reserve(collision_world.boxes().size());
    for (std::size_t index = 0; index < collision_world.boxes().size(); ++index) {
        const world::CollisionBox& box = collision_world.boxes()[index];
        objects.push_back({
            static_cast<editor::SelectionId>(index + 1),
            {box.minimum, box.maximum},
            {},
            {},
            glm::vec3{1.0f},
            "Wall " + std::to_string(index + 1),
        });
    }
    return objects;
}

}

int run_application()
{
    try {
        platform::GlfwRuntime glfw;
        platform::Window window(1280, 720, "MMO Engine - Movement Prototype");
        renderer::Renderer renderer;
        editor::EditorUI editor_ui(window.native_handle());
        scene::CameraController camera;
        game::client::CombatCameraController combat_camera;
        editor::WorldEditor world_editor;
        editor::EditorCamera editor_camera;
        world::Terrain terrain;
        std::optional<editor::WorldDocumentData> startup_world;
        const std::filesystem::path world_path{kWorldPath};
        const std::filesystem::path terrain_path{kTerrainPath};
        terrain.configure_region_storage(
            std::filesystem::path{world_path.string() + ".regions"});
        if (std::filesystem::exists(world_path)) {
            startup_world = editor::WorldDocument::load(world_path);
            terrain = startup_world->terrain;
            std::cout << "[world] loaded " << world_path.string() << '\n';
        } else if (std::filesystem::exists(terrain_path)) {
            terrain.load(terrain_path);
            std::cout << "[terrain] loaded " << terrain_path.string() << '\n';
        }
        world::CollisionWorld collision_world = make_demo_collision_world();
        editor::SelectionManager selection(make_demo_selectables(collision_world));
        if (startup_world) {
            selection.replace_objects(std::move(startup_world->objects));
        }
        editor::EditorHistory editor_history;
        const auto rebuild_editor_collision = [&]() {
            std::vector<world::CollisionBox> edited_boxes;
            edited_boxes.reserve(selection.objects().size());
            for (const editor::SelectableObject& object : selection.objects()) {
                if (!object.asset_path.empty()) {
                    continue;
                }
                const std::optional<editor::SelectionBounds> bounds =
                    selection.bounds_for(object.id);
                if (bounds) {
                    edited_boxes.push_back({bounds->minimum, bounds->maximum});
                }
            }
            collision_world = world::CollisionWorld(
                collision_world.bounds(), std::move(edited_boxes));
        };
        if (startup_world) {
            rebuild_editor_collision();
        }
        const platform::InputSettings input_settings{};

        assets::GltfModelLoader model_loader;
        assets::AssetSystem assets(model_loader);
        editor::AssetDatabase asset_database(std::filesystem::current_path());
        asset_database.refresh();
        std::cout << "[assets] indexed " << asset_database.entries().size()
            << " models and prefabs.\n";
        const auto load_editor_models = [&]() {
            for (const editor::SelectableObject& object : selection.objects()) {
                if (object.asset_path.empty()) {
                    continue;
                }
                const std::filesystem::path model_path =
                    asset_database.resolve_model_path(object.asset_path);
                assets::Model model = assets.load_model(model_path);
                (void)renderer.register_editor_model(object.asset_path, model);
            }
        };
        if (startup_world) {
            load_editor_models();
        }
        const auto save_world = [&]() {
            editor::WorldDocument::save(world_path, terrain, selection.objects());
            editor_ui.set_status("Mundo salvo: " + world_path.generic_string(), false);
            std::cout << "[world] saved " << world_path.string() << '\n';
        };
        const auto load_world = [&]() {
            if (std::filesystem::exists(world_path)) {
                editor::WorldDocumentData loaded =
                    editor::WorldDocument::load(world_path);
                for (const editor::SelectableObject& object : loaded.objects) {
                    if (object.asset_path.empty()) {
                        continue;
                    }
                    assets::Model model = assets.load_model(
                        asset_database.resolve_model_path(object.asset_path));
                    (void)renderer.register_editor_model(object.asset_path, model);
                }
                terrain = std::move(loaded.terrain);
                terrain.invalidate_render_chunks();
                selection.replace_objects(std::move(loaded.objects));
            } else if (std::filesystem::exists(terrain_path)) {
                terrain.load(terrain_path);
            } else {
                throw std::runtime_error(
                    "Nenhum mundo salvo ou terreno legado foi encontrado");
            }
            renderer.set_terrain(terrain);
            rebuild_editor_collision();
            editor_history.clear();
            editor_ui.set_status("Mundo carregado: " +
                (std::filesystem::exists(world_path)
                    ? world_path.generic_string() : terrain_path.generic_string()), false);
            std::cout << "[world] loaded "
                << (std::filesystem::exists(world_path)
                    ? world_path.string() : terrain_path.string()) << '\n';
        };
        assets::Model loaded_model = assets.load_model(
            "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb");
        const std::size_t backward_walk_index =
            game::client::add_backward_walk_clip(loaded_model);
        // Optional locally generated clips are not redistributed with the project.
        std::filesystem::path melee_animation_path{
            "assets/animations/generated/melee_combat_ual.glb"};
        if (!std::filesystem::exists(melee_animation_path)) {
            melee_animation_path = "assets/animations/generated/combat_strafe_ual.glb";
        }
        if (std::filesystem::exists(melee_animation_path)) {
            assets::append_animations(
                loaded_model, assets.load_model(melee_animation_path));
            const auto left_strafe = std::find_if(
                loaded_model.animations.begin(), loaded_model.animations.end(),
                [](const animation::AnimationClip& clip) {
                    return clip.name == "Melee_StrafeRun_Left";
                });
            const std::optional<std::size_t> pelvis =
                loaded_model.skeleton.find_bone("pelvis");
            if (left_strafe != loaded_model.animations.end() && pelvis) {
                animation::hold_root_translation_axis(*left_strafe, *pelvis, 0);
            }
            game::client::compose_running_animations(loaded_model);
        } else {
            std::cout << "[animation] optional retargeted melee locomotion is missing; "
                << "using built-in UAL locomotion. Run "
                << "scripts/retarget_melee_animations.js with FBX2glTF to enable "
                << "Human Melee combat movement.\n";
        }
        const std::filesystem::path weapon_animation_path{
            "assets/animations/generated/weapon_animations_ual.glb"};
        if (std::filesystem::exists(weapon_animation_path)) {
            assets::append_animations(
                loaded_model, assets.load_model(weapon_animation_path));
        } else {
            std::cout << "[animation] optional weapon animation pack is missing; "
                << "run scripts/retarget_melee_animations.js with --weapon-packs "
                << "to enable the imported attacks, longbow locomotion and running jump.\n";
        }
        auto body_model = std::make_shared<const assets::Model>(std::move(loaded_model));
        character::Character player(body_model);
        game::client::bind_locomotion_animations(player);
        if (!player.model().animation().bind_state(
                animation::AnimationState::Walk, backward_walk_index)) {
            throw std::runtime_error("Could not bind the backward-walking animation");
        }
        character::CharacterController controller(player);
        // Gameplay entities. Animation never owns damage or targets: it only emits events.
        game::combat::EntityRegistry combat_registry;
        game::combat::DamageSystem damage_system;
        game::combat::TargetSystem targets;
        game::combat::CombatSystem combat_system(
            game::combat::CombatSettings{}, targets, damage_system);
        game::combat::AbilitySystem warrior_abilities(
            game::combat::CombatClass::Warrior, targets, damage_system);
        game::combat::AbilitySystem mage_abilities(
            game::combat::CombatClass::Mage, targets, damage_system);
        game::combat::AbilitySystem archer_abilities(
            game::combat::CombatClass::Archer, targets, damage_system);
        game::combat::AbilitySystem* active_abilities = &warrior_abilities;
        game::combat::EntityId player_entity_id = game::combat::kInvalidEntity;
        {
            game::combat::CombatEntity player_entity;
            player_entity.name = "Player";
            player_entity.faction = game::combat::Faction::Player;
            player_entity.max_hp = 100.0f;
            player_entity.hp = 100.0f;
            player_entity_id = combat_registry.create(player_entity);
        }
        const animation::Vector3& start = player.transform().position;
        DemoEnemies enemies(body_model, combat_registry, {start[0], start[1], start[2]});
        // Reactions to damage live outside the combat rules: they only drive presentation.
        damage_system.add_listener([&](const game::combat::DamageEvent& event) {
            std::cout << "[combat] damage " << event.amount << " to entity " << event.target
                << (event.killed ? " (killed)" : "") << '\n';
            enemies.on_damage(event);
        });
        character::CombatSystem combat_system_stats;
        character::CombatController combat(player, character::default_combat_actions());
        game::combat::CombatContext combat_context;
        combat.set_event_handler([&](const character::CombatEvent& event) {
            combat_system_stats.handle(event);
            if (event.type == character::CombatEventType::AttackHit &&
                event.state != character::CombatState::Cast) {
                (void)combat_system.on_attack_hit(
                    combat_registry, combat_context, &collision_world);
            } else if (event.type == character::CombatEventType::AttackEnd) {
                combat_system.on_swing_end();
            }
        });
        const auto& weapons = game::items::weapon_catalog();
        const game::items::WeaponDefinition* selected_weapon = &weapons.front();
        std::optional<std::string_view> selected_weapon_animation;
        const auto select_weapon = [&](std::size_t index) {
            selected_weapon = &weapons[index];
            active_abilities = selected_weapon->combat_class == game::combat::CombatClass::Mage
                ? &mage_abilities
                : selected_weapon->combat_class == game::combat::CombatClass::Archer
                    ? &archer_abilities : &warrior_abilities;
            selected_weapon_animation =
                game::client::find_weapon_animation(
                    *selected_weapon, body_model->animations);
            std::vector<character::CombatActionDefinition> actions;
            for (const std::string_view name : selected_weapon->attack_animations) {
                if (name.empty() ||
                    !game::client::has_animation(body_model->animations, name)) {
                    continue;
                }
                const animation::AnimationClip& clip = body_model->animations[
                    game::client::find_clip_index(body_model->animations, name)];
                const float duration = clip.duration;
                actions.push_back({character::CombatState::Attack,
                    std::string(name), duration, 1.0f,
                    duration * 0.5f, 0.1f, 0.15f});
            }
            if (selected_weapon_animation &&
                selected_weapon->combat_class == game::combat::CombatClass::Mage) {
                const animation::AnimationClip& clip = body_model->animations[
                    game::client::find_clip_index(
                        body_model->animations, *selected_weapon_animation)];
                const float duration = clip.duration;
                actions.push_back({character::CombatState::Cast,
                    std::string(*selected_weapon_animation), duration, 1.0f,
                    duration * 0.5f, 0.1f, 0.15f});
            }
            combat.set_actions(std::move(actions));
            if (!selected_weapon_animation) {
                combat_system.set_auto_attack(false);
            }
            if (selected_weapon->type == game::items::WeaponType::Archer) {
                const character::LocomotionAnimationSet longbow_clips{
                    "Longbow_Idle", "Longbow_Walk_Forward", "Longbow_Walk_Backward",
                    "Longbow_Walk_Left", "Longbow_Walk_Right", "Longbow_Run_Forward",
                    "Longbow_Run_Backward", "Longbow_Run_Left", "Longbow_Run_Right"};
                const std::array<std::string_view, 9> names{
                    longbow_clips.idle, longbow_clips.walk_forward,
                    longbow_clips.walk_backward, longbow_clips.walk_left,
                    longbow_clips.walk_right, longbow_clips.run_forward,
                    longbow_clips.run_backward, longbow_clips.run_left,
                    longbow_clips.run_right};
                if (std::all_of(names.begin(), names.end(),
                        [&](std::string_view name) {
                            return game::client::has_animation(body_model->animations, name);
                        })) {
                    controller.set_locomotion_animation_set(longbow_clips);
                } else {
                    controller.set_locomotion_animation_set(std::nullopt);
                }
            } else {
                controller.set_locomotion_animation_set(std::nullopt);
            }
            std::cout << "[weapon] selected " << selected_weapon->name;
            if (selected_weapon_animation) {
                std::cout << " (animation: " << *selected_weapon_animation << ')';
            } else {
                std::cout << " (no bow attack animation available)";
            }
            std::cout << '\n';
        };
        select_weapon(0);
        bool previous_target_click = false;
        bool previous_right_down = false;
        double right_drag_distance = 0.0;
        double right_click_x = 0.0;
        double right_click_y = 0.0;
        bool previous_left_camera_dragging = false;
        double left_drag_distance = 0.0;
        double left_click_x = 0.0;
        double left_click_y = 0.0;
        MouseCameraMode previous_mouse_camera_mode = MouseCameraMode::none;
        float left_camera_movement_yaw = kPi;
        bool manual_camera_orientation_locked = false;
        bool previous_tab = false;
        bool previous_clear_target_key = false;
        bool previous_auto_attack_key = false;
        bool previous_lock_on_key = false;
        bool previous_ability_two_key = false;
        bool previous_ability_three_key = false;
        std::array<bool, 5> previous_weapon_keys{};
        bool previous_debug_toggle = false;
        bool animation_debug = false;
        double next_debug_log = 0.0;
        renderer.set_skinned_model(player.model().body().model());
        presentation::EquipmentPresenter equipment_presenter(assets, renderer);
        auto& equipment = player.model().equipment();
        // Items come from content/items/**/*.mmoitem; the player starts with the training sword.
        // The equipped item sets the model in hand, damage, interval and range; keys 4-8 still
        // pick the combat class, abilities and the attack variations used without an item.
        PlayerItems items{std::filesystem::path{kItemsDirectory}};
        items.give("wooden_sword");
        if (std::string reason; !items.toggle_main_hand(&reason)) {
            throw std::runtime_error("could not equip starting weapon: " + reason);
        }
        items.apply(equipment, combat_system);
        std::cout << "[items] loaded " << items.catalog().definitions().size()
            << " item definitions.\n";
        bool previous_equipment_toggle = false;
        bool previous_inventory_toggle = false;
        bool inventory_open = false;
        renderer.set_terrain(terrain);
        const animation::Vector3& initial_position = player.transform().position;
        scene::CameraPose last_game_camera_pose = camera.update(
            0.0f,
            {{initial_position[0], initial_position[1], initial_position[2]},
                controller.yaw()},
            &collision_world);

        const auto& clips = body_model->animations;
        std::cout << "[app] Movement prototype running. WASD moves relative to the camera; "
            << "always running; Space jumps; hold left or right mouse and drag to orbit freely (camera smoothly returns behind); right-click selects/engages or exits combat when not dragged; left click selects a target; Tab/Shift+Tab cycles targets; L toggles combat lock-on; 1 toggles auto attack; F3 prints animation layer debug; F4 equips/removes the main-hand item; I opens the inventory; "
            << "scroll zooms; [ and ] preview every animation; F1 toggles World Editor; "
            << "4 Greatsword, 5 Archer, 6 Mage, 7 Sword+Shield, 8 Axe; Escape exits.\n"
            << "[editor] F1 toggles editor; RMB + mouse looks; WASD moves; Q/E descend/ascend; "
            << "Shift speeds up; Ctrl slows down; wheel adjusts speed; M/R/T select move/rotate/scale, "
            << "select an object and drag LMB to transform; X/Y/Z/U select axes, "
            << "G toggles snap and C toggles world/local space.\n"
            << "[terrain] in World Editor, F2 toggles terrain tools; 1 raise, 2 lower, "
            << "3 flatten, 4 material paint; in paint mode 1 grass, 2 dirt, "
            << "3 rock, 5 sand; left mouse edits; wheel changes brush size; "
            << "Ctrl+S saves the world; Ctrl+L loads; Ctrl+Z/Y undo/redo. "
            << "The editor toolbar has the same actions.\n"
            << "[model] loaded " << body_model->meshes.size() << " meshes, "
            << body_model->skeleton.bones.size() << " bones and "
            << clips.size() << " available animation clips.\n"
            << "[world] two visible static walls provide character and camera collision.\n";

        std::size_t preview_index = 0;
        bool previous_clip_back = false;
        bool previous_clip_forward = false;
        bool previous_terrain_tool_toggle = false;
        bool previous_save = false;
        bool previous_load = false;
        bool previous_brush = false;
        bool previous_flattening = false;
        bool previous_selection_click = false;
        bool previous_focus_pressed = false;
        bool previous_snap_toggle = false;
        bool previous_space_toggle = false;
        bool previous_undo = false;
        bool previous_redo = false;
        bool previous_transform_x = false;
        bool previous_transform_y = false;
        bool previous_transform_z = false;
        bool previous_transform_uniform = false;
        bool terrain_editor_active = false;
        bool brush_cursor_valid = false;
        float brush_radius = 3.0f;
        float brush_strength = 1.8f;
        float flatten_height = 0.0f;
        glm::vec2 brush_center{0.0f};
        world::TerrainBrush selected_brush = world::TerrainBrush::raise;
        world::TerrainMaterial selected_material =
            world::TerrainMaterial::grass;
        double previous_time = window.time_seconds();

        while (!window.should_close()) {
            window.poll_events();
            editor_ui.begin_frame();
            editor::EditorSnapshot frame_before =
                editor::capture_snapshot(terrain, selection);
            bool world_changed_this_frame = false;
            std::string history_key;
            if (window.escape_pressed()) {
                window.request_close();
            }

            if (world_editor.update_toggle(window.key_pressed(platform::Key::F1))) {
                if (world_editor.is_active()) {
                    editor_camera.set_pose(last_game_camera_pose);
                    std::cout << "[editor] World Editor Mode enabled; game controls paused.\n";
                } else {
                    terrain_editor_active = false;
                    std::cout << "[editor] Game Mode restored.\n";
                }
            }

            const bool terrain_tool_toggle =
                world_editor.is_active() && !editor_ui.wants_keyboard_capture() &&
                window.key_pressed(platform::Key::F2);
            if (terrain_tool_toggle && !previous_terrain_tool_toggle) {
                terrain_editor_active = !terrain_editor_active;
                std::cout << "[terrain] tools "
                    << (terrain_editor_active ? "enabled" : "disabled") << '\n';
            }
            previous_terrain_tool_toggle = terrain_tool_toggle;
            if (!world_editor.is_active()) {
                terrain_editor_active = false;
            }

            const bool save_pressed =
                world_editor.is_active() &&
                !editor_ui.wants_keyboard_capture() &&
                window.key_pressed(platform::Key::LeftControl) &&
                window.key_pressed(platform::Key::S);
            if (save_pressed && !previous_save) {
                try {
                    save_world();
                } catch (const std::exception& error) {
                    const std::string message =
                        "Não foi possível salvar o mundo: " +
                        std::string(error.what());
                    std::cerr << "[world] " << message << '\n';
                    editor_ui.set_status(message, true);
                }
            }
            previous_save = save_pressed;

            const bool load_pressed =
                world_editor.is_active() &&
                !editor_ui.wants_keyboard_capture() &&
                window.key_pressed(platform::Key::LeftControl) &&
                window.key_pressed(platform::Key::L);
            if (load_pressed && !previous_load) {
                try {
                    load_world();
                    frame_before = editor::capture_snapshot(terrain, selection);
                } catch (const std::exception& error) {
                    const std::string message =
                        "Não foi possível carregar o mundo: " +
                        std::string(error.what());
                    std::cerr << "[world] " << message << '\n';
                    editor_ui.set_status(message, true);
                }
            }
            previous_load = load_pressed;
            const bool control_down = window.key_pressed(platform::Key::LeftControl);
            const bool undo_pressed = world_editor.is_active() && control_down &&
                !editor_ui.wants_keyboard_capture() &&
                window.key_pressed(platform::Key::Z);
            const bool redo_pressed = world_editor.is_active() && control_down &&
                !editor_ui.wants_keyboard_capture() &&
                window.key_pressed(platform::Key::Y);
            const bool undo_requested = undo_pressed && !previous_undo;
            const bool redo_requested = redo_pressed && !previous_redo;
            previous_undo = undo_pressed;
            previous_redo = redo_pressed;

            const bool game_mode = world_editor.mode() == editor::EngineMode::game;
            const bool right_dragging =
                window.right_mouse_pressed() && world_editor.is_active() &&
                !editor_ui.wants_mouse_capture();
            const bool camera_mouse_input =
                game_mode && !editor_ui.wants_mouse_capture();
            const bool left_mouse_down =
                camera_mouse_input && window.left_mouse_pressed();
            const bool right_mouse_down =
                camera_mouse_input && window.right_mouse_pressed();
            const MouseCameraMode camera_mode =
                mouse_camera_mode(left_mouse_down, right_mouse_down);
            const bool left_camera_dragging = camera_mode == MouseCameraMode::left ||
                camera_mode == MouseCameraMode::both;
            const bool right_camera_dragging = camera_mode == MouseCameraMode::right ||
                camera_mode == MouseCameraMode::both;
            const bool both_camera_buttons = camera_mode == MouseCameraMode::both;
            if (camera_mode == MouseCameraMode::left &&
                previous_mouse_camera_mode != MouseCameraMode::left) {
                left_camera_movement_yaw = last_game_camera_pose.horizontal_yaw();
            }
            previous_mouse_camera_mode = camera_mode;
            if (right_camera_dragging && !previous_right_down) {
                window.cursor_position(right_click_x, right_click_y);
                right_drag_distance = 0.0;
            }
            if (left_camera_dragging && !previous_left_camera_dragging) {
                window.cursor_position(left_click_x, left_click_y);
                left_drag_distance = 0.0;
            }
            window.set_cursor_captured(
                right_dragging || right_camera_dragging || left_camera_dragging);
            double mouse_delta_x = 0.0;
            double mouse_delta_y = 0.0;
            double scroll_delta = 0.0;
            window.consume_mouse_input(mouse_delta_x, mouse_delta_y, scroll_delta);
            if (rotates_camera(camera_mode) &&
                (mouse_delta_x != 0.0 || mouse_delta_y != 0.0)) {
                manual_camera_orientation_locked = true;
            }
            if (terrain_editor_active && scroll_delta != 0.0) {
                brush_radius = std::clamp(
                    brush_radius + static_cast<float>(scroll_delta) * 0.5f, 1.0f, 12.0f);
                std::cout << "[terrain] brush radius " << brush_radius << '\n';
            }
            if (terrain_editor_active) {
                const world::TerrainBrush previous_selection = selected_brush;
                const world::TerrainMaterial previous_material = selected_material;
                if (!editor_ui.wants_keyboard_capture()) {
                    if (window.key_pressed(platform::Key::Digit4)) {
                        selected_brush = selected_brush ==
                                world::TerrainBrush::paint_material
                            ? world::TerrainBrush::raise
                            : world::TerrainBrush::paint_material;
                    } else if (window.key_pressed(platform::Key::Digit1)) {
                        if (selected_brush == world::TerrainBrush::paint_material) {
                            selected_material = world::TerrainMaterial::grass;
                        } else {
                            selected_brush = world::TerrainBrush::raise;
                        }
                    } else if (window.key_pressed(platform::Key::Digit2)) {
                        if (selected_brush == world::TerrainBrush::paint_material) {
                            selected_material = world::TerrainMaterial::dirt;
                        } else {
                            selected_brush = world::TerrainBrush::lower;
                        }
                    } else if (window.key_pressed(platform::Key::Digit3)) {
                        if (selected_brush == world::TerrainBrush::paint_material) {
                            selected_material = world::TerrainMaterial::rock;
                        } else {
                            selected_brush = world::TerrainBrush::flatten;
                        }
                    } else if (window.key_pressed(platform::Key::Digit5) &&
                        selected_brush == world::TerrainBrush::paint_material) {
                        selected_material = world::TerrainMaterial::sand;
                    }
                }
                if (selected_brush != previous_selection ||
                    selected_material != previous_material) {
                    const char* const brush_name =
                        selected_brush == world::TerrainBrush::raise ? "raise" :
                        selected_brush == world::TerrainBrush::lower ? "lower" :
                        selected_brush == world::TerrainBrush::flatten ? "flatten" :
                        "material paint";
                    const char* const material_name =
                        selected_material == world::TerrainMaterial::grass ? "grass" :
                        selected_material == world::TerrainMaterial::dirt ? "dirt" :
                        selected_material == world::TerrainMaterial::rock ? "rock" :
                        "sand";
                    std::cout << "[terrain] brush " << brush_name;
                    if (selected_brush == world::TerrainBrush::paint_material) {
                        std::cout << " / " << material_name;
                    }
                    std::cout << '\n';
                }
            }

            const double current_time = window.time_seconds();
            const float delta_seconds = std::clamp(
                static_cast<float>(current_time - previous_time), 0.0f, 0.1f);
            previous_time = current_time;
            std::optional<game::combat::EntityId> right_click_lock_target;
            scene::CameraPose camera_pose{};
            const animation::Vector3& position = player.transform().position;
            if (left_camera_dragging) {
                left_drag_distance += std::abs(mouse_delta_x) + std::abs(mouse_delta_y);
            }
            if (right_camera_dragging) {
                right_drag_distance += std::abs(mouse_delta_x) + std::abs(mouse_delta_y);
            }
            if (both_camera_buttons) {
                left_drag_distance = std::max(left_drag_distance, 6.0);
                right_drag_distance = std::max(right_drag_distance, 6.0);
            }

            if (world_editor.is_active() && !terrain_editor_active && !right_dragging &&
                !control_down &&
                !editor_ui.wants_keyboard_capture()) {
                if (window.key_pressed(platform::Key::M)) {
                    selection.set_transform_mode(editor::TransformMode::translate);
                } else if (window.key_pressed(platform::Key::R)) {
                    selection.set_transform_mode(editor::TransformMode::rotate);
                } else if (window.key_pressed(platform::Key::T)) {
                    selection.set_transform_mode(editor::TransformMode::scale);
                }
                const bool axis_x = window.key_pressed(platform::Key::X);
                const bool axis_y = window.key_pressed(platform::Key::Y);
                const bool axis_z = window.key_pressed(platform::Key::Z);
                const bool axis_uniform = window.key_pressed(platform::Key::U);
                if (axis_x && !previous_transform_x) {
                    selection.set_transform_axis(editor::TransformAxis::x);
                } else if (axis_y && !previous_transform_y) {
                    selection.set_transform_axis(editor::TransformAxis::y);
                } else if (axis_z && !previous_transform_z) {
                    selection.set_transform_axis(editor::TransformAxis::z);
                } else if (axis_uniform && !previous_transform_uniform) {
                    selection.set_transform_axis(editor::TransformAxis::uniform);
                }
                previous_transform_x = axis_x;
                previous_transform_y = axis_y;
                previous_transform_z = axis_z;
                previous_transform_uniform = axis_uniform;

                const bool snap_toggle = window.key_pressed(platform::Key::G);
                if (snap_toggle && !previous_snap_toggle) {
                    selection.set_snapping_enabled(!selection.snapping_enabled());
                    std::cout << "[editor] transform snap "
                        << (selection.snapping_enabled() ? "enabled" : "disabled") << '\n';
                }
                previous_snap_toggle = snap_toggle;
                const bool space_toggle = window.key_pressed(platform::Key::C);
                if (space_toggle && !previous_space_toggle) {
                    selection.set_transform_space(
                        selection.transform_space() == editor::TransformSpace::world
                            ? editor::TransformSpace::local
                            : editor::TransformSpace::world);
                    std::cout << "[editor] transform space "
                        << (selection.transform_space() == editor::TransformSpace::world
                                ? "world" : "local") << '\n';
                }
                previous_space_toggle = space_toggle;
            } else {
                previous_snap_toggle = false;
                previous_space_toggle = false;
                previous_transform_x = false;
                previous_transform_y = false;
                previous_transform_z = false;
                previous_transform_uniform = false;
            }

            character::CharacterControllerInput input;
            if (game_mode) {
                input.forward =
                    (window.key_pressed(input_settings.move_forward) ? 1.0f : 0.0f) -
                    (window.key_pressed(input_settings.move_backward) ? 1.0f : 0.0f);
                input.strafe =
                    (window.key_pressed(input_settings.strafe_right) ? 1.0f : 0.0f) -
                    (window.key_pressed(input_settings.strafe_left) ? 1.0f : 0.0f);
                input.jump_pressed = window.key_pressed(input_settings.jump);
            }

            const bool clip_back = game_mode &&
                window.key_pressed(input_settings.previous_animation);
            const bool clip_forward = game_mode &&
                window.key_pressed(input_settings.next_animation);
            const std::array<platform::Key, 5> weapon_keys{
                platform::Key::Digit4, platform::Key::Digit5,
                platform::Key::Digit6, platform::Key::Digit7, platform::Key::Digit8};
            for (std::size_t index = 0; index < weapon_keys.size(); ++index) {
                const bool pressed = game_mode && !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(weapon_keys[index]);
                if (pressed && !previous_weapon_keys[index]) {
                    select_weapon(index);
                }
                previous_weapon_keys[index] = pressed;
            }
            if (clip_back && !previous_clip_back) {
                preview_index = (preview_index + clips.size() - 1) % clips.size();
                if (!controller.preview_animation(clips[preview_index].name)) {
                    throw std::runtime_error(
                        "Could not play animation: " + clips[preview_index].name);
                }
                std::cout << "[animation] preview " << clips[preview_index].name << '\n';
            } else if (clip_forward && !previous_clip_forward) {
                preview_index = (preview_index + 1) % clips.size();
                if (!controller.preview_animation(clips[preview_index].name)) {
                    throw std::runtime_error(
                        "Could not play animation: " + clips[preview_index].name);
                }
                std::cout << "[animation] preview " << clips[preview_index].name << '\n';
            }
            previous_clip_back = clip_back;
            previous_clip_forward = clip_forward;
            const bool equipment_toggle = window.key_pressed(platform::Key::F4);
            if (game_mode && !editor_ui.wants_keyboard_capture() &&
                equipment_toggle && !previous_equipment_toggle) {
                std::string reason;
                if (items.toggle_main_hand(&reason)) {
                    const auto main_hand = items.loadout().equipped(game::items::ItemSlot::MainHand);
                    std::cout << "[items] main hand "
                        << (main_hand ? items.catalog().find(*main_hand)->name : "empty") << '\n';
                } else {
                    std::cout << "[items] " << reason << '\n';
                }
            }
            previous_equipment_toggle = equipment_toggle;
            const bool inventory_toggle = window.key_pressed(platform::Key::I);
            if (game_mode && !editor_ui.wants_keyboard_capture() &&
                inventory_toggle && !previous_inventory_toggle) {
                inventory_open = !inventory_open;
            }
            previous_inventory_toggle = inventory_toggle;

            if (game_mode) {
                const animation::Vector3& player_position = player.transform().position;
                game::combat::CombatEntity& player_entity = *combat_registry.find(player_entity_id);
                player_entity.position = {player_position[0], player_position[1], player_position[2]};
                const glm::vec3 camera_forward{
                    last_game_camera_pose.focus_x - last_game_camera_pose.eye_x,
                    0.0f,
                    last_game_camera_pose.focus_z - last_game_camera_pose.eye_z};
                const float camera_yaw_forward = std::atan2(camera_forward.x, camera_forward.z);

                // Target selection: click picks an entity, clicking empty space clears.
                const bool target_click = !left_camera_dragging &&
                    !right_camera_dragging && !editor_ui.wants_mouse_capture() &&
                    previous_left_camera_dragging;
                if (target_click && !previous_target_click) {
                    int window_width = 0;
                    int window_height = 0;
                    window.window_size(window_width, window_height);
                    if (left_drag_distance < 6.0) {
                        const glm::vec3 direction = cursor_ray(
                            last_game_camera_pose, left_click_x, left_click_y,
                            window_width, window_height);
                        const game::combat::EntityId picked = game::combat::pick_entity(combat_registry,
                            {last_game_camera_pose.eye_x, last_game_camera_pose.eye_y,
                                last_game_camera_pose.eye_z},
                            direction, 80.0f);
                        if (picked != game::combat::kInvalidEntity) {
                            targets.select(combat_registry, player_entity, picked);
                        } else {
                            targets.clear();
                        }
                    }
                    left_drag_distance = 0.0;
                }
                previous_target_click = target_click;
                // Right-click still engages combat only when the gesture stayed a click.
                const bool right_down = right_camera_dragging;
                if (!right_down && previous_right_down) {
                    if (!left_camera_dragging && !editor_ui.wants_mouse_capture() &&
                        right_drag_distance < 6.0) {
                        int window_width = 0;
                        int window_height = 0;
                        window.window_size(window_width, window_height);
                        const glm::vec3 direction = cursor_ray(last_game_camera_pose,
                            right_click_x, right_click_y, window_width, window_height);
                        const game::combat::EntityId picked = game::combat::pick_entity(
                            combat_registry,
                            {last_game_camera_pose.eye_x, last_game_camera_pose.eye_y,
                                last_game_camera_pose.eye_z},
                            direction, 80.0f);
                        if (picked != game::combat::kInvalidEntity) {
                            targets.select(combat_registry, player_entity, picked);
                            const game::combat::CombatEntity* picked_entity =
                                combat_registry.find(picked);
                            if (picked_entity != nullptr && picked_entity->alive() &&
                                picked_entity->faction == game::combat::Faction::Hostile) {
                                combat_system.set_combat_engaged(true);
                                right_click_lock_target = picked;
                                if (selected_weapon_animation) {
                                    combat_system.set_auto_attack(true);
                                } else {
                                    combat_system.set_auto_attack(false);
                                    std::cout << "[weapon] bow animation unavailable; "
                                        << "use archer abilities instead\n";
                                }
                                std::cout << "[combat] engaged " << picked_entity->name << '\n';
                            } else {
                                combat_system.set_auto_attack(false);
                                combat_system.set_combat_engaged(false);
                                right_click_lock_target.reset();
                            }
                        } else {
                            combat_system.set_auto_attack(false);
                            combat_system.set_combat_engaged(false);
                            right_click_lock_target.reset();
                            std::cout << "[combat] left combat\n";
                        }
                    }
                }
                if (!right_down) {
                    right_drag_distance = 0.0;
                }
                previous_right_down = right_down;

                const bool tab_pressed = !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(platform::Key::Tab);
                if (tab_pressed && !previous_tab) {
                    targets.cycle(combat_registry, player_entity, camera_yaw_forward,
                        window.key_pressed(platform::Key::LeftShift) ||
                        window.key_pressed(platform::Key::RightShift));
                }
                previous_tab = tab_pressed;
                const bool clear_target_key = !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(platform::Key::Digit0);
                if (clear_target_key && !previous_clear_target_key) {
                    targets.clear();
                    combat_system.set_auto_attack(false);
                    combat_system.set_combat_engaged(false);
                    right_click_lock_target.reset();
                }
                previous_clear_target_key = clear_target_key;
                const bool auto_attack_key = !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(platform::Key::Digit1);
                if (auto_attack_key && !previous_auto_attack_key) {
                    if (selected_weapon_animation) {
                        combat_system.toggle_auto_attack();
                    } else {
                        std::cout << "[weapon] auto attack unavailable without a bow animation\n";
                    }
                    std::cout << "[combat] auto attack "
                        << (combat_system.auto_attack_enabled() ? "on" : "off") << '\n';
                }
                previous_auto_attack_key = auto_attack_key;
                combat_context = {
                    player_entity_id, player_entity.position, controller.yaw(), true};
                const bool ability_two_key = !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(platform::Key::Digit2);
                if (ability_two_key && !previous_ability_two_key) {
                    const auto result = active_abilities->cast(
                        combat_registry, combat_context, &collision_world,
                        std::string(selected_weapon->basic_ability_id));
                    if (result.damage) {
                        active_abilities->gain_resource(10.0f);
                    }
                    if (result.block == game::combat::AbilityBlock::None &&
                        selected_weapon->combat_class == game::combat::CombatClass::Mage) {
                        (void)combat.request(character::CombatState::Cast);
                    }
                    std::cout << "[combat] ability " << selected_weapon->basic_ability_id << ": "
                        << game::combat::to_string(result.block) << '\n';
                }
                previous_ability_two_key = ability_two_key;
                const bool ability_three_key = !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(platform::Key::Digit3);
                if (ability_three_key && !previous_ability_three_key) {
                    const auto result = active_abilities->cast(
                        combat_registry, combat_context, &collision_world,
                        std::string(selected_weapon->power_ability_id));
                    if (result.block == game::combat::AbilityBlock::None &&
                        selected_weapon->combat_class == game::combat::CombatClass::Mage) {
                        (void)combat.request(character::CombatState::Cast);
                    }
                    std::cout << "[combat] ability " << selected_weapon->power_ability_id << ": "
                        << game::combat::to_string(result.block) << '\n';
                }
                previous_ability_three_key = ability_three_key;

                // Combat only reads a snapshot of the player; it never touches movement input.
                targets.update(combat_registry);
                active_abilities->update(delta_seconds);
                combat_system.update(delta_seconds, combat_registry, combat_context,
                    &collision_world, [&combat] {
                        // The equipped item's clip, or the selected class's variations.
                        return combat.request_primary_attack();
                    });
                controller.set_facing_target(
                    combat_system.desired_facing(combat_registry, combat_context));
                controller.set_facing_rotation_speed(combat_system.settings().rotation_speed);
                const game::combat::CombatState combat_state = combat_system.state();
                const bool combat_camera_active =
                    combat_state == game::combat::CombatState::Combat ||
                    combat_state == game::combat::CombatState::Attacking ||
                    combat_state == game::combat::CombatState::Casting;
                if (combat_camera_active && !combat_camera.active()) {
                    combat_camera.enter(camera);
                } else if (!combat_camera_active && combat_camera.active()) {
                    combat_camera.leave(camera);
                }
                if (combat_camera.active() && right_click_lock_target) {
                    combat_camera.set_lock_target(*right_click_lock_target);
                    right_click_lock_target.reset();
                }

                const bool moving_character =
                    std::abs(input.forward) > 0.001f || std::abs(input.strafe) > 0.001f;
                const bool lock_on_key = !editor_ui.wants_keyboard_capture() &&
                    window.key_pressed(platform::Key::L);
                if (lock_on_key && !previous_lock_on_key && combat_camera.active()) {
                    right_click_lock_target.reset();
                    if (combat_camera.locked()) {
                        combat_camera.clear_lock_target();
                    } else {
                        game::combat::CombatEntity& camera_player_entity =
                            *combat_registry.find(player_entity_id);
                        const std::optional<game::combat::EntityId> selected =
                            combat_camera.choose_lock_target(combat_registry, targets,
                                camera_player_entity, last_game_camera_pose, collision_world);
                        if (selected) {
                            if (*selected != targets.current()) {
                                (void)targets.select(
                                    combat_registry, camera_player_entity, *selected);
                            }
                            combat_camera.set_lock_target(*selected);
                        }
                    }
                }
                previous_lock_on_key = lock_on_key;

                if (combat_camera.active()) {
                    game::combat::CombatEntity& camera_player_entity =
                        *combat_registry.find(player_entity_id);
                    if (combat_camera.locked() &&
                        targets.current() != *combat_camera.lock_target()) {
                        const game::combat::CombatEntity* selected_target =
                            combat_registry.find(targets.current());
                        if (selected_target != nullptr &&
                            game::combat::TargetSystem::is_valid_target(
                                *selected_target, camera_player_entity)) {
                            combat_camera.set_lock_target(selected_target->id);
                        } else {
                            combat_camera.clear_lock_target();
                        }
                    }
                    const game::combat::CombatEntity* locked_target =
                        combat_camera.locked()
                        ? combat_registry.find(*combat_camera.lock_target())
                        : nullptr;
                    last_game_camera_pose = combat_camera.update(
                        delta_seconds,
                        {{position[0], position[1], position[2]}, controller.yaw()},
                        camera_player_entity, locked_target,
                        {mouse_delta_x, mouse_delta_y, scroll_delta,
                            rotates_camera(camera_mode)},
                        moving_character, collision_world, terrain);
                } else {
                    (void)camera.apply_input({
                        mouse_delta_x, mouse_delta_y, scroll_delta,
                        rotates_camera(camera_mode)});
                    camera.set_follow_target_yaw(
                        !follows_character(camera_mode, moving_character,
                            manual_camera_orientation_locked)
                        ? std::nullopt
                        : std::optional<float>{controller.yaw() + 3.14159265f});
                    last_game_camera_pose = camera.update(
                        delta_seconds,
                        {{position[0], position[1], position[2]}, controller.yaw()},
                        &collision_world);
                }
                camera_pose = last_game_camera_pose;
                if (rebase_left_look_heading(
                        camera_mode, input.forward, input.strafe)) {
                    left_camera_movement_yaw = camera_pose.horizontal_yaw();
                }
                if (const std::optional<float> manual_facing =
                        manual_facing_yaw(camera_mode, camera_pose.horizontal_yaw())) {
                    controller.set_facing_target(*manual_facing);
                }
                character::CharacterControllerInput movement_input = input;
                movement_input.forward = forward_input(input.forward, camera_mode);
                controller.update(
                    delta_seconds, movement_input,
                    movement_yaw(camera_mode, camera_pose.horizontal_yaw(),
                        left_camera_movement_yaw),
                    collision_world, &terrain);
                player_entity.position = {
                    player.transform().position[0],
                    player.transform().position[1],
                    player.transform().position[2]};
                combat_context = {
                    player_entity_id, player_entity.position, controller.yaw(), true};
                combat.update();
                selection.clear();
            }
            {
                const animation::Vector3& p = player.transform().position;
                enemies.update(delta_seconds, terrain, {p[0], p[1], p[2]});
            }
            if (game_mode) {
                targets.update(combat_registry);
            }
            const bool debug_toggle = window.key_pressed(platform::Key::F3);
            if (debug_toggle && !previous_debug_toggle) {
                animation_debug = !animation_debug;
                std::cout << "[debug] Animation Layers "
                    << (animation_debug ? "on" : "off") << '\n';
            }
            previous_debug_toggle = debug_toggle;
            if (animation_debug && window.time_seconds() >= next_debug_log) {
                next_debug_log = window.time_seconds() + 0.25;
                const animation::AnimationMixer& mixer = player.model().animation().mixer();
                std::cout << "[anim] Movement: "
                    << character::to_string(controller.movement_state())
                    << " | Combat: " << game::combat::to_string(combat_system.state())
                    << " | Target: " << game::combat::to_string(targets.state(combat_registry))
                    << " | Anim: " << character::to_string(combat.state())
                    << " | Lower: " << player.model().animation().lower_clip_name()
                    << " w=" << mixer.lower_weight()
                    << " | Upper: " << player.model().animation().upper_clip_name()
                    << " w=" << mixer.upper_weight()
                    << " | Attack " << combat.attack_time() << "/" << combat.attack_duration()
                    << " | mask bones=" << mixer.mask().active_bone_count()
                    << " | layers=" << mixer.active_layer_count() << '\n';
            }
            if (game_mode) {
                camera_pose = last_game_camera_pose;
            } else {
                previous_lock_on_key = false;
                if (combat_camera.active()) {
                    combat_camera.leave(camera);
                }
                editor::EditorCameraInput editor_input;
                if (!editor_ui.wants_keyboard_capture() &&
                    !editor_ui.wants_mouse_capture()) {
                    editor_input.forward =
                    (window.key_pressed(platform::Key::W) ? 1.0f : 0.0f) -
                    (window.key_pressed(platform::Key::S) ? 1.0f : 0.0f);
                    editor_input.right =
                    (window.key_pressed(platform::Key::D) ? 1.0f : 0.0f) -
                    (window.key_pressed(platform::Key::A) ? 1.0f : 0.0f);
                    editor_input.up =
                    (window.key_pressed(platform::Key::E) ? 1.0f : 0.0f) -
                    (window.key_pressed(platform::Key::Q) ? 1.0f : 0.0f);
                }
                editor_input.mouse_delta_x = mouse_delta_x;
                editor_input.mouse_delta_y = mouse_delta_y;
                editor_input.scroll_delta = terrain_editor_active ? 0.0 : scroll_delta;
                editor_input.look = right_dragging;
                editor_input.fast = window.key_pressed(platform::Key::LeftShift);
                editor_input.slow = window.key_pressed(platform::Key::LeftControl);
                editor_camera.update(delta_seconds, editor_input);
                camera_pose = editor_camera.pose();
            }
            previous_left_camera_dragging = left_camera_dragging;
            const bool left_brush = terrain_editor_active &&
                window.left_mouse_pressed() && !editor_ui.wants_mouse_capture();
            const bool selection_click =
                world_editor.is_active() && !terrain_editor_active &&
                window.left_mouse_pressed() && !editor_ui.wants_mouse_capture();
            if (selection_click && !previous_selection_click) {
                double cursor_x = 0.0;
                double cursor_y = 0.0;
                int window_width = 0;
                int window_height = 0;
                window.cursor_position(cursor_x, cursor_y);
                window.window_size(window_width, window_height);
                const glm::vec3 direction = cursor_ray(
                    camera_pose, cursor_x, cursor_y, window_width, window_height);
                const auto hit = selection.select_ray(
                    {camera_pose.eye_x, camera_pose.eye_y, camera_pose.eye_z},
                    direction, 1000.0f);
                if (hit) {
                    std::cout << "[selection] selected object " << hit->id
                        << " at distance " << hit->distance << '\n';
                } else {
                    std::cout << "[selection] cleared\n";
                }
            }
            const bool focus_pressed = world_editor.is_active() &&
                window.key_pressed(platform::Key::F);
            if (focus_pressed && !previous_focus_pressed && selection.selected()) {
                const auto bounds = selection.bounds_for(*selection.selected());
                if (bounds) {
                    editor_camera.focus_on((bounds->minimum + bounds->maximum) * 0.5f);
                    std::cout << "[editor] focused object "
                        << *selection.selected() << '\n';
                }
            }
            previous_focus_pressed = focus_pressed;
            if (selection_click && previous_selection_click && !right_dragging &&
                selection.selected() &&
                (mouse_delta_x != 0.0 || mouse_delta_y != 0.0)) {
                glm::vec3 translation{0.0f};
                glm::vec3 rotation{0.0f};
                glm::vec3 scale_delta{0.0f};
                const float horizontal_delta = static_cast<float>(mouse_delta_x);
                const float vertical_delta = static_cast<float>(mouse_delta_y);
                const glm::vec3 camera_forward = glm::normalize(
                    glm::vec3{camera_pose.focus_x - camera_pose.eye_x,
                        camera_pose.focus_y - camera_pose.eye_y,
                        camera_pose.focus_z - camera_pose.eye_z});
                const glm::vec3 camera_right = glm::normalize(
                    glm::cross(camera_forward, glm::vec3{0.0f, 1.0f, 0.0f}));
                const glm::vec3 camera_up = glm::normalize(
                    glm::cross(camera_right, camera_forward));
                if (selection.transform_mode() == editor::TransformMode::translate) {
                    translation = (camera_right * horizontal_delta -
                        camera_up * vertical_delta) * 0.02f;
                } else if (selection.transform_mode() == editor::TransformMode::rotate) {
                    const float degrees = horizontal_delta * 0.5f;
                    rotation = {degrees, degrees, degrees};
                } else {
                    const glm::vec3 axis = selection.transform_axis_direction();
                    const float axis_screen_x = glm::dot(axis, camera_right);
                    const float axis_screen_y = glm::dot(axis, camera_up);
                    const float projected_length_squared =
                        axis_screen_x * axis_screen_x + axis_screen_y * axis_screen_y;
                    const float projected_delta = projected_length_squared > 0.0001f
                        ? (horizontal_delta * axis_screen_x -
                            vertical_delta * axis_screen_y) / projected_length_squared
                        : horizontal_delta;
                    const float amount = projected_delta * 0.01f;
                    if (selection.transform_axis() == editor::TransformAxis::uniform) {
                        scale_delta = glm::vec3{amount};
                    } else {
                        scale_delta[static_cast<int>(selection.transform_axis())] = amount;
                    }
                }
                selection.transform_selected(translation, rotation, scale_delta);
                world_changed_this_frame = true;
                history_key = "object-transform";
                rebuild_editor_collision();
            }
            previous_selection_click = selection_click;
            if (terrain_editor_active) {
                double cursor_x = 0.0;
                double cursor_y = 0.0;
                int window_width = 0;
                int window_height = 0;
                window.cursor_position(cursor_x, cursor_y);
                window.window_size(window_width, window_height);
                const glm::vec3 direction = cursor_ray(
                    camera_pose, cursor_x, cursor_y, window_width, window_height);
                glm::vec3 hit;
                brush_cursor_valid = terrain.intersect_ray(
                    {camera_pose.eye_x, camera_pose.eye_y, camera_pose.eye_z},
                    direction, 500.0f, hit);
                if (brush_cursor_valid) {
                    brush_center = {hit.x, hit.z};
                    if (left_brush) {
                        if (!previous_brush ||
                            (selected_brush == world::TerrainBrush::flatten &&
                                !previous_flattening)) {
                            flatten_height = terrain.height_at(brush_center);
                        }
                        terrain.preload_regions_near(
                            brush_center,
                            static_cast<int>(std::ceil(
                                brush_radius / world::Terrain::kChunkCells)) + 1);
                        frame_before = editor::capture_snapshot(terrain, selection);
                        const bool changed = terrain.apply_brush(
                            brush_center, brush_radius, brush_strength, delta_seconds,
                            selected_brush, flatten_height, selected_material);
                        if (changed) {
                            renderer.set_terrain(terrain, {camera_pose.focus_x, camera_pose.focus_z});
                            world_changed_this_frame = true;
                            history_key = "terrain-brush";
                        }
                    }
                }
                renderer.set_brush_cursor(
                    terrain, brush_center, brush_radius, brush_cursor_valid);
            } else {
                brush_cursor_valid = false;
                renderer.set_brush_cursor(terrain, brush_center, brush_radius, false);
            }
            previous_brush = left_brush && brush_cursor_valid;
            previous_flattening =
                selected_brush == world::TerrainBrush::flatten;

            const editor::EditorUIActions ui_actions = editor_ui.draw(
                world_editor.is_active(), selection, asset_database, editor_history,
                terrain_editor_active, selected_brush, selected_material, brush_radius,
                brush_strength);
            if (ui_actions.world_changed) {
                world_changed_this_frame = true;
                if (history_key.empty()) {
                    history_key = "inspector";
                }
                rebuild_editor_collision();
            }
            if (ui_actions.request_save) {
                try {
                    save_world();
                } catch (const std::exception& error) {
                    const std::string message =
                        "Não foi possível salvar o mundo: " +
                        std::string(error.what());
                    std::cerr << "[world] " << message << '\n';
                    editor_ui.set_status(message, true);
                }
            }
            if (ui_actions.request_load) {
                try {
                    load_world();
                    frame_before = editor::capture_snapshot(terrain, selection);
                    world_changed_this_frame = false;
                } catch (const std::exception& error) {
                    const std::string message =
                        "Não foi possível carregar o mundo: " +
                        std::string(error.what());
                    std::cerr << "[world] " << message << '\n';
                    editor_ui.set_status(message, true);
                }
            }
            if (undo_requested || ui_actions.request_undo) {
                if (editor_history.undo(terrain, selection)) {
                    renderer.set_terrain(terrain, {camera_pose.focus_x, camera_pose.focus_z});
                    rebuild_editor_collision();
                }
                world_changed_this_frame = false;
                editor_history.break_coalescing();
            } else if (redo_requested || ui_actions.request_redo) {
                if (editor_history.redo(terrain, selection)) {
                    renderer.set_terrain(terrain, {camera_pose.focus_x, camera_pose.focus_z});
                    rebuild_editor_collision();
                }
                world_changed_this_frame = false;
                editor_history.break_coalescing();
            }
            if (ui_actions.asset_drop) {
                try {
                    const std::filesystem::path model_path =
                        asset_database.resolve_model_path(
                            ui_actions.asset_drop->asset_path);
                    const auto asset_entry = std::find_if(
                        asset_database.entries().begin(),
                        asset_database.entries().end(),
                        [&ui_actions](const editor::AssetEntry& entry) {
                            return entry.relative_path ==
                                ui_actions.asset_drop->asset_path;
                        });
                    if (asset_entry == asset_database.entries().end()) {
                        throw std::runtime_error(
                            "dropped asset disappeared from the asset database");
                    }
                    assets::Model model = assets.load_model(model_path);
                    int window_width = 0;
                    int window_height = 0;
                    window.window_size(window_width, window_height);
                    const glm::vec2 cursor = ui_actions.asset_drop->cursor_position;
                    const glm::vec3 direction = cursor_ray(
                        camera_pose, cursor.x, cursor.y, window_width, window_height);
                    glm::vec3 hit;
                    if (!terrain.intersect_ray(
                            {camera_pose.eye_x, camera_pose.eye_y, camera_pose.eye_z},
                            direction, 500.0f, hit)) {
                        throw std::runtime_error(
                            "Não foi possível posicionar o ativo: o cursor não atingiu o terreno");
                    }
                    const std::string model_key =
                        asset_entry->model_path.generic_string();
                    const renderer::ModelBounds bounds =
                        renderer.register_editor_model(model_key, model);
                    const glm::vec3 center{
                        hit.x,
                        hit.y + (bounds.maximum.y - bounds.minimum.y) * 0.5f,
                        hit.z,
                    };
                    const std::string object_name =
                        ui_actions.asset_drop->asset_path.stem().string();
                    (void)selection.add_object({
                        0,
                        {bounds.minimum, bounds.maximum},
                        center,
                        {},
                        glm::vec3{1.0f},
                        object_name.empty() ? "Asset" : object_name,
                        model_key,
                    });
                    world_changed_this_frame = true;
                    history_key = "asset-drop";
                    rebuild_editor_collision();
                    editor_ui.set_status(
                        "Added " + ui_actions.asset_drop->asset_path.generic_string(),
                        false);
                } catch (const std::exception& error) {
                    const std::string message =
                        "Não foi possível adicionar o ativo: " +
                        std::string(error.what());
                    std::cerr << "[assets] " << message << '\n';
                    editor_ui.set_status(message, true);
                }
            }
            if (world_changed_this_frame) {
                editor_history.record(
                    std::move(frame_before),
                    editor::capture_snapshot(terrain, selection),
                    history_key);
            } else if (!left_brush && !selection_click) {
                editor_history.break_coalescing();
            }

            if (game_mode) {
                const ui::InventoryActions inventory_actions =
                    ui::draw_inventory_window(items, &inventory_open);
                std::string reason;
                if (inventory_actions.equip_index &&
                    !items.equip(*inventory_actions.equip_index, &reason)) {
                    std::cout << "[items] " << reason << '\n';
                }
                if (inventory_actions.unequip_slot &&
                    !items.unequip(*inventory_actions.unequip_slot, &reason)) {
                    std::cout << "[items] " << reason << '\n';
                }
            }
            items.apply(equipment, combat_system);

            const auto& player_pose = player.model().animation().pose();
            const std::vector<renderer::RigidAttachment> player_attachments =
                equipment_presenter.attachments(equipment, player_pose);
            std::vector<renderer::SkinnedInstance> skinned_instances;
            skinned_instances.push_back({
                {position[0], position[1], position[2], controller.yaw()},
                glm::vec3{1.0f},
                player_pose.skin_matrices,
                player_attachments,
            });
            enemies.append_instances(skinned_instances);
            renderer.set_skinned_instances(skinned_instances);

            {
                const game::combat::CombatEntity* target = combat_registry.find(targets.current());
                if (game_mode && target != nullptr) {
                    renderer.set_target_indicator(true, target->position, 0.9f,
                        target->alive() ? glm::vec3{1.0f, 0.2f, 0.15f} : glm::vec3{0.5f});
                    const std::string auto_attack_status = selected_weapon_animation
                        ? (combat_system.auto_attack_enabled() ? "ON" : "OFF (press 1)")
                        : "OFF (no bow animation)";
                    std::string status = "Weapon: " + std::string(selected_weapon->name) +
                        " (4-7) | Auto Attack: " + auto_attack_status;
                    status += " | 2 Basic | 3 Power (" +
                        std::to_string(static_cast<int>(active_abilities->resource())) + " " +
                        game::combat::to_string(active_abilities->resource_type()) + ')';
                    if (combat_system.auto_attack_enabled() &&
                        combat_system.last_block() != game::combat::AttackBlock::None) {
                        status += std::string(" - ") + game::combat::to_string(combat_system.last_block());
                    }
                    ui::draw_target_frame({target->name, target->level, target->hp, target->max_hp,
                        status, !target->alive()});
                } else {
                    renderer.set_target_indicator(false, {}, 1.0f, {});
                }
            }
            int framebuffer_width = 0;
            int framebuffer_height = 0;
            window.framebuffer_size(framebuffer_width, framebuffer_height);
            const bool editor_mode = world_editor.is_active();
            std::vector<renderer::RenderBox> render_boxes;
            render_boxes.reserve(selection.objects().size());
            for (const editor::SelectableObject& object : selection.objects()) {
                render_boxes.push_back({
                    object.id,
                    object.position,
                    (object.local_bounds.maximum - object.local_bounds.minimum) * 0.5f,
                    object.rotation_degrees,
                    object.scale,
                    object.asset_path,
                });
            }
            renderer::GizmoMode gizmo_mode = renderer::GizmoMode::translate;
            if (selection.transform_mode() == editor::TransformMode::rotate) {
                gizmo_mode = renderer::GizmoMode::rotate;
            } else if (selection.transform_mode() == editor::TransformMode::scale) {
                gizmo_mode = renderer::GizmoMode::scale;
            }
            const auto selected_object = selection.selected();
            const auto selected_bounds = selected_object
                ? selection.bounds_for(*selected_object)
                : std::nullopt;
            renderer.set_terrain(terrain, {camera_pose.focus_x, camera_pose.focus_z});
            renderer.render(framebuffer_width, framebuffer_height,
                camera_pose, collision_world,
                editor_mode ? selected_object : std::nullopt,
                render_boxes,
                editor_mode && selected_object.has_value() && !terrain_editor_active,
                selected_bounds
                    ? (selected_bounds->minimum + selected_bounds->maximum) * 0.5f
                    : glm::vec3{0.0f},
                gizmo_mode,
                selection.transform_space() == editor::TransformSpace::local);
            editor_ui.render();
            window.swap_buffers();
        }

        std::cout << "[app] shutdown complete.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "[app] aborted: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

}
