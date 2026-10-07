#include "game/world/terrain.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_brush_shapes_and_height_sampling()
{
    using namespace mmo::game::world;
    Terrain terrain;
    check(terrain.apply_brush({0.0f, 0.0f}, 4.0f, 2.0f, 1.0f, TerrainBrush::raise),
        "raise brush did not modify terrain");
    check(terrain.height_at({0.0f, 0.0f}) > 1.9f,
        "raise brush did not reach its center strength");
    check(terrain.height_at({3.0f, 0.0f}) > 0.0f &&
            terrain.height_at({3.0f, 0.0f}) < terrain.height_at({0.0f, 0.0f}),
        "raise brush did not apply radial falloff");
    check(terrain.height_at({4.0f, 0.0f}) == 0.0f,
        "raise brush affected terrain outside its radius");
    glm::vec3 hit;
    check(terrain.intersect_ray({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f, hit),
        "downward ray did not intersect sculpted terrain");
    check(std::abs(hit.y - terrain.height_at({0.0f, 0.0f})) < 0.001f,
        "ray intersection did not follow the sculpted terrain height");

    const float center_height = terrain.height_at({0.0f, 0.0f});
    check(terrain.apply_brush(
            {0.0f, 0.0f}, 4.0f, 3.0f, 1.0f, TerrainBrush::flatten, 0.0f),
        "flatten brush did not modify terrain");
    check(terrain.height_at({0.0f, 0.0f}) < center_height,
        "flatten brush did not approach the target height");
    check(terrain.apply_brush({0.0f, 0.0f}, 4.0f, 2.0f, 1.0f, TerrainBrush::lower),
        "lower brush did not modify terrain");
    check(terrain.height_at({0.0f, 0.0f}) < 0.0f,
        "lower brush did not lower the terrain");
}

void test_mesh_and_ray_intersection()
{
    using namespace mmo::game::world;
    Terrain terrain;
    glm::vec3 hit;
    const auto mesh = terrain.vertices();
    check(mesh.size() == static_cast<std::size_t>(Terrain::kCells * Terrain::kCells * 6),
        "terrain mesh has the wrong number of triangle vertices");
    check(std::abs(mesh.front().normal.y - 1.0f) < 0.0001f,
        "flat terrain has an invalid normal");

    check(terrain.intersect_ray({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f, hit),
        "downward ray did not intersect flat terrain");
    check(std::abs(hit.y) < 0.001f,
        "ray intersection returned the wrong ground height");
    check(!terrain.intersect_ray({0.0f, 5.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 20.0f, hit),
        "upward ray incorrectly intersected flat terrain");
}

void test_save_load_round_trip()
{
    using namespace mmo::game::world;
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "mmo-terrain-test.mmoterrain";
    try {
        Terrain source;
        check(source.apply_brush({-3.0f, 2.0f}, 5.0f, 2.5f, 0.75f, TerrainBrush::raise),
            "test setup did not change terrain");
        source.save(path);

        Terrain loaded;
        loaded.load(path);
        check(std::abs(loaded.height_at({-3.0f, 2.0f}) -
                source.height_at({-3.0f, 2.0f})) < 0.00001f,
            "saved terrain did not load with the same heights");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw;
    }
    std::filesystem::remove(path);
}

}

int main()
{
    try {
        test_brush_shapes_and_height_sampling();
        test_mesh_and_ray_intersection();
        test_save_load_round_trip();
        std::cout << "Terrain tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Terrain test failed: " << error.what() << '\n';
        return 1;
    }
}
