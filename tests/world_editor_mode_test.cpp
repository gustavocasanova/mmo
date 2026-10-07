#include "editor/world_editor.hpp"
#include "editor/editor_camera.hpp"
#include "editor/selection.hpp"

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

void test_mode_switches_on_key_edges()
{
    mmo::editor::WorldEditor editor;
    check(editor.mode() == mmo::editor::EngineMode::game && !editor.is_active(),
        "editor did not start in game mode");
    check(!editor.update_toggle(false), "released toggle changed the mode");
    check(editor.update_toggle(true) && editor.is_active(),
        "toggle press did not enter world editor mode");
    check(!editor.update_toggle(true) && editor.is_active(),
        "holding the toggle key changed modes more than once");
    check(!editor.update_toggle(false) && editor.is_active(),
        "releasing the toggle key changed modes");
    check(editor.update_toggle(true) && !editor.is_active() &&
            editor.mode() == mmo::editor::EngineMode::game,
        "second toggle press did not return to game mode");
}

void test_editor_camera_navigation_and_speed_controls()
{
    using namespace mmo::editor;
    EditorCamera camera;
    camera.set_pose({
        0.0f, 2.0f, 0.0f,
        0.0f, 2.0f, 1.0f,
    });
    camera.update(1.0f, {.right = 1.0f});
    check(camera.position().x < -7.9f && camera.position().x > -8.1f,
        "D/right strafe moved in the opposite direction");
    camera.set_pose({
        0.0f, 2.0f, 0.0f,
        0.0f, 2.0f, 1.0f,
    });
    camera.update(1.0f, {.right = -1.0f});
    check(camera.position().x > 7.9f && camera.position().x < 8.1f,
        "A/left strafe moved in the opposite direction");
    camera.set_pose({
        0.0f, 2.0f, 0.0f,
        0.0f, 2.0f, 1.0f,
    });
    camera.update(1.0f, {.forward = 1.0f});
    check(camera.position().z > 7.9f && camera.position().z < 8.1f,
        "editor camera did not fly forward at its configured base speed");

    const float before_vertical = camera.position().y;
    camera.update(0.5f, {.up = 1.0f});
    check(camera.position().y > before_vertical + 3.9f,
        "editor camera did not move vertically");

    const float before_fast_move = camera.position().z;
    camera.update(0.5f, {.forward = 1.0f, .fast = true});
    check(camera.position().z > before_fast_move + 15.9f,
        "fast modifier did not increase editor camera speed");

    const float before_slow_move = camera.position().z;
    camera.update(1.0f, {.forward = 1.0f, .slow = true});
    check(camera.position().z > before_slow_move &&
            camera.position().z < before_slow_move + 2.0f,
        "precision modifier did not reduce editor camera speed");

    const float speed_before_wheel = camera.camera_speed();
    camera.update(0.0f, {.scroll_delta = 1.0});
    check(camera.camera_speed() > speed_before_wheel,
        "mouse wheel did not increase editor camera speed");
    camera.update(0.0f, {.scroll_delta = -100.0});
    check(camera.camera_speed() >= camera.settings().minimum_speed,
        "editor camera speed fell below its configured minimum");

    const float yaw_before_look = camera.yaw();
    const float pitch_before_look = camera.pitch();
    camera.update(0.0f, {.mouse_delta_x = 100.0, .mouse_delta_y = -100.0, .look = true});
    check(camera.yaw() < yaw_before_look && camera.pitch() > pitch_before_look,
        "editor mouse-look axes were not inverted");
    check(camera.pose().far_plane > camera.pose().near_plane,
        "editor camera did not provide a valid render pose");
    camera.focus_on({2.0f, 1.0f, -3.0f});
    check(glm::length(camera.position() + camera.forward() *
            camera.settings().camera_focus_distance -
            glm::vec3{2.0f, 1.0f, -3.0f}) < 0.0001f,
        "editor camera did not focus the supplied position");
}

void test_selection_raycast_selects_nearest_and_clears_on_miss()
{
    using namespace mmo::editor;
    SelectionManager selection({
        {42, {{-1.0f, -1.0f, 8.0f}, {1.0f, 1.0f, 10.0f}}},
        {17, {{-1.0f, -1.0f, 3.0f}, {1.0f, 1.0f, 5.0f}}},
    });
    const auto nearest = selection.select_ray(
        {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f}, 100.0f);
    check(nearest && nearest->id == 17 && std::abs(nearest->distance - 3.0f) < 0.0001f,
        "selection ray did not choose the nearest object");
    check(selection.selected() == 17,
        "selection manager did not retain the selected object");
    const auto selected_bounds = selection.bounds_for(*selection.selected());
    check(selected_bounds && selected_bounds->minimum.z == 3.0f,
        "selection manager did not expose selected bounds for camera focus");

    const auto miss = selection.select_ray(
        {5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 100.0f);
    check(!miss && !selection.selected(),
        "ray miss did not clear the current selection");
}

void test_transform_modes_axes_and_snap()
{
    using namespace mmo::editor;
    SelectionManager selection({
        {5, {{-1.0f, -1.0f, 4.0f}, {1.0f, 1.0f, 6.0f}}},
    });
    const auto hit = selection.select_ray(
        {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 20.0f);
    check(hit && selection.selected() == 5, "transform test could not select object");

    selection.set_transform_mode(TransformMode::translate);
    selection.set_transform_axis(TransformAxis::x);
    selection.transform_selected({0.6f, 0.0f, 0.0f}, {}, {});
    auto object = selection.object_for(5);
    check(object && std::abs(object->position.x - 1.0f) < 0.0001f,
        "translation grid snap did not round to the configured 1-unit grid");

    selection.set_snapping_enabled(false);
    selection.transform_selected({0.25f, 0.0f, 0.0f}, {}, {});
    object = selection.object_for(5);
    check(object && std::abs(object->position.x - 1.25f) < 0.0001f,
        "translation with snapping disabled did not preserve its delta");

    selection.set_transform_mode(TransformMode::rotate);
    selection.set_transform_axis(TransformAxis::y);
    selection.set_snapping_enabled(true);
    selection.transform_selected({}, {0.0f, 8.0f, 0.0f}, {});
    object = selection.object_for(5);
    check(object && std::abs(object->rotation_degrees.y - 15.0f) < 0.0001f,
        "rotation snap did not round to 15 degrees");

    selection.set_transform_mode(TransformMode::scale);
    selection.set_transform_axis(TransformAxis::uniform);
    selection.transform_selected({}, {}, {0.06f, 0.06f, 0.06f});
    object = selection.object_for(5);
    check(object && glm::length(object->scale - glm::vec3{1.1f}) < 0.0001f,
        "uniform scale did not apply scale snap");
    const auto bounds = selection.bounds_for(5);
    check(bounds && bounds->maximum.x - bounds->minimum.x > 2.1f,
        "transformed bounds did not reflect object scale");
}

}

int main()
{
    try {
        test_mode_switches_on_key_edges();
        test_editor_camera_navigation_and_speed_controls();
        test_selection_raycast_selects_nearest_and_clears_on_miss();
        test_transform_modes_axes_and_snap();
        std::cout << "WorldEditorModeTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WorldEditorModeTest failed: " << error.what() << '\n';
        return 1;
    }
}
