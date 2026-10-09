#include "assets/static_model_geometry.hpp"
#include "editor/asset_database.hpp"
#include "editor/editor_history.hpp"
#include "editor/world_editor.hpp"
#include "editor/editor_camera.hpp"
#include "editor/selection.hpp"
#include "editor/world_document.hpp"
#include "game/world/terrain.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
    check(camera.yaw() > yaw_before_look && camera.pitch() > pitch_before_look,
        "mouse look did not rotate the editor camera");
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

void test_local_axis_and_rotated_selection_bounds()
{
    using namespace mmo::editor;
    SelectionManager selection({
        {9, {{-2.0f, -0.5f, -0.5f}, {2.0f, 0.5f, 0.5f}}},
    });
    check(selection.select_ray(
            {0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 1.0f}, 10.0f).has_value(),
        "rotated-object test could not select the source object");

    selection.set_snapping_enabled(false);
    selection.set_transform_mode(TransformMode::rotate);
    selection.set_transform_axis(TransformAxis::y);
    selection.transform_selected({}, {0.0f, 90.0f, 0.0f}, {});
    const auto bounds = selection.bounds_for(9);
    check(bounds && bounds->maximum.z - bounds->minimum.z > 3.9f,
        "rotation did not update the object's world-space bounds");
    check(selection.select_ray(
            {0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 1.0f}, 10.0f).has_value(),
        "ray selection did not follow the rotated object");

    selection.set_transform_space(TransformSpace::local);
    selection.set_transform_axis(TransformAxis::x);
    const glm::vec3 axis = selection.transform_axis_direction();
    check(std::abs(axis.x) < 0.0001f && std::abs(std::abs(axis.z) - 1.0f) < 0.0001f,
        "local transform axis did not follow the object's rotation");
}

void test_hierarchy_rename_duplicate_delete_and_inspector_transform()
{
    using namespace mmo::editor;
    SelectionManager selection({
        {12, {{1.0f, 0.0f, 2.0f}, {3.0f, 2.0f, 4.0f}},
            {}, {}, glm::vec3{1.0f}, "Stone"},
    });
    check(selection.select(12), "hierarchy could not select an object by ID");
    check(selection.rename(12, "Ancient Stone"),
        "hierarchy could not rename an object");
    check(selection.object_for(12)->name == "Ancient Stone",
        "renamed object was not visible to the inspector");

    const auto duplicate_id = selection.duplicate_selected();
    check(duplicate_id && *duplicate_id != 12 &&
            selection.selected() == duplicate_id,
        "duplicate did not create and select a new object ID");
    const auto duplicate = selection.object_for(*duplicate_id);
    check(duplicate && duplicate->name == "Ancient Stone Copy" &&
            std::abs(duplicate->position.x - 3.0f) < 0.0001f,
        "duplicate did not receive a unique copy name and offset");
    check(selection.select(12),
        "hierarchy could not reselect the original object");
    const auto second_duplicate_id = selection.duplicate_selected();
    check(second_duplicate_id &&
            selection.object_for(*second_duplicate_id)->name == "Ancient Stone Copy 2",
        "duplicate naming did not disambiguate existing names");

    check(selection.set_transform(
            *second_duplicate_id, {8.0f, 2.0f, 4.0f}, {0.0f, 30.0f, 0.0f},
            {2.0f, 1.0f, 1.0f}),
        "inspector did not update an object's transform");
    const auto bounds = selection.bounds_for(*second_duplicate_id);
    check(bounds && bounds->maximum.x - bounds->minimum.x > 4.4f,
        "inspector transform did not update world-space bounds");
    check(selection.delete_selected() && !selection.selected() &&
            !selection.object_for(*second_duplicate_id),
        "delete did not remove the selected object");
    check(selection.objects().size() == 2,
        "delete changed the wrong hierarchy item");
}

void test_asset_database_indexes_models_and_round_trips_prefabs()
{
    const auto unique_stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("mmo_asset_database_test_" + std::to_string(unique_stamp));
    std::filesystem::create_directories(root / "assets" / "vehicles");
    std::filesystem::create_directories(root / "build");
    {
        std::ofstream model(root / "assets" / "vehicles" / "cart.GLB");
        model << "test fixture";
    }
    {
        std::ofstream ignored_model(root / "build" / "ignored.glb");
        ignored_model << "not indexed";
    }

    try {
        mmo::editor::AssetDatabase database(root);
        database.refresh();
        check(database.entries().size() == 1 &&
                database.entries().front().kind == mmo::editor::AssetKind::model,
            "asset database did not index model files or included a build directory");

        mmo::editor::AssetEntry prefab;
        std::string error;
        check(database.create_prefab(
                database.entries().front().relative_path, "TestCart", prefab, error),
            "asset database could not create a prefab manifest");
        check(prefab.kind == mmo::editor::AssetKind::prefab &&
                prefab.model_path == database.entries().front().relative_path,
            "created prefab did not reference its source model");
        check(database.resolve_model_path(prefab.relative_path) ==
                std::filesystem::absolute(root / "assets" / "vehicles" / "cart.GLB"),
            "prefab did not resolve to its model file");

        mmo::editor::AssetDatabase reloaded(root);
        reloaded.refresh();
        check(reloaded.entries().size() == 2,
            "asset database did not reload the saved prefab");
        check(!reloaded.create_prefab(
                reloaded.entries().front().relative_path, "../escape", prefab, error) &&
                !error.empty(),
            "asset database accepted an unsafe prefab name");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        throw;
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

void test_static_model_geometry_applies_node_hierarchy_and_centers_bounds()
{
    using namespace mmo::assets;
    Model model;
    Mesh mesh;
    mesh.vertices = {
        ModelVertex{{0.0f, 0.0f, 0.0f}},
        ModelVertex{{2.0f, 0.0f, 0.0f}},
        ModelVertex{{0.0f, 2.0f, 0.0f}},
    };
    mesh.indices = {0, 1, 2};
    model.meshes.push_back(mesh);

    Node parent;
    parent.local_transform.translation = {2.0f, 0.0f, 0.0f};
    Node child;
    child.parent_index = 0;
    child.local_transform.translation = {0.0f, 3.0f, 0.0f};
    child.meshes = {0};
    model.nodes = {parent, child};

    const StaticModelGeometry geometry = build_static_model_geometry(model);
    check(geometry.vertices.size() == 3 &&
            std::abs(geometry.minimum.x + 1.0f) < 0.0001f &&
            std::abs(geometry.maximum.x - 1.0f) < 0.0001f &&
            std::abs(geometry.minimum.y + 1.0f) < 0.0001f &&
            std::abs(geometry.maximum.y - 1.0f) < 0.0001f,
        "static model geometry did not produce centered node-transformed bounds");
    check(std::abs(geometry.vertices.front().position[0] + 1.0f) < 0.0001f &&
            std::abs(geometry.vertices.front().position[1] + 1.0f) < 0.0001f,
        "static model vertices did not include their parent node transforms");
}

void test_selection_can_add_asset_instances()
{
    using namespace mmo::editor;
    SelectionManager selection({
        {5, {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}},
    });
    const SelectionId id = selection.add_object({
        0,
        {{-2.0f, -1.0f, -3.0f}, {2.0f, 1.0f, 3.0f}},
        {8.0f, 4.0f, -2.0f},
        {},
        glm::vec3{1.0f},
        "Cart",
        "assets/vehicles/cart.glb",
    });
    const auto object = selection.object_for(id);
    check(object && selection.selected() == id &&
            object->asset_path == "assets/vehicles/cart.glb" &&
            glm::length(object->position - glm::vec3{8.0f, 4.0f, -2.0f}) < 0.0001f,
        "new asset instance did not preserve its model reference and placement");
}

void test_world_document_round_trip()
{
    using namespace mmo::editor;
    using namespace mmo::game::world;
    const auto path = std::filesystem::temp_directory_path() /
        "mmo-world-document-test.mmoworld";
    const auto region_path = std::filesystem::path{path.string() + ".regions"};
    try {
        Terrain source_terrain;
        check(source_terrain.apply_brush(
                {2.0f, -3.0f}, 5.0f, 2.0f, 0.75f, TerrainBrush::raise),
            "world document test setup did not change terrain");
        check(source_terrain.paint_material(
                {2.0f, -3.0f}, 3.0f, 2.0f, 1.0f,
                TerrainMaterial::sand),
            "world document test setup did not paint material");
        SelectionManager source_selection({});
        (void)source_selection.add_object({
            12, {{-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f}},
            {8.0f, 4.0f, -2.0f}, {10.0f, 20.0f, 30.0f},
            {1.5f, 2.0f, 0.75f}, "Cart With Space",
            "assets/vehicles/cart.glb"});
        WorldDocument::save(path, source_terrain, source_selection.objects());
        WorldDocumentData loaded = WorldDocument::load(path);
        check(std::abs(loaded.terrain.height_at({2.0f, -3.0f}) -
                source_terrain.height_at({2.0f, -3.0f})) < 0.00001f,
            "world document did not preserve the terrain heightmap");
        check(loaded.terrain.material_weights_at({2.0f, -3.0f}).w > 0.99f,
            "world document did not preserve terrain material weights");
        check(loaded.objects.size() == 1 &&
                loaded.objects.front().name == "Cart With Space" &&
                loaded.objects.front().asset_path == "assets/vehicles/cart.glb" &&
                glm::length(loaded.objects.front().position -
                    glm::vec3{8.0f, 4.0f, -2.0f}) < 0.0001f &&
                glm::length(loaded.objects.front().rotation_degrees -
                    glm::vec3{10.0f, 20.0f, 30.0f}) < 0.0001f &&
                glm::length(loaded.objects.front().scale -
                    glm::vec3{1.5f, 2.0f, 0.75f}) < 0.0001f,
            "world document did not preserve object data and transforms");
        check(loaded.terrain.resident_region_count() <=
                loaded.terrain.region_storage_limit() &&
                std::filesystem::exists(region_path),
            "loaded world did not establish bounded regional terrain storage");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove_all(region_path, ignored);
        throw;
    }
    std::filesystem::remove(path);
    std::error_code ignored;
    std::filesystem::remove_all(region_path, ignored);
}

void test_editor_history_undoes_and_redoes_world_state()
{
    using namespace mmo::editor;
    using namespace mmo::game::world;
    Terrain terrain;
    SelectionManager selection({
        {3, {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}},
    });
    (void)selection.select(3);
    const EditorSnapshot before = capture_snapshot(terrain, selection);
    check(terrain.apply_brush({0.0f, 0.0f}, 3.0f, 2.0f, 1.0f, TerrainBrush::raise),
        "history test terrain setup did not modify terrain");
    const EditorSnapshot after_brush = capture_snapshot(terrain, selection);
    check(selection.set_transform(
            3, {4.0f, 2.0f, 1.0f}, {}, glm::vec3{1.0f}),
        "history test object setup did not modify its transform");
    EditorHistory history;
    history.record(before, after_brush, "test");
    history.record(after_brush, capture_snapshot(terrain, selection), "test");
    check(history.undo_count() == 1,
        "consecutive changes in one interaction were not coalesced");

    check(history.undo(terrain, selection) &&
            terrain.height_at({0.0f, 0.0f}) == 0.0f &&
            selection.object_for(3)->position == glm::vec3{0.0f},
        "undo did not restore the terrain and object state");
    check(history.redo(terrain, selection) &&
            terrain.height_at({0.0f, 0.0f}) > 1.9f &&
            selection.object_for(3)->position == glm::vec3{4.0f, 2.0f, 1.0f},
        "redo did not reapply the terrain and object state");
}

}

int main()
{
    try {
        test_mode_switches_on_key_edges();
        test_editor_camera_navigation_and_speed_controls();
        test_selection_raycast_selects_nearest_and_clears_on_miss();
        test_transform_modes_axes_and_snap();
        test_local_axis_and_rotated_selection_bounds();
        test_hierarchy_rename_duplicate_delete_and_inspector_transform();
        test_asset_database_indexes_models_and_round_trips_prefabs();
        test_static_model_geometry_applies_node_hierarchy_and_centers_bounds();
        test_selection_can_add_asset_instances();
        test_world_document_round_trip();
        test_editor_history_undoes_and_redoes_world_state();
        std::cout << "WorldEditorModeTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WorldEditorModeTest failed: " << error.what() << '\n';
        return 1;
    }
}
