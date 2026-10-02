#include "core/application.hpp"

#include "core/camera_controller.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace mmo::core {

int run_application()
{
    try {
        platform::GlfwRuntime glfw;
        platform::Window window(1280, 720, "MMO Engine - Character Equipment Test");
        renderer::Renderer renderer;
        core::CameraController camera;

        std::cout << "[app] Character Equipment Test running. WASD move; Escape exits.\n"
            << "[demo] 1 helmet, 2 shoulders, 3 chest, 4 gloves, 5 pants, 6 boots,\n"
            << "       7 cloak, 8 bracers, 9 rings/earrings, T palette, 0 reset.\n";

        renderer::CharacterEquipment demo_equipment = renderer::make_default_character_equipment();
        constexpr std::array<platform::Key, 11> demo_keys{
            platform::Key::Digit1, platform::Key::Digit2, platform::Key::Digit3,
            platform::Key::Digit4, platform::Key::Digit5, platform::Key::Digit6,
            platform::Key::Digit7, platform::Key::Digit8, platform::Key::Digit9,
            platform::Key::Digit0, platform::Key::T,
        };
        std::array<bool, demo_keys.size()> was_pressed{};
        std::size_t palette = 0;

        float player_x = 0.0f;
        float player_z = 0.0f;
        float player_yaw = 0.0f;
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
                    demo_equipment = renderer::make_default_character_equipment();
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
                        renderer::EquipmentItem& item = demo_equipment.items[item_index];
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
                    const auto toggle = [&demo_equipment](renderer::EquipmentSlot slot) {
                        demo_equipment[slot].equipped = !demo_equipment[slot].equipped;
                    };
                    switch (key_index) {
                    case 0: toggle(renderer::EquipmentSlot::Helmet); break;
                    case 1:
                        toggle(renderer::EquipmentSlot::ShoulderLeft);
                        toggle(renderer::EquipmentSlot::ShoulderRight);
                        break;
                    case 2: toggle(renderer::EquipmentSlot::Chest); break;
                    case 3:
                        toggle(renderer::EquipmentSlot::GloveLeft);
                        toggle(renderer::EquipmentSlot::GloveRight);
                        break;
                    case 4: toggle(renderer::EquipmentSlot::Pants); break;
                    case 5:
                        toggle(renderer::EquipmentSlot::BootLeft);
                        toggle(renderer::EquipmentSlot::BootRight);
                        break;
                    case 6: toggle(renderer::EquipmentSlot::Cape); break;
                    case 7:
                        toggle(renderer::EquipmentSlot::BracerLeft);
                        toggle(renderer::EquipmentSlot::BracerRight);
                        break;
                    case 8:
                        for (std::size_t slot = static_cast<std::size_t>(renderer::EquipmentSlot::RingLeftIndex);
                             slot < static_cast<std::size_t>(renderer::EquipmentSlot::Count); ++slot) {
                            toggle(static_cast<renderer::EquipmentSlot>(slot));
                        }
                        break;
                    default: break;
                    }
                    std::cout << "[demo] toggled " << labels[key_index] << '\n';
                }
                renderer.set_character_equipment(demo_equipment);
            }

            const bool right_dragging = window.right_mouse_pressed();
            const bool left_dragging = window.left_mouse_pressed();
            const bool camera_dragging = right_dragging || left_dragging;
            window.set_cursor_captured(camera_dragging);

            double mouse_delta_x = 0.0;
            double mouse_delta_y = 0.0;
            double scroll_delta = 0.0;
            window.consume_mouse_input(mouse_delta_x, mouse_delta_y, scroll_delta);
            camera.apply_input({mouse_delta_x, mouse_delta_y, scroll_delta,
                camera_dragging, right_dragging}, player_yaw);

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

            float move_x = std::sin(player_yaw) * forward_input +
                std::cos(player_yaw) * strafe_input;
            float move_z = std::cos(player_yaw) * forward_input -
                std::sin(player_yaw) * strafe_input;

            const float move_length = std::sqrt(move_x * move_x + move_z * move_z);
            if (move_length > 0.0f) {
                move_x /= move_length;
                move_z /= move_length;
                constexpr float kMoveSpeed = 4.2f;
                player_x = std::clamp(player_x + move_x * kMoveSpeed * delta_seconds,
                    -36.0f, 36.0f);
                player_z = std::clamp(player_z + move_z * kMoveSpeed * delta_seconds,
                    -36.0f, 36.0f);
            }

            int framebuffer_width = 0;
            int framebuffer_height = 0;
            window.framebuffer_size(framebuffer_width, framebuffer_height);
            const core::CameraPose camera_pose = camera.update(
                delta_seconds, player_x, 0.0f, player_z, player_yaw,
                forward_input > 0.0f && !camera_dragging);
            renderer.render(framebuffer_width, framebuffer_height,
                player_x, player_z, player_yaw,
                {camera_pose.eye_x, camera_pose.eye_y, camera_pose.eye_z,
                    camera_pose.focus_x, camera_pose.focus_y, camera_pose.focus_z});
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