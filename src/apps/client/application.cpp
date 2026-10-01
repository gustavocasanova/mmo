#include "apps/client/application.hpp"

#include "scene/camera_controller.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"
#include "game/client/demo_scene.hpp"
#include "game/characters/character.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace mmo::app {

int run_application()
{
    try {
        platform::GlfwRuntime glfw;
        platform::Window window(1280, 720, "MMO Engine - Marco 003");
        assets::MeshCatalog assets;
        game::client::DemoScene scene(assets);
        renderer::Renderer renderer(platform::Window::get_proc_address, assets);
        scene::CameraController camera_controller;
        scene::Camera camera;
        game::characters::Character player;
        const game::world::MovementBounds movement_bounds;

        std::cout << "[app] Marco 003 running. Use WASD to move; press Escape to exit.\n";

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
