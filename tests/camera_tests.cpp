#include "scene/camera.hpp"
#include "scene/camera_controller.hpp"
#include "game/world/collision_world.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_orbit_and_target_independence()
{
    using namespace mmo::scene;
    CameraSettings settings;
    settings.distance_smoothing_seconds = 0.0f;
    CameraController camera(settings);
    const CameraTarget target{{0.0f, 0.0f, 0.0f}, 0.0f};
    const CameraPose initial = camera.update(0.0f, target);
    check(initial.eye_z < 0.0f, "camera should start behind its target");
    check(std::abs(initial.focus_y - 1.5f) < 0.0001f,
        "camera should use the configured target offset");
    check(camera.apply_input({20.0, 0.0, 0.0, true}) == 0.0f,
        "free camera should not rotate the target by default");

    const CameraPose rotated = camera.update(0.016f, target);
    check(rotated.eye_x != initial.eye_x || rotated.eye_z != initial.eye_z,
        "mouse input did not orbit the camera");
    const float starting_yaw = camera.yaw();
    camera.apply_input({2.0 * 3.14159265358979323846 /
        settings.horizontal_sensitivity, 0.0, 0.0, true});
    check(std::abs(std::remainder(camera.yaw() - starting_yaw,
              2.0f * 3.14159265f)) < 0.001f,
        "camera did not support a complete 360 degree orbit");

    camera.reset_behind_target(1.57079632679f);
    const CameraPose reset = camera.update(0.016f, target);
    check(reset.eye_x < 0.0f, "camera did not reset behind target yaw");
}

void test_camera_returns_behind_character_smoothly()
{
    using namespace mmo::scene;
    CameraSettings settings;
    settings.follow_rotation_smoothing_seconds = 0.5f;
    CameraController camera(settings);
    const CameraTarget target{{0.0f, 0.0f, 0.0f}, 0.0f};
    (void)camera.update(0.0f, target);
    camera.set_follow_target_yaw(std::nullopt);
    (void)camera.apply_input({180.0, 0.0, 0.0, true});
    (void)camera.update(1.0f, target);
    const float orbit_yaw = camera.yaw();
    camera.set_follow_target_yaw(target.yaw + 3.14159265f);
    (void)camera.update(0.1f, target);
    const float first_return_yaw = camera.yaw();
    check(std::abs(std::remainder(first_return_yaw - orbit_yaw,
              2.0f * 3.14159265f)) > 0.01f,
        "camera did not start returning after orbit input ended");
    check(std::abs(std::remainder(first_return_yaw - (target.yaw + 3.14159265f),
              2.0f * 3.14159265f)) <
        std::abs(std::remainder(orbit_yaw - (target.yaw + 3.14159265f),
              2.0f * 3.14159265f)),
        "camera return was not gradual");
    for (int frame = 0; frame < 100; ++frame) {
        (void)camera.update(0.05f, target);
    }
    check(std::abs(std::remainder(camera.yaw() - (target.yaw + 3.14159265f),
              2.0f * 3.14159265f)) < 0.01f,
        "camera did not return behind the character");
}

void test_pitch_zoom_and_follow()
{
    using namespace mmo::scene;
    CameraSettings settings;
    settings.distance_smoothing_seconds = 0.12f;
    check(!settings.invert_vertical,
        "third-person camera should use standard vertical mouse look by default");
    CameraController camera(settings);
    const CameraTarget target{{0.0f, 0.0f, 0.0f}, 0.0f};
    (void)camera.update(0.0f, target);
    const float initial_pitch = camera.pitch();
    camera.apply_input({0.0, -100.0, 0.0, true});
    check(camera.pitch() < initial_pitch,
        "moving the mouse upward should make the third-person camera look upward");
    camera.apply_input({0.0, 100.0, 0.0, true});
    camera.apply_input({0.0, 100000.0, -100000.0, true});
    check(camera.pitch() >= settings.minimum_pitch &&
        camera.pitch() <= settings.maximum_pitch,
        "pitch exceeded configured limits");
    check(camera.desired_distance() == settings.maximum_distance,
        "zoom exceeded maximum distance");
    check(camera.current_distance() < settings.maximum_distance,
        "zoom smoothing did not preserve the current distance");
    (void)camera.update(0.2f, target);
    check(camera.current_distance() > settings.initial_distance,
        "camera did not smoothly approach the requested zoom");

    const CameraPose before_follow = camera.update(0.0f, target);
    const CameraPose after_follow = camera.update(
        0.12f, {{10.0f, 0.0f, 0.0f}, 0.0f});
    check(after_follow.focus_x > before_follow.focus_x &&
        after_follow.focus_x < 10.0f,
        "camera target following should be smoothed");

    Camera view;
    view.set_pose(after_follow);
    const mmo::math::Mat4 view_projection =
        view.projection(16.0f / 9.0f) * view.view();
    check(std::isfinite(view_projection[0][0]),
        "camera generated an invalid view-projection matrix");
}

void test_projection_validation()
{
    using namespace mmo::scene;
    Camera camera;
    check([&] {
        try {
            (void)camera.projection(0.0f);
            return false;
        } catch (const std::invalid_argument&) {
            return true;
        }
    }(), "camera accepted a zero aspect ratio");
}

void test_camera_obstacle_collision()
{
    using namespace mmo;
    const game::world::CollisionWorld world(
        {},
        {game::world::CollisionBox{
            {-1.0f, 0.0f, -2.2f}, {1.0f, 3.0f, -1.8f}}});
    scene::CameraController camera;
    const scene::CameraTarget target{{0.0f, 0.0f, 0.0f}, 0.0f};
    (void)camera.update(0.0f, target, &world);
    check(camera.current_distance() < 3.0f,
        "camera did not move forward to avoid an obstacle");
    check(camera.current_distance() >= camera.settings().minimum_collision_distance,
        "camera moved into the player's collision space");
    check(camera.desired_distance() == camera.settings().initial_distance,
        "obstacle collision changed the player's chosen zoom");

    const float obstructed_distance = camera.current_distance();
    (void)camera.update(0.12f, target);
    check(camera.current_distance() > obstructed_distance,
        "camera did not recover distance after an obstacle disappeared");
    check(camera.current_distance() <= camera.desired_distance(),
        "camera exceeded its chosen zoom during recovery");
}

}

int main()
{
    try {
        test_orbit_and_target_independence();
        test_camera_returns_behind_character_smoothly();
        test_pitch_zoom_and_follow();
        test_projection_validation();
        test_camera_obstacle_collision();
        std::cout << "ThirdPersonCameraTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ThirdPersonCameraTest failed: " << error.what() << '\n';
        return 1;
    }
}
