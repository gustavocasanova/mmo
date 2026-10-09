#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <unordered_map>
#include <vector>

namespace mmo::game::world {

enum class TerrainBrush {
    raise,
    lower,
    flatten,
    paint_material,
};

enum class TerrainMaterial : std::uint8_t {
    grass,
    dirt,
    rock,
    sand,
    count,
};

struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

struct TerrainChunk {
    int x;
    int z;
    std::uint64_t revision;
    std::vector<TerrainVertex> vertices;
};

struct TerrainChunkCoordinate {
    int x;
    int z;
};

class Terrain {
public:
    static constexpr int kCells = 384;
    static constexpr int kVerticesPerSide = kCells + 1;
    static constexpr int kChunkCells = 16;
    static constexpr int kChunksPerSide = kCells / kChunkCells;
    static_assert(kCells % kChunkCells == 0);
    static constexpr float kSpacing = 1.0f;
    static constexpr float kHalfExtent = kCells * kSpacing * 0.5f;

    Terrain();

    [[nodiscard]] bool contains(glm::vec2 position) const;
    [[nodiscard]] float height_at(glm::vec2 position) const;
    [[nodiscard]] bool apply_brush(
        glm::vec2 center,
        float radius,
        float strength,
        float delta_seconds,
        TerrainBrush brush,
        float flatten_height = 0.0f,
        TerrainMaterial material = TerrainMaterial::grass);
    [[nodiscard]] bool paint_material(
        glm::vec2 center,
        float radius,
        float strength,
        float delta_seconds,
        TerrainMaterial material);
    [[nodiscard]] glm::vec4 material_weights_at(glm::vec2 position) const;
    [[nodiscard]] bool intersect_ray(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance,
        glm::vec3& hit) const;
    [[nodiscard]] std::vector<TerrainChunk> chunks_near(
        glm::vec2 center,
        int radius_in_chunks) const;
    [[nodiscard]] std::vector<TerrainChunkCoordinate> chunk_coordinates_near(
        glm::vec2 center,
        int radius_in_chunks) const;
    [[nodiscard]] TerrainChunk chunk_geometry(int x, int z) const;
    void preload_regions_near(glm::vec2 center, int radius_in_chunks) const;
    [[nodiscard]] std::uint64_t chunk_revision(int x, int z) const;
    [[nodiscard]] std::uint64_t revision() const;
    void invalidate_render_chunks();
    void configure_region_storage(
        std::filesystem::path directory,
        std::size_t resident_region_limit = 128);
    [[nodiscard]] std::size_t resident_region_count() const;
    [[nodiscard]] std::size_t region_storage_limit() const;

    void write(std::ostream& output) const;
    void read(std::istream& input);
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);

private:
    [[nodiscard]] float height_at_grid(int x, int z) const;
    void set_height_at_grid(int x, int z, float height);
    [[nodiscard]] glm::vec4 material_weights_at_grid(int x, int z) const;
    void set_material_weights_at_grid(int x, int z, glm::vec4 weights);
    [[nodiscard]] glm::vec3 material_color_at_grid(int x, int z) const;
    [[nodiscard]] glm::vec3 normal_at(int x, int z) const;
    void mark_chunks_changed(int min_x, int min_z, int max_x, int max_z);
    struct RegionPage {
        int width;
        int height;
        std::vector<float> samples;
        std::vector<glm::vec4> materials;
        std::uint64_t last_access;
        bool dirty;
    };
    [[nodiscard]] RegionPage& region_page(int x, int z) const;
    [[nodiscard]] std::filesystem::path region_path(int x, int z) const;
    void flush_region(int x, int z, RegionPage& page) const;
    void evict_regions() const;

    std::vector<std::uint64_t> chunk_revisions_;
    std::uint64_t revision_ = 0;
    std::filesystem::path region_storage_directory_;
    std::size_t resident_region_limit_ = 128;
    mutable std::unordered_map<std::size_t, RegionPage> resident_regions_;
    mutable std::uint64_t region_access_clock_ = 0;
};

}
