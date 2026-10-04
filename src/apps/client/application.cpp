#include "core/application.hpp"

#include "assets/gltf_model_loader.hpp"
#include "character/character.hpp"
#include "core/camera_controller.hpp"
#include "platform/window.hpp"
#include "renderer/renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace mmo::core {
namespace {

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

void bind_animations(character::Character& character)
{
    animation::AnimationController& controller = character.model().animation();
    const auto& clips = character.model().body().model().animations;
    bool idle_bound = false;
    bool walk_bound = false;
    std::optional<std::size_t> run_clip;
    for (std::size_t index = 0; index < clips.size(); ++index) {
        const std::string name = lowercase(clips[index].name);
        if (!idle_bound && name.find("idle") != std::string::npos) {
            idle_bound = controller.bind_state(animation::AnimationState::Idle, index);
        }
        if (!walk_bound && name.find("walk") != std::string::npos) {
            walk_bound = controller.bind_state(animation::AnimationState::Walk, index);
        }
        if (!run_clip && name.find("run") != std::string::npos) {
            run_clip = index;
            (void)controller.bind_state(animation::AnimationState::Run, index);
        }
    }
    if (!walk_bound && run_clip) {
        walk_bound = controller.bind_state(animation::AnimationState::Walk, *run_clip);
    }
    (void)controller.set_state(animation::AnimationState::Idle);
}

}

int run_application()
{
    try {
        platform::GlfwRuntime glfw;
        platform::Window window(1280, 720, "MMO Engine - Character Preview");
        renderer::Renderer renderer;
        core::CameraController camera;

        assets::GltfModelLoader model_loader;
        assets::AssetSystem assets(model_loader);
        auto body_model = std::make_shared<const assets::Model>(
            assets.load_model("personagem/characterRIGGED.glb"));
        character::Character player(body_model);
        bind_animations(player);
        renderer.set_character_model(player.model().body().model());
        renderer.set_skinning_matrices(player.model().animation().pose().skin_matrices);

        std::cout << "[app] Character preview running. WASD moves; right mouse orbits and turns; "
            << "scroll zooms; Escape exits.\n"
            << "[model] loaded " << body_model->meshes.size() << " meshes, "
            << body_model->skeleton.bones.size() << " bones and "
            << body_model->animations.size() << " animation clips.\n";
        if (body_model->animations.empty()) {
            std::cout << "[model] no animation clips are embedded; the character will stay "
                << "in its bind pose.\n";
        } else if (!player.model().animation().animator().playing()) {
            std::cout << "[model] no Idle clip is bound; a movement clip will start when "
                << "the character moves.\n";
        }

        float player_x = 0.0f;
        float player_z = 0.0f;
        float player_yaw = 0.0f;
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
                (void)player.model().animation().set_state(animation::AnimationState::Walk);
            } else {
                animation::AnimationController& animation = player.model().animation();
                if (!animation.set_state(animation::AnimationState::Idle)) {
                    animation.stop();
                }
            }
            player.model().update(delta_seconds);
            renderer.set_skinning_matrices(player.model().animation().pose().skin_matrices);

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
