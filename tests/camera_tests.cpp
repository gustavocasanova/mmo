#include "scene/camera.hpp"
#include "scene/camera_controller.hpp"
#include "apps/client/combat_camera_controller.hpp"
#include "apps/client/mouse_camera_controls.hpp"
#include "world/collision_world.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include <glm/geometric.hpp>

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

void test_camera_view_can_be_transferred_between_modes()
{
    using namespace mmo::scene;
    CameraController normal;
    (void)normal.update(0.0f, {{2.0f, 0.0f, 3.0f}, 0.4f});
    normal.apply_input({75.0, -20.0, 1.0, true});

    CameraSettings combat_settings;
    combat_settings.minimum_distance = 2.0f;
    combat_settings.maximum_distance = 9.0f;
    CameraController combat(combat_settings);
    combat.adopt_view_from(normal);
    check(std::abs(std::remainder(combat.yaw() - normal.yaw(),
              2.0f * 3.14159265f)) < 0.0001f,
        "combat camera did not inherit free-camera yaw");
    check(std::abs(combat.pitch() - normal.pitch()) < 0.0001f,
        "combat camera did not inherit free-camera pitch");
    check(std::abs(combat.desired_distance() - normal.desired_distance()) < 0.0001f,
        "combat camera did not inherit free-camera zoom");

    combat.apply_input({-35.0, 0.0, -2.0, true});
    normal.adopt_view_from(combat);
    check(std::abs(std::remainder(normal.yaw() - combat.yaw(),
              2.0f * 3.14159265f)) < 0.0001f,
        "free camera did not inherit combat-camera yaw on exit");
    check(std::abs(normal.desired_distance() - combat.desired_distance()) < 0.0001f,
        "free camera did not inherit combat-camera zoom on exit");
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

void test_camera_pose_horizontal_yaw_ignores_pitch()
{
    const mmo::scene::CameraPose pose{
        1.5f, 50.0f, 2.5980762f, 0.0f, 0.0f, 0.0f};
    check(std::abs(pose.horizontal_yaw() - 0.5235988f) < 0.0001f,
        "camera movement yaw included vertical camera pitch");
}

void test_mouse_camera_mode_controls()
{
    using namespace mmo::app;
    check(mouse_camera_mode(false, false) == MouseCameraMode::none,
        "mouse camera mode should be none when no button is held");
    check(mouse_camera_mode(true, false) == MouseCameraMode::left,
        "left mouse button state was not distinguished");
    check(mouse_camera_mode(false, true) == MouseCameraMode::right,
        "right mouse button state was not distinguished");
    check(mouse_camera_mode(true, true) == MouseCameraMode::both,
        "combined mouse button state was not distinguished");

    constexpr float camera_yaw = 1.2f;
    constexpr float left_start_yaw = -0.8f;
    check(!rotates_camera(MouseCameraMode::none) &&
            rotates_camera(MouseCameraMode::left) &&
            rotates_camera(MouseCameraMode::right) &&
            rotates_camera(MouseCameraMode::both),
        "camera rotation did not match held mouse buttons");
    check(!controls_character_facing(MouseCameraMode::left) &&
            controls_character_facing(MouseCameraMode::right) &&
            controls_character_facing(MouseCameraMode::both),
        "only right-button modes should control character facing");
    check(movement_yaw(MouseCameraMode::left, camera_yaw, left_start_yaw) ==
            left_start_yaw &&
            movement_yaw(MouseCameraMode::right, camera_yaw, left_start_yaw) ==
                camera_yaw &&
            movement_yaw(MouseCameraMode::both, camera_yaw, left_start_yaw) ==
                camera_yaw,
        "mouse mode selected an inconsistent movement heading");
    check(!rebase_left_look_heading(MouseCameraMode::left, 1.0f, 0.0f) &&
            rebase_left_look_heading(MouseCameraMode::left, 0.0f, 0.0f) &&
            !rebase_left_look_heading(MouseCameraMode::right, 0.0f, 0.0f),
        "idle left-camera look should rebase WASD without redirecting active movement");
    const float rebased_yaw = rebase_left_look_heading(
        MouseCameraMode::left, 0.0f, 0.0f) ? camera_yaw : left_start_yaw;
    check(movement_yaw(MouseCameraMode::left, camera_yaw, rebased_yaw) == camera_yaw,
        "WASD heading did not reset to the direction reached while looking");
    const std::optional<float> right_facing =
        manual_facing_yaw(MouseCameraMode::right, camera_yaw);
    const std::optional<float> both_facing =
        manual_facing_yaw(MouseCameraMode::both, camera_yaw);
    check(!manual_facing_yaw(MouseCameraMode::left, camera_yaw) && right_facing &&
            both_facing && std::abs(*right_facing - (camera_yaw + 3.14159265f)) < 0.0001f &&
            std::abs(*both_facing - *right_facing) < 0.0001f,
        "right-button modes should steer toward the view; left look must not steer");
    check(forward_input(-1.0f, MouseCameraMode::none) == -1.0f &&
            forward_input(-1.0f, MouseCameraMode::left) == -1.0f &&
            forward_input(-1.0f, MouseCameraMode::right) == -1.0f &&
            forward_input(-1.0f, MouseCameraMode::both) == 1.0f,
        "both-button forward should be temporary and preserve keyboard input on release");
    check(follows_character(MouseCameraMode::none, false, false) &&
            !follows_character(MouseCameraMode::left, false, false) &&
            !follows_character(MouseCameraMode::right, false, false) &&
            !follows_character(MouseCameraMode::none, false, true),
        "camera should preserve manually chosen yaw after the mouse gesture ends");
}

void test_proportional_zoom_and_invalid_inputs()
{
    using namespace mmo::scene;
    CameraController near_camera;
    CameraController far_camera;
    far_camera.set_desired_distance(12.0f);
    near_camera.apply_input({0.0, 0.0, 1.0, false});
    far_camera.apply_input({0.0, 0.0, 1.0, false});
    check(std::abs(near_camera.desired_distance() / 6.0f -
              far_camera.desired_distance() / 12.0f) < 0.0001f,
        "one wheel step should apply the same proportional zoom at every distance");
    check(near_camera.desired_distance() > 5.0f,
        "one wheel step should not jump most of the way to the zoom limit");

    const auto rejects = [](auto action) {
        try {
            action();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    check(rejects([&] {
        near_camera.apply_input({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, true});
    }), "camera accepted non-finite mouse input");
    check(rejects([&] {
        near_camera.set_follow_target_yaw(std::numeric_limits<float>::infinity());
    }), "camera accepted a non-finite follow yaw");
    check(rejects([&] {
        (void)near_camera.update(0.016f,
            {{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}, 0.0f});
    }), "camera accepted a non-finite target position");
    check(rejects([&] {
        (void)near_camera.update(std::numeric_limits<float>::infinity(), {});
    }), "camera accepted a non-finite frame time");
    check(rejects([&] {
        CameraSettings invalid_settings;
        invalid_settings.field_of_view = std::numeric_limits<float>::quiet_NaN();
        CameraController invalid_camera(invalid_settings);
    }), "camera accepted a non-finite projection setting");
}

void test_camera_obstacle_collision()
{
    using namespace mmo;
    const world::CollisionWorld world(
        {},
        {world::CollisionBox{
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

void test_combat_camera_lock_on_and_target_loss()
{
    using namespace mmo;
    scene::CameraController free_camera;
    (void)free_camera.update(0.0f, {{0.0f, 0.0f, 0.0f}, 0.0f});
    game::client::CombatCameraSettings settings;
    settings.orbit.distance_smoothing_seconds = 0.0f;
    game::client::CombatCameraController camera(settings);
    camera.enter(free_camera);
    check(camera.active(), "combat camera did not activate");

    game::combat::EntityRegistry registry;
    game::combat::CombatEntity player;
    player.name = "Player";
    player.faction = game::combat::Faction::Player;
    player.id = registry.create(player);
    game::combat::CombatEntity centered;
    centered.name = "Centered";
    centered.position = {0.0f, 0.0f, 10.0f};
    const game::combat::EntityId centered_id = registry.create(centered);
    game::combat::CombatEntity closer_off_center;
    closer_off_center.name = "Off center";
    closer_off_center.position = {4.0f, 0.0f, 5.0f};
    const game::combat::EntityId off_center_id = registry.create(closer_off_center);
    game::combat::CombatEntity behind;
    behind.name = "Behind";
    behind.position = {0.0f, 0.0f, -20.0f};
    (void)registry.create(behind);

    game::combat::TargetSystem targets;
    world::CollisionWorld world;
    const scene::CameraPose view{0.0f, 2.0f, -5.0f, 0.0f, 1.0f, 0.0f};
    check(camera.choose_lock_target(registry, targets, player, view, world) == centered_id,
        "lock-on did not prefer the target near the center of the camera");
    check(targets.select(registry, player, off_center_id),
        "valid existing target could not be selected");
    check(camera.choose_lock_target(registry, targets, player, view, world) == off_center_id,
        "lock-on did not reuse the official selected target");

    targets.clear();
    world::Terrain terrain;
    const scene::CameraTarget player_view{{0.0f, 0.0f, 0.0f}, 0.0f};
    game::combat::CombatEntity& target = *registry.find(centered_id);
    const scene::CameraPose orbit_pose = camera.update(0.1f, player_view, player,
        nullptr, {}, false, world, terrain);
    camera.set_lock_target(centered_id);
    const scene::CameraPose first = camera.update(0.016f, player_view, player,
        &target, {}, false, world, terrain);
    check(camera.locked() && camera.lock_target() == centered_id,
        "combat camera did not keep the chosen target locked");
    check(first.focus_z > 0.0f && first.focus_z < 5.0f,
        "lock-on aim did not transition smoothly toward the enemy");
    check(glm::length(glm::vec3{first.eye_x - orbit_pose.eye_x,
              first.eye_y - orbit_pose.eye_y, first.eye_z - orbit_pose.eye_z}) < 0.001f,
        "acquiring a target moved the combat camera");

    target.position.z = 14.0f;
    const scene::CameraPose following = camera.update(0.1f, player_view, player,
        &target, {}, false, world, terrain);
    check(following.focus_z > first.focus_z && following.focus_z < 9.0f,
        "lock-on aim did not smoothly follow a moving target");

    target.hp = 0.0f;
    (void)camera.update(0.016f, player_view, player, &target,
        {}, false, world, terrain);
    check(!camera.locked(), "lock-on remained active after its target died");

    target.hp = target.max_hp;
    camera.set_lock_target(off_center_id);
    game::combat::CombatEntity* switched_target = registry.find(off_center_id);
    (void)camera.update(0.016f, player_view, player, switched_target,
        {}, false, world, terrain);
    check(camera.lock_target() == off_center_id,
        "combat camera did not switch to the selected target");

    switched_target->position.z = 100.0f;
    (void)camera.update(0.016f, player_view, player, switched_target,
        {}, false, world, terrain);
    check(!camera.locked(), "lock-on remained active after the target left its range");
    camera.leave(free_camera);
    check(!camera.active(), "combat camera did not leave combat mode");
}

void test_combat_camera_keeps_position_while_aiming_at_target()
{
    using namespace mmo;
    scene::CameraController free_camera;
    const scene::CameraTarget player_view{{0.0f, 0.0f, 0.0f}, 0.0f};
    (void)free_camera.update(0.0f, player_view);

    game::combat::EntityRegistry registry;
    game::combat::CombatEntity player;
    player.faction = game::combat::Faction::Player;
    player.id = registry.create(player);
    game::combat::CombatEntity target;
    target.position = {10.0f, 0.0f, 0.0f};
    target.id = registry.create(target);

    game::client::CombatCameraSettings settings;
    settings.orbit.distance_smoothing_seconds = 0.0f;
    game::client::CombatCameraController camera(settings);
    camera.enter(free_camera);
    world::CollisionWorld collision_world;
    world::Terrain terrain;
    const scene::CameraPose orbit_pose = camera.update(0.05f, player_view, player,
        nullptr, {}, false, collision_world, terrain);
    camera.set_lock_target(target.id);
    scene::CameraPose pose{};
    for (int frame = 0; frame < 100; ++frame) {
        pose = camera.update(0.05f, player_view, player, &target, {}, false,
            collision_world, terrain);
    }

    const glm::vec2 view_direction{pose.focus_x - pose.eye_x,
        pose.focus_z - pose.eye_z};
    const glm::vec2 target_direction{target.position.x - pose.eye_x,
        target.position.z - pose.eye_z};
    const float alignment = glm::dot(glm::normalize(view_direction),
        glm::normalize(target_direction));
    check(alignment > 0.98f,
        "lock-on camera did not turn to frame a target beside the player");
    check(glm::length(glm::vec3{pose.eye_x - orbit_pose.eye_x,
              pose.eye_y - orbit_pose.eye_y, pose.eye_z - orbit_pose.eye_z}) < 0.001f,
        "lock-on camera moved instead of only aiming at the target");
}

void test_combat_camera_keeps_manual_yaw_after_release()
{
    using namespace mmo;
    game::client::CombatCameraSettings settings;
    settings.orbit.rotation_smoothing_seconds = 0.0f;
    settings.orbit.follow_rotation_smoothing_seconds = 0.0f;
    scene::CameraController free_camera;
    const scene::CameraTarget player_view{{0.0f, 0.0f, 0.0f}, 0.0f};
    (void)free_camera.update(0.0f, player_view);
    game::client::CombatCameraController camera(settings);
    camera.enter(free_camera);

    game::combat::CombatEntity player;
    player.id = 1;
    player.faction = game::combat::Faction::Player;
    const world::CollisionWorld collision_world;
    const world::Terrain terrain;
    const scene::CameraPose manually_rotated = camera.update(0.1f, player_view,
        player, nullptr, {80.0, -20.0, 0.0, true}, false,
        collision_world, terrain);
    const scene::CameraPose after_release = camera.update(0.1f, player_view,
        player, nullptr, {}, false, collision_world, terrain);
    check(std::abs(std::remainder(
              after_release.horizontal_yaw() - manually_rotated.horizontal_yaw(),
              2.0f * 3.14159265f)) < 0.0001f,
        "combat camera returned behind the character after manual orbit was released");
}

void test_combat_camera_obstruction_and_terrain()
{
    using namespace mmo;
    game::client::CombatCameraSettings settings;
    settings.obstruction_grace_seconds = 0.08f;
    settings.orbit.distance_smoothing_seconds = 0.0f;

    game::combat::CombatEntity player;
    player.id = 1;
    player.faction = game::combat::Faction::Player;
    game::combat::CombatEntity target;
    target.id = 2;
    target.position = {0.0f, 0.0f, 10.0f};
    const scene::CameraTarget player_view{{0.0f, 0.0f, 0.0f}, 0.0f};
    world::Terrain flat_terrain;
    world::CollisionWorld obstructed_world({}, {
        world::CollisionBox{{-1.0f, 0.0f, -2.2f}, {1.0f, 8.0f, -1.8f}},
        world::CollisionBox{{-2.0f, 0.0f, 6.0f}, {2.0f, 8.0f, 7.0f}}});
    scene::CameraController free_camera;
    (void)free_camera.update(0.0f, player_view);
    game::client::CombatCameraController camera(settings);
    camera.enter(free_camera);
    camera.set_lock_target(target.id);

    scene::CameraPose pose{};
    float distance_on_first_frame = 0.0f;
    const float frame_times[] = {1.0f / 30.0f, 1.0f / 144.0f, 1.0f / 60.0f};
    for (int frame = 0; frame < 24 && camera.locked(); ++frame) {
        pose = camera.update(frame_times[frame % 3], player_view, player,
            &target, {}, false, obstructed_world, flat_terrain);
        if (frame == 0) {
            distance_on_first_frame = glm::length(
                glm::vec3{pose.eye_x - pose.focus_x, pose.eye_y - pose.focus_y,
                    pose.eye_z - pose.focus_z});
        }
        check(std::isfinite(pose.eye_x) && std::isfinite(pose.eye_y) &&
                std::isfinite(pose.eye_z),
            "combat camera produced a non-finite pose while avoiding an obstacle");
    }
    check(!camera.locked(), "persistent obstruction did not release the lock-on");
    check(distance_on_first_frame < 10.9f,
        "combat camera did not shorten its orbit near a wall");

    world::Terrain raised_terrain;
    check(raised_terrain.apply_brush({0.0f, -1.0f}, 4.0f, 2.0f, 1.0f,
            world::TerrainBrush::raise),
        "raised terrain fixture could not be created");
    game::combat::CombatEntity hill_player = player;
    hill_player.position.y = raised_terrain.height_at({0.0f, 0.0f});
    scene::CameraTarget hill_view = player_view;
    hill_view.position.y = hill_player.position.y;
    scene::CameraController terrain_free_camera;
    (void)terrain_free_camera.update(0.0f, hill_view);
    game::client::CombatCameraController terrain_camera(settings);
    terrain_camera.enter(terrain_free_camera);
    scene::CameraPose terrain_pose = terrain_camera.update(0.016f,
        hill_view, hill_player, nullptr, {}, false, world::CollisionWorld{},
        raised_terrain);
    for (int frame = 0; frame < 12; ++frame) {
        terrain_pose = terrain_camera.update(frame_times[frame % 3], hill_view,
            hill_player, nullptr, {}, frame % 2 == 0,
            world::CollisionWorld{}, raised_terrain);
    }
    const glm::vec2 camera_ground{terrain_pose.eye_x, terrain_pose.eye_z};
    check(terrain_pose.eye_y >= raised_terrain.height_at(camera_ground) +
            settings.terrain_clearance - 0.001f,
        "combat camera intersected raised terrain");
}

}

int main()
{
    try {
        test_orbit_and_target_independence();
        test_camera_returns_behind_character_smoothly();
        test_camera_view_can_be_transferred_between_modes();
        test_pitch_zoom_and_follow();
        test_projection_validation();
        test_camera_pose_horizontal_yaw_ignores_pitch();
        test_mouse_camera_mode_controls();
        test_proportional_zoom_and_invalid_inputs();
        test_camera_obstacle_collision();
        test_combat_camera_lock_on_and_target_loss();
        test_combat_camera_keeps_position_while_aiming_at_target();
        test_combat_camera_keeps_manual_yaw_after_release();
        test_combat_camera_obstruction_and_terrain();
        std::cout << "ThirdPersonCameraTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ThirdPersonCameraTest failed: " << error.what() << '\n';
        return 1;
    }
}
