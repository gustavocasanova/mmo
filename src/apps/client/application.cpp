#include "apps/client/application.hpp"

#include "assets/gltf_model_loader.hpp"
#include "character/character.hpp"
#include "character/character_controller.hpp"
#include "editor/editor_camera.hpp"
#include "editor/selection.hpp"
#include "editor/world_editor.hpp"
#include "game/world/collision_world.hpp"
#include "game/world/terrain.hpp"
#include "platform/input_settings.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"
#include "scene/camera_controller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

namespace mmo::app {
namespace {

constexpr char kTerrainPath[] = "content/worlds/demo.mmoterrain";

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

void reverse_keyframes(animation::Vector3Keyframes& keyframes)
{
    const float last_time = keyframes.times.empty() ? 0.0f : keyframes.times.back();
    std::reverse(keyframes.times.begin(), keyframes.times.end());
    std::reverse(keyframes.values.begin(), keyframes.values.end());
    for (float& time : keyframes.times) {
        time = last_time - time;
    }
}

void reverse_keyframes(animation::QuaternionKeyframes& keyframes)
{
    const float last_time = keyframes.times.empty() ? 0.0f : keyframes.times.back();
    std::reverse(keyframes.times.begin(), keyframes.times.end());
    std::reverse(keyframes.values.begin(), keyframes.values.end());
    for (float& time : keyframes.times) {
        time = last_time - time;
    }
}

std::size_t add_backward_walk_clip(assets::Model& model)
{
    const auto existing = std::find_if(model.animations.begin(), model.animations.end(),
        [](const animation::AnimationClip& clip) {
            return clip.name == "Walk_Backward_Loop";
        });
    if (existing != model.animations.end()) {
        return static_cast<std::size_t>(std::distance(model.animations.begin(), existing));
    }
    const auto forward_walk = std::find_if(model.animations.begin(), model.animations.end(),
        [](const animation::AnimationClip& clip) { return clip.name == "Walk_Loop"; });
    if (forward_walk == model.animations.end()) {
        throw std::runtime_error("Universal Animation Library is missing Walk_Loop");
    }
    animation::AnimationClip backward_walk = *forward_walk;
    backward_walk.name = "Walk_Backward_Loop";
    for (animation::AnimationChannel& channel : backward_walk.channels) {
        reverse_keyframes(channel.translations);
        reverse_keyframes(channel.rotations);
        reverse_keyframes(channel.scales);
    }
    if (!backward_walk.is_valid(model.skeleton.bones.size())) {
        throw std::runtime_error("Could not construct a valid backward-walking clip");
    }
    model.animations.push_back(std::move(backward_walk));
    return model.animations.size() - 1;
}

std::size_t find_clip_index(
    const std::vector<animation::AnimationClip>& clips,
    std::string_view name)
{
    const auto clip = std::find_if(clips.begin(), clips.end(),
        [name](const animation::AnimationClip& candidate) {
            return candidate.name == name;
        });
    if (clip == clips.end()) {
        throw std::runtime_error(
            "Universal Animation Library is missing required clip: " + std::string(name));
    }
    return static_cast<std::size_t>(std::distance(clips.begin(), clip));
}

void bind_animations(character::Character& character)
{
    animation::AnimationController& controller = character.model().animation();
    const auto& clips = character.model().body().model().animations;
    const bool bound =
        controller.bind_state(animation::AnimationState::Idle,
            find_clip_index(clips, "Idle_Loop")) &&
        controller.bind_state(animation::AnimationState::Walk,
            find_clip_index(clips, "Walk_Loop")) &&
        controller.bind_state(animation::AnimationState::Run,
            find_clip_index(clips, "Jog_Fwd_Loop")) &&
        controller.bind_state(animation::AnimationState::Jump,
            find_clip_index(clips, "Jump_Loop"));
    (void)find_clip_index(clips, "Sprint_Loop");
    (void)find_clip_index(clips, "Jump_Start");
    (void)find_clip_index(clips, "Jump_Land");
    if (!bound || !controller.set_state(animation::AnimationState::Idle, 0.0f)) {
        throw std::runtime_error("Could not bind required locomotion animations");
    }
}

game::world::CollisionWorld make_demo_collision_world()
{
    using game::world::CollisionBox;
    return game::world::CollisionWorld(
        {},
        {
            CollisionBox{{4.0f, 0.0f, -2.5f}, {4.6f, 3.0f, 2.5f}},
            CollisionBox{{-5.0f, 0.0f, -2.0f}, {-4.6f, 3.0f, 2.0f}},
        });
}

std::vector<editor::SelectableObject> make_demo_selectables(
    const game::world::CollisionWorld& collision_world)
{
    std::vector<editor::SelectableObject> objects;
    objects.reserve(collision_world.boxes().size());
    for (std::size_t index = 0; index < collision_world.boxes().size(); ++index) {
        const game::world::CollisionBox& box = collision_world.boxes()[index];
        objects.push_back({
            static_cast<editor::SelectionId>(index + 1),
            {box.minimum, box.maximum},
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
        scene::CameraController camera;
        editor::WorldEditor world_editor;
        editor::EditorCamera editor_camera;
        game::world::Terrain terrain;
        const std::filesystem::path terrain_path{kTerrainPath};
        if (std::filesystem::exists(terrain_path)) {
            terrain.load(terrain_path);
            std::cout << "[terrain] loaded " << terrain_path.string() << '\n';
        }
        game::world::CollisionWorld collision_world = make_demo_collision_world();
        editor::SelectionManager selection(make_demo_selectables(collision_world));
        const platform::InputSettings input_settings{};

        assets::GltfModelLoader model_loader;
        assets::AssetSystem assets(model_loader);
        assets::Model loaded_model = assets.load_model(
            "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb");
        const std::size_t backward_walk_index = add_backward_walk_clip(loaded_model);
        auto body_model = std::make_shared<const assets::Model>(std::move(loaded_model));
        character::Character player(body_model);
        bind_animations(player);
        if (!player.model().animation().bind_state(
                animation::AnimationState::Walk, backward_walk_index)) {
            throw std::runtime_error("Could not bind the backward-walking animation");
        }
        character::CharacterController controller(player);
        renderer.set_character_model(player.model().body().model());
        renderer.set_skinning_matrices(player.model().animation().pose().skin_matrices);
        renderer.set_terrain(terrain);
        const animation::Vector3& initial_position = player.transform().position;
        scene::CameraPose last_game_camera_pose = camera.update(
            0.0f,
            {{initial_position[0], initial_position[1], initial_position[2]},
                controller.yaw()},
            &collision_world);

        const auto& clips = body_model->animations;
        std::cout << "[app] Movement prototype running. WASD moves relative to the camera; "
            << "hold Shift to run; Space jumps; hold right mouse to orbit; "
            << "scroll zooms; [ and ] preview every animation; F1 toggles World Editor; "
            << "Escape exits.\n"
            << "[editor] F1 toggles editor; RMB + mouse looks; WASD moves; Q/E descend/ascend; "
            << "Shift speeds up; Ctrl slows down; wheel adjusts speed.\n"
            << "[terrain] in World Editor, F2 toggles terrain tools; 1 raise, 2 lower, "
            << "3 flatten; left mouse sculpts; wheel changes brush size; "
            << "Ctrl+S saves; Ctrl+L loads.\n"
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
        bool previous_transform_x = false;
        bool previous_transform_y = false;
        bool previous_transform_z = false;
        bool previous_transform_uniform = false;
        bool terrain_editor_active = false;
        bool brush_cursor_valid = false;
        float brush_radius = 3.0f;
        float flatten_height = 0.0f;
        glm::vec2 brush_center{0.0f};
        game::world::TerrainBrush selected_brush = game::world::TerrainBrush::raise;
        double previous_time = window.time_seconds();

        while (!window.should_close()) {
            window.poll_events();
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
                world_editor.is_active() && window.key_pressed(platform::Key::F2);
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
                window.key_pressed(platform::Key::LeftControl) &&
                window.key_pressed(platform::Key::S);
            if (save_pressed && !previous_save) {
                terrain.save(terrain_path);
                std::cout << "[terrain] saved " << terrain_path.string() << '\n';
            }
            previous_save = save_pressed;

            const bool load_pressed =
                world_editor.is_active() &&
                window.key_pressed(platform::Key::LeftControl) &&
                window.key_pressed(platform::Key::L);
            if (load_pressed && !previous_load) {
                if (std::filesystem::exists(terrain_path)) {
                    terrain.load(terrain_path);
                    renderer.set_terrain(terrain);
                    std::cout << "[terrain] loaded " << terrain_path.string() << '\n';
                } else {
                    std::cerr << "[terrain] no saved terrain at "
                        << terrain_path.string() << '\n';
                }
            }
            previous_load = load_pressed;

            const bool game_mode = world_editor.mode() == editor::EngineMode::game;
            const bool right_dragging =
                window.right_mouse_pressed() && (game_mode || world_editor.is_active());
            window.set_cursor_captured(right_dragging);
            double mouse_delta_x = 0.0;
            double mouse_delta_y = 0.0;
            double scroll_delta = 0.0;
            window.consume_mouse_input(mouse_delta_x, mouse_delta_y, scroll_delta);
            if (terrain_editor_active && scroll_delta != 0.0) {
                brush_radius = std::clamp(
                    brush_radius + static_cast<float>(scroll_delta) * 0.5f, 1.0f, 12.0f);
                std::cout << "[terrain] brush radius " << brush_radius << '\n';
            }
            if (terrain_editor_active) {
                const game::world::TerrainBrush previous_selection = selected_brush;
                if (window.key_pressed(platform::Key::Digit1)) {
                    selected_brush = game::world::TerrainBrush::raise;
                } else if (window.key_pressed(platform::Key::Digit2)) {
                    selected_brush = game::world::TerrainBrush::lower;
                } else if (window.key_pressed(platform::Key::Digit3)) {
                    selected_brush = game::world::TerrainBrush::flatten;
                }
                if (selected_brush != previous_selection) {
                    const char* const brush_name =
                        selected_brush == game::world::TerrainBrush::raise ? "raise" :
                        selected_brush == game::world::TerrainBrush::lower ? "lower" :
                        "flatten";
                    std::cout << "[terrain] brush " << brush_name << '\n';
                }
            }

            const double current_time = window.time_seconds();
            const float delta_seconds = std::clamp(
                static_cast<float>(current_time - previous_time), 0.0f, 0.1f);
            previous_time = current_time;

            if (!game_mode && !terrain_editor_active && !right_dragging) {
                if (window.key_pressed(platform::Key::W)) {
                    selection.set_transform_mode(editor::TransformMode::translate);
                } else if (window.key_pressed(platform::Key::E)) {
                    selection.set_transform_mode(editor::TransformMode::rotate);
                } else if (window.key_pressed(platform::Key::R)) {
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
            }

            character::CharacterControllerInput input;
            if (game_mode) {
                input.forward =
                    (window.key_pressed(input_settings.move_forward) ? 1.0f : 0.0f) -
                    (window.key_pressed(input_settings.move_backward) ? 1.0f : 0.0f);
                input.strafe =
                    (window.key_pressed(input_settings.strafe_right) ? 1.0f : 0.0f) -
                    (window.key_pressed(input_settings.strafe_left) ? 1.0f : 0.0f);
                input.run = window.key_pressed(input_settings.run);
                input.jump_pressed = window.key_pressed(input_settings.jump);
            }

            const bool clip_back = game_mode &&
                window.key_pressed(input_settings.previous_animation);
            const bool clip_forward = game_mode &&
                window.key_pressed(input_settings.next_animation);
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

            if (game_mode) {
                controller.update(
                    delta_seconds, input, camera.yaw(), collision_world, &terrain);
                selection.clear();
            }
            const animation::Vector3& position = player.transform().position;
            scene::CameraPose camera_pose;
            if (game_mode) {
                (void)camera.apply_input({
                    mouse_delta_x, mouse_delta_y, scroll_delta, right_dragging});
                last_game_camera_pose = camera.update(
                    delta_seconds,
                    {{position[0], position[1], position[2]}, controller.yaw()},
                    &collision_world);
                camera_pose = last_game_camera_pose;
            } else {
                editor::EditorCameraInput editor_input;
                if (right_dragging) {
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
            const bool left_brush = terrain_editor_active && window.left_mouse_pressed();
            const bool selection_click =
                world_editor.is_active() && !terrain_editor_active &&
                window.left_mouse_pressed();
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
                if (selection.transform_mode() == editor::TransformMode::translate) {
                    translation = {
                        horizontal_delta * 0.02f,
                        -vertical_delta * 0.02f,
                        vertical_delta * 0.02f,
                    };
                } else if (selection.transform_mode() == editor::TransformMode::rotate) {
                    const float degrees = horizontal_delta * 0.5f;
                    rotation = {degrees, degrees, degrees};
                } else {
                    const float amount = horizontal_delta * 0.01f;
                    scale_delta = {amount, -vertical_delta * 0.01f,
                        vertical_delta * 0.01f};
                }
                selection.transform_selected(translation, rotation, scale_delta);
                std::vector<game::world::CollisionBox> edited_boxes;
                edited_boxes.reserve(selection.objects().size());
                for (const editor::SelectableObject& object : selection.objects()) {
                    const editor::SelectionBounds bounds =
                        *selection.bounds_for(object.id);
                    edited_boxes.push_back({bounds.minimum, bounds.maximum});
                }
                collision_world = game::world::CollisionWorld(
                    collision_world.bounds(), std::move(edited_boxes));
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
                            (selected_brush == game::world::TerrainBrush::flatten &&
                                !previous_flattening)) {
                            flatten_height = terrain.height_at(brush_center);
                        }
                        const bool changed = terrain.apply_brush(
                            brush_center, brush_radius, 1.8f, delta_seconds,
                            selected_brush, flatten_height);
                        if (changed) {
                            renderer.set_terrain(terrain);
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
                selected_brush == game::world::TerrainBrush::flatten;
            renderer.set_skinning_matrices(
                player.model().animation().pose().skin_matrices);

            int framebuffer_width = 0;
            int framebuffer_height = 0;
            window.framebuffer_size(framebuffer_width, framebuffer_height);
            const bool editor_mode = world_editor.is_active();
            std::vector<renderer::RenderBox> render_boxes;
            if (editor_mode) {
                render_boxes.reserve(selection.objects().size());
                for (const editor::SelectableObject& object : selection.objects()) {
                    render_boxes.push_back({
                        object.id,
                        object.position,
                        (object.local_bounds.maximum - object.local_bounds.minimum) * 0.5f,
                        object.rotation_degrees,
                        object.scale,
                    });
                }
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
            renderer.render(framebuffer_width, framebuffer_height,
                {position[0], position[1], position[2], controller.yaw()},
                camera_pose, collision_world,
                editor_mode ? selected_object : std::nullopt,
                render_boxes,
                editor_mode && selected_object.has_value() && !terrain_editor_active,
                selected_bounds
                    ? (selected_bounds->minimum + selected_bounds->maximum) * 0.5f
                    : glm::vec3{0.0f},
                gizmo_mode);
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
