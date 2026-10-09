#include "game/world/terrain.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
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
    const auto center_chunks = terrain.chunks_near({0.0f, 0.0f}, 0);
    check(center_chunks.size() == 1 &&
            center_chunks.front().x == Terrain::kChunksPerSide / 2 &&
            center_chunks.front().z == Terrain::kChunksPerSide / 2,
        "zero-radius streaming did not select only the camera's chunk");
    check(center_chunks.front().vertices.size() == static_cast<std::size_t>(
            Terrain::kChunkCells * Terrain::kChunkCells * 6) &&
            std::abs(center_chunks.front().vertices.front().normal.y - 1.0f) < 0.0001f,
        "streamed terrain chunk has invalid geometry or normals");
    const auto expanded_chunks = terrain.chunks_near({0.0f, 0.0f}, 1);
    check(expanded_chunks.size() == 9,
        "streaming radius did not load the neighboring chunks");
    const TerrainChunk& left_chunk = expanded_chunks[3];
    const TerrainChunk& right_chunk = expanded_chunks[4];
    check(left_chunk.x + 1 == right_chunk.x && left_chunk.z == right_chunk.z &&
            left_chunk.vertices.front().position.x ==
                static_cast<float>(left_chunk.x * Terrain::kChunkCells) -
                    Terrain::kHalfExtent &&
            right_chunk.vertices.front().position.x ==
                static_cast<float>(right_chunk.x * Terrain::kChunkCells) -
                    Terrain::kHalfExtent,
        "streamed chunks did not preserve their global heightmap coordinates");
    Terrain sculpted;
    check(sculpted.apply_brush(
            {16.0f, 5.0f}, 2.0f, 1.0f, 1.0f, TerrainBrush::raise),
        "chunk seam test could not sculpt across the chunk boundary");
    const std::uint64_t old_left_revision = sculpted.chunk_revision(12, 12);
    const std::uint64_t old_right_revision = sculpted.chunk_revision(13, 12);
    check(sculpted.apply_brush(
            {16.0f, 5.0f}, 2.0f, 1.0f, 1.0f, TerrainBrush::raise),
        "second chunk seam edit did not change terrain");
    const auto sculpted_chunks = sculpted.chunks_near({16.0f, 5.0f}, 1);
    const TerrainChunk& sculpted_left = sculpted_chunks[3];
    const TerrainChunk& sculpted_right = sculpted_chunks[4];
    const TerrainVertex& left_seam = sculpted_left.vertices[(5 * 16 + 15) * 6 + 2];
    const TerrainVertex& right_seam = sculpted_right.vertices[(5 * 16) * 6 + 1];
    check(left_seam.position == right_seam.position &&
            left_seam.normal == right_seam.normal,
        "adjacent chunks do not share matching height and normal values");
    check(sculpted.chunk_revision(sculpted_left.x, sculpted_left.z) >
            old_left_revision &&
            sculpted.chunk_revision(sculpted_right.x, sculpted_right.z) >
            old_right_revision,
        "editing a shared edge did not invalidate both chunk meshes");

    check(terrain.intersect_ray({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f, hit),
        "downward ray did not intersect flat terrain");
    check(std::abs(hit.y) < 0.001f,
        "ray intersection returned the wrong ground height");
    check(!terrain.intersect_ray({0.0f, 5.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 20.0f, hit),
        "upward ray incorrectly intersected flat terrain");
}

void test_legacy_heightmap_loads_into_center_of_expanded_world()
{
    using namespace mmo::game::world;
    std::ostringstream serialized;
    serialized << "MMO_TERRAIN 1 97 97\n";
    for (int index = 0; index < 97 * 97; ++index) {
        serialized << "2\n";
    }
    std::istringstream input(serialized.str());
    Terrain terrain;
    terrain.read(input);
    check(std::abs(terrain.height_at({0.0f, 0.0f}) - 2.0f) < 0.0001f,
        "legacy heightmap was not centered in the expanded terrain bounds");
    check(terrain.height_at({-180.0f, 0.0f}) == 0.0f,
        "legacy heightmap unexpectedly filled new terrain outside its old bounds");
}

void test_material_paint_blends_and_updates_chunk_revision()
{
    using namespace mmo::game::world;
    Terrain terrain;
    const int grid_x = static_cast<int>(
        (0.0f + Terrain::kHalfExtent) / Terrain::kSpacing);
    const int grid_z = grid_x;
    const int chunk_x = grid_x / Terrain::kChunkCells;
    const int chunk_z = grid_z / Terrain::kChunkCells;
    const std::uint64_t revision_before = terrain.chunk_revision(chunk_x, chunk_z);

    check(terrain.paint_material(
            {0.0f, 0.0f}, 4.0f, 0.5f, 1.0f, TerrainMaterial::dirt),
        "material brush did not change the terrain");
    const glm::vec4 weights = terrain.material_weights_at({0.0f, 0.0f});
    check(std::abs(weights.x - 0.5f) < 0.0001f &&
            std::abs(weights.y - 0.5f) < 0.0001f &&
            std::abs(glm::dot(weights, glm::vec4{1.0f}) - 1.0f) < 0.0001f,
        "material brush did not preserve smooth normalized blending weights");
    check(terrain.chunk_revision(chunk_x, chunk_z) > revision_before,
        "material painting did not invalidate the changed render chunk");

    const TerrainChunk chunk = terrain.chunk_geometry(chunk_x, chunk_z);
    const bool has_non_grass_color = std::any_of(
        chunk.vertices.begin(), chunk.vertices.end(),
        [](const TerrainVertex& vertex) {
            return glm::length(vertex.color - glm::vec3{0.31f, 0.48f, 0.23f}) > 0.01f;
        });
    check(has_non_grass_color,
        "terrain chunk geometry did not encode its blended material colors");
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
        check(source.paint_material(
                {-3.0f, 2.0f}, 4.0f, 2.0f, 1.0f, TerrainMaterial::rock),
            "test setup did not paint a terrain material");
        source.save(path);

        Terrain loaded;
        loaded.load(path);
        check(std::abs(loaded.height_at({-3.0f, 2.0f}) -
                source.height_at({-3.0f, 2.0f})) < 0.00001f,
            "saved terrain did not load with the same heights");
        check(loaded.material_weights_at({-3.0f, 2.0f}).z > 0.99f,
            "saved terrain did not preserve its painted material weights");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw;
    }
    std::filesystem::remove(path);
}

void test_region_storage_evicts_and_reloads_cpu_pages()
{
    using namespace mmo::game::world;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "mmo-terrain-region-cache-test";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    try {
        Terrain terrain;
        terrain.configure_region_storage(directory, 2);
        check(terrain.apply_brush(
                {-175.0f, 0.0f}, 2.0f, 3.0f, 1.0f, TerrainBrush::raise),
            "first streamed region edit did not modify terrain");
        check(terrain.paint_material(
                {-175.0f, 0.0f}, 2.0f, 3.0f, 1.0f, TerrainMaterial::dirt),
            "first streamed region material edit did not modify terrain");
        const float west_height = terrain.height_at({-175.0f, 0.0f});
        check(terrain.apply_brush(
                {175.0f, 0.0f}, 2.0f, 4.0f, 1.0f, TerrainBrush::raise),
            "second streamed region edit did not modify terrain");
        check(terrain.resident_region_count() <= terrain.region_storage_limit(),
            "terrain exceeded its configured resident CPU region limit");
        check(std::filesystem::exists(directory / "region_1_12.mmohm"),
            "evicted terrain region was not persisted to its backing store");
        check(std::abs(terrain.height_at({-175.0f, 0.0f}) - west_height) < 0.0001f,
            "evicted terrain region did not reload its edited height samples");
        check(terrain.material_weights_at({-175.0f, 0.0f}).y > 0.99f,
            "evicted terrain region did not reload its material weights");
        check(terrain.resident_region_count() <= terrain.region_storage_limit(),
            "reloading terrain region exceeded the CPU residency limit");

        Terrain reopened;
        reopened.configure_region_storage(directory, 2);
        check(std::abs(reopened.height_at({-175.0f, 0.0f}) - west_height) < 0.0001f &&
                std::abs(reopened.height_at({175.0f, 0.0f}) - 4.0f) < 0.0001f &&
                reopened.material_weights_at({-175.0f, 0.0f}).y > 0.99f,
            "terrain region backing store did not survive reopening");
        check(reopened.resident_region_count() <= reopened.region_storage_limit(),
            "reopened terrain exceeded its configured CPU residency limit");
    } catch (...) {
        std::filesystem::remove_all(directory, ignored);
        throw;
    }
    std::filesystem::remove_all(directory, ignored);
}

}

int main()
{
    try {
        test_brush_shapes_and_height_sampling();
        test_mesh_and_ray_intersection();
        test_legacy_heightmap_loads_into_center_of_expanded_world();
        test_material_paint_blends_and_updates_chunk_revision();
        test_save_load_round_trip();
        test_region_storage_evicts_and_reloads_cpu_pages();
        std::cout << "Terrain tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Terrain test failed: " << error.what() << '\n';
        return 1;
    }
}
