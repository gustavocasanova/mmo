#include "apps/client/application.hpp"

#include "scene/camera_controller.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"
#include "game/client/demo_scene.hpp"
#include "game/characters/character.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace mmo::app {

int run_application()
{
    try {
        platform::GlfwRuntime glfw;
        platform::Window window(1280, 720, "MMO Engine - Character Equipment Test");
        assets::MeshCatalog assets;
        game::client::DemoScene scene(assets);
        renderer::Renderer renderer(platform::Window::get_proc_address, assets);
        scene::CameraController camera_controller;
        scene::Camera camera;
        game::characters::Character player;
        const game::world::MovementBounds movement_bounds;

        std::cout << "[app] Character Equipment Test running. WASD move; Escape exits.\n"
            << "[demo] 1 helmet, 2 shoulders, 3 chest, 4 gloves, 5 pants, 6 boots,\n"
            << "       7 cloak, 8 bracers, 9 rings/earrings, T palette, 0 reset.\n";

        game::client::CharacterEquipment demo_equipment = game::client::make_default_character_equipment();
        constexpr std::array<platform::Key, 11> demo_keys{
            platform::Key::Digit1, platform::Key::Digit2, platform::Key::Digit3,
            platform::Key::Digit4, platform::Key::Digit5, platform::Key::Digit6,
            platform::Key::Digit7, platform::Key::Digit8, platform::Key::Digit9,
            platform::Key::Digit0, platform::Key::T,
        };
        std::array<bool, demo_keys.size()> was_pressed{};
        std::size_t palette = 0;

        double previous_time = window.time_seconds();

        while (!window.should_close()) {
            window.poll_events();
            if (window.escape_pressed()) {
                window.request_close();
            }

            for (std::size_t key_index = 0; key_index < demo_keys.size(); ++key_index) {
                const bool pressed = window.key_pressed(demo_keys[key_index]);
                const bool just_pressed = pressed && !was_pressed[key_index];
                was_pressed[key_index] = pressed;
                if (!just_pressed) {
                    continue;
                }

                if (key_index == 9) {
                    demo_equipment = game::client::make_default_character_equipment();
                    palette = 0;
                    std::cout << "[demo] default equipment restored\n";
                } else if (key_index == 10) {
                    constexpr std::array<std::array<float, 3>, 4> palettes{{
                        {{0.72f, 0.25f, 0.16f}},
                        {{0.18f, 0.48f, 0.62f}},
                        {{0.62f, 0.48f, 0.16f}},
                        {{0.36f, 0.58f, 0.30f}},
                    }};
                    palette = (palette + 1) % palettes.size();
                    for (std::size_t item_index = 0;
                         item_index < demo_equipment.items.size(); ++item_index) {
                        game::client::EquipmentItem& item = demo_equipment.items[item_index];
                        if (item.equipped) {
                            item.color = palettes[(palette + item_index) % palettes.size()];
                        }
                    }
                    std::cout << "[demo] equipment palette " << palette + 1 << '\n';
                } else {
                    constexpr std::array<const char*, 9> labels{
                        "helmet", "shoulders", "chest", "gloves", "pants",
                        "boots", "cloak", "bracers", "rings and earrings",
                    };
                    const auto toggle = [&demo_equipment](game::client::EquipmentSlot slot) {
                        demo_equipment[slot].equipped = !demo_equipment[slot].equipped;
                    };
                    switch (key_index) {
                    case 0: toggle(game::client::EquipmentSlot::Helmet); break;
                    case 1:
                        toggle(game::client::EquipmentSlot::ShoulderLeft);
                        toggle(game::client::EquipmentSlot::ShoulderRight);
                        break;
                    case 2: toggle(game::client::EquipmentSlot::Chest); break;
                    case 3:
                        toggle(game::client::EquipmentSlot::GloveLeft);
                        toggle(game::client::EquipmentSlot::GloveRight);
                        break;
                    case 4: toggle(game::client::EquipmentSlot::Pants); break;
                    case 5:
                        toggle(game::client::EquipmentSlot::BootLeft);
                        toggle(game::client::EquipmentSlot::BootRight);
                        break;
                    case 6: toggle(game::client::EquipmentSlot::Cape); break;
                    case 7:
                        toggle(game::client::EquipmentSlot::BracerLeft);
                        toggle(game::client::EquipmentSlot::BracerRight);
                        break;
                    case 8:
                        for (std::size_t slot = static_cast<std::size_t>(game::client::EquipmentSlot::RingLeftIndex);
                             slot < static_cast<std::size_t>(game::client::EquipmentSlot::Count); ++slot) {
                            toggle(static_cast<game::client::EquipmentSlot>(slot));
                        }
                        break;
                    default: break;
                    }
                    std::cout << "[demo] toggled " << labels[key_index] << '\n';
                }
                const auto mesh = scene.set_equipment(assets, demo_equipment);
                renderer.update_mesh(mesh, assets.get(mesh));
            }

            const bool right_dragging = window.right_mouse_pressed();
            const bool left_dragging = window.left_mouse_pressed();
            const bool camera_dragging = right_dragging || left_dragging;
            window.set_cursor_captured(camera_dragging);

            double mouse_delta_x = 0.0;
            double mouse_delta_y = 0.0;
            double scroll_delta = 0.0;
            window.consume_mouse_input(mouse_delta_x, mouse_delta_y, scroll_delta);
            const float yaw_delta = camera_controller.apply_input(
                {mouse_delta_x, mouse_delta_y, scroll_delta, camera_dragging});
            if (right_dragging) player.rotate(yaw_delta);

            const double current_time = window.time_seconds();
            const float delta_seconds = std::clamp(
                static_cast<float>(current_time - previous_time), 0.0f, 0.1f);
            previous_time = current_time;

            float forward_input = 0.0f;
            float strafe_input = 0.0f;
            if (window.key_pressed(platform::Key::W)) forward_input += 1.0f;
            if (window.key_pressed(platform::Key::S)) forward_input -= 1.0f;
            if (window.key_pressed(platform::Key::A)) strafe_input -= 1.0f;
            if (window.key_pressed(platform::Key::D)) strafe_input += 1.0f;

            player.update(delta_seconds, {forward_input, strafe_input}, movement_bounds);

            int framebuffer_width = 0;
            int framebuffer_height = 0;
            window.framebuffer_size(framebuffer_width, framebuffer_height);
            const auto& position = player.position();
            camera.set_pose(camera_controller.update(
                delta_seconds, position.x, position.y, position.z, player.yaw(),
                forward_input > 0.0f && !camera_dragging));
            const auto draws = scene.update(delta_seconds, player);
            renderer.render(framebuffer_width, framebuffer_height, camera, draws);
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
