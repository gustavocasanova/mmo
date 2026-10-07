#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mmo::game::world {

enum class TerrainBrush {
    raise,
    lower,
    flatten,
};

struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

class Terrain {
public:
    static constexpr int kCells = 96;
    static constexpr int kVerticesPerSide = kCells + 1;
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
        float flatten_height = 0.0f);
    [[nodiscard]] bool intersect_ray(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance,
        glm::vec3& hit) const;
    [[nodiscard]] std::vector<TerrainVertex> vertices() const;
    [[nodiscard]] std::uint64_t revision() const;

    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);

private:
    [[nodiscard]] float height_at_grid(int x, int z) const;
    [[nodiscard]] glm::vec3 normal_at(int x, int z) const;

    std::vector<float> heights_;
    std::uint64_t revision_ = 0;
};

}
