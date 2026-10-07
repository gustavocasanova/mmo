#include "assets/procedural_meshes.hpp"
#include "game/characters/character.hpp"
#include "game/client/demo_scene.hpp"
#include "game/client/character_visual.hpp"
#include "game/items/item.hpp"
#include "scene/camera_controller.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void near(float actual, float expected, const char* message)
{
    check(std::isfinite(actual) && std::abs(actual - expected) < 0.0001f, message);
}
template<class F> void rejects(F action, const char* message)
{
    try { action(); } catch (const std::logic_error&) { return; }
    throw std::runtime_error(message);
}
void movement()
{
    using namespace mmo::game;
    characters::Character straight, diagonal;
    const world::MovementBounds bounds;
    straight.update(1.0f, {1, 0}, bounds);
    diagonal.update(1.0f, {1, 1}, bounds);
    near(straight.position().z, 4.2f, "forward speed changed");
    near(glm::length(diagonal.position()), glm::length(straight.position()), "diagonal speed differs");
    characters::Character turned;
    turned.rotate(1.5707963268f);
    turned.update(1.0f, {1, 0}, bounds);
    near(turned.position().x, 4.2f, "movement must follow character orientation");
    near(turned.position().z, 0, "rotated movement drift");
    for (int i = 0; i < 200; ++i) diagonal.update(0.1f, {1, 1}, bounds);
    near(diagonal.position().x, 36, "world X limit lost");
    near(diagonal.position().z, 36, "world Z limit lost");
    const auto before = diagonal.position();
    diagonal.update(0, {-1, -1}, bounds);
    check(diagonal.position() == before, "zero time must not move character");
    diagonal.update(0.1f, {}, bounds);
    check(!diagonal.walking(), "idle character remains walking");
    rejects([&] { diagonal.update(-1, {}, bounds); }, "negative time accepted");
}
void items()
{
    using namespace mmo::game::items;
    rejects([] { ItemDefinition bad("", "Wood", 20); }, "empty item ID accepted");
    rejects([] { ItemDefinition bad("wood", "Wood", 0); }, "zero stack limit accepted");
    ItemStack stack(ItemDefinition("wood", "Wood", 20), 3);
    check(stack.try_add(17) && stack.quantity() == 20, "stack fill failed");
    check(!stack.try_add(std::numeric_limits<std::uint32_t>::max()), "stack overflow accepted");
    check(stack.quantity() == 20, "failed addition mutated stack");
    check(!stack.try_remove(21) && stack.quantity() == 20, "failed removal mutated stack");
    check(stack.try_remove(20) && stack.quantity() == 0, "emptying stack failed");
    rejects([] { ItemStack bad(ItemDefinition("wood", "Wood", 20), 21); }, "oversized stack accepted");
}
void scene_and_assets()
{
    using namespace mmo;
    assets::MeshCatalog catalog;
    rejects([&] { catalog.add({}); }, "empty mesh accepted");
    game::client::DemoScene scene(catalog);
    game::characters::Character character;
    auto draws = scene.update(0, character);
    check(draws.size() == 2, "equipment demo must draw ground and character");
    check(catalog.meshes().size() == 2, "demo should register two meshes");
    check(catalog.get(draws[0].mesh).vertices.size() == 6, "optimized ground must stay a single quad");
    const auto character_mesh = draws[1].mesh;
    const auto original_vertices = catalog.get(character_mesh).vertices.size();
    check(original_vertices > 36, "upstream equipment character geometry lost");
    auto equipment = game::client::make_default_character_equipment();
    equipment[game::client::EquipmentSlot::Helmet].equipped = false;
    check(scene.set_equipment(catalog, equipment) == character_mesh, "equipment replacement changed mesh ID");
    check(catalog.get(character_mesh).vertices.size() < original_vertices, "helmet toggle did not change geometry");
    scene.set_equipment(catalog, game::client::make_default_character_equipment());
    check(catalog.get(character_mesh).vertices.size() == original_vertices, "equipment reset lost geometry");
    character.update(0.1f, {1, 0}, {});
    draws = scene.update(0.1f, character);
    near(draws[1].transform[3].z, 0.42f, "visual does not follow character");
    check(catalog.meshes().size() == 2, "equipment update reallocates catalog IDs");
    rejects([&] { catalog.get({99}); }, "unknown mesh ID accepted");
    rejects([&] { catalog.replace(character_mesh, {}); }, "invalid replacement accepted");
    check(catalog.get(character_mesh).vertices.size() == original_vertices, "failed replacement changed geometry");

    // Keep the PR's reusable block-character animation tested independently of the new demo.
    const auto cube = catalog.add(assets::make_cube());
    game::client::CharacterVisual blocks(cube);
    std::vector<scene::DrawItem> parts;
    blocks.append_draws(character, parts);
    check(parts.size() == 10, "block character parts lost");
    const auto original_leg = parts[6].transform;
    blocks.update(0.1f, true);
    parts.clear();
    blocks.append_draws(character, parts);
    check(parts[6].transform != original_leg, "block walking animation lost");
}
void camera()
{
    using namespace mmo::scene;
    CameraSettings settings;
    settings.distance_smoothing_seconds = 0.0f;
    CameraController controller(settings);
    const auto initial = controller.update(0, {{0, 0, 0}, 0});
    near(initial.focus_y, 1.5f, "camera focus changed");
    check(initial.eye_z < 0, "camera must start behind character");
    const float rotation = controller.apply_input({20, 0, 0, true});
    near(rotation, 0.0f, "free camera rotated the target");
    controller.apply_input({0, 10000, -10000, true});
    Camera view;
    for (int i = 0; i < 120; ++i) {
        auto pose = controller.update(1.0f / 60.0f, {{0, 0, 0}, 0});
        view.set_pose(pose);
    }
    const auto matrix = view.projection(16.0f / 9.0f) * view.view();
    for (int col = 0; col < 4; ++col) for (int row = 0; row < 4; ++row)
        check(std::isfinite(matrix[col][row]), "camera produced invalid matrix");
    rejects([&] { view.projection(0); }, "zero aspect accepted");
    rejects([&] { view.set_pose({0, 0, 0, 0, 0, 0}); }, "degenerate camera pose accepted");
}
}
int main()
{
    try {
        movement(); items(); scene_and_assets(); camera();
        std::cout << "Movement, items, scene/assets, and camera checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
