#include "core/application.hpp"

#include "core/camera_controller.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace mmo::core {

int run_application()
{
    try {
        platform::GlfwRuntime glfw;
        platform::Window window(1280, 720, "MMO Engine - Marco 003");
        renderer::Renderer renderer;
        core::CameraController camera;

        std::cout << "[app] Marco 003 running. Use WASD to move; press Escape to exit.\n";

        float player_x = 0.0f;
        float player_z = 0.0f;
        float player_yaw = 0.0f;
        float walk_phase = 0.0f;
        double previous_time = window.time_seconds();

        while (!window.should_close()) {
            window.poll_events();
            if (window.escape_pressed()) {
                window.request_close();
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
                walk_phase += delta_seconds * 9.0f;
            } else {
                walk_phase = 0.0f;
            }

            int framebuffer_width = 0;
            int framebuffer_height = 0;
            window.framebuffer_size(framebuffer_width, framebuffer_height);
            const core::CameraPose camera_pose = camera.update(
                delta_seconds, player_x, 0.0f, player_z, player_yaw,
                forward_input > 0.0f && !camera_dragging);
            renderer.render(framebuffer_width, framebuffer_height,
                player_x, player_z, player_yaw, walk_phase,
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