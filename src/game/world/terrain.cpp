#include "game/world/terrain.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>

namespace mmo::game::world {
namespace {

constexpr char kFileSignature[] = "MMO_TERRAIN";
constexpr int kFileVersion = 1;
constexpr float kMinimumHeight = -100.0f;
constexpr float kMaximumHeight = 100.0f;
constexpr float kRayStep = 0.25f;

std::size_t grid_index(int x, int z)
{
    return static_cast<std::size_t>(z * Terrain::kVerticesPerSide + x);
}

}

Terrain::Terrain()
    : heights_(static_cast<std::size_t>(kVerticesPerSide * kVerticesPerSide), 0.0f)
{
}

bool Terrain::contains(glm::vec2 position) const
{
    return std::isfinite(position.x) && std::isfinite(position.y) &&
        position.x >= -kHalfExtent && position.x <= kHalfExtent &&
        position.y >= -kHalfExtent && position.y <= kHalfExtent;
}

float Terrain::height_at(glm::vec2 position) const
{
    if (!contains(position)) {
        throw std::out_of_range("terrain position is outside its bounds");
    }

    const float grid_x = (position.x + kHalfExtent) / kSpacing;
    const float grid_z = (position.y + kHalfExtent) / kSpacing;
    const int x = std::min(static_cast<int>(grid_x), kCells - 1);
    const int z = std::min(static_cast<int>(grid_z), kCells - 1);
    const float tx = grid_x - static_cast<float>(x);
    const float tz = grid_z - static_cast<float>(z);
    const float h00 = height_at_grid(x, z);
    const float h01 = height_at_grid(x, z + 1);
    const float h10 = height_at_grid(x + 1, z);
    const float h11 = height_at_grid(x + 1, z + 1);

    if (tz >= tx) {
        return h00 * (1.0f - tz) + h01 * (tz - tx) + h11 * tx;
    }
    return h00 * (1.0f - tx) + h11 * tz + h10 * (tx - tz);
}

bool Terrain::apply_brush(
    glm::vec2 center,
    float radius,
    float strength,
    float delta_seconds,
    TerrainBrush brush,
    float flatten_height)
{
    if (!contains(center) || !std::isfinite(radius) || radius <= 0.0f ||
        radius > kHalfExtent * 2.0f ||
        !std::isfinite(strength) || !std::isfinite(delta_seconds) ||
        delta_seconds < 0.0f || !std::isfinite(flatten_height) ||
        (brush != TerrainBrush::raise && brush != TerrainBrush::lower &&
            brush != TerrainBrush::flatten)) {
        throw std::invalid_argument("terrain brush parameters must be finite and valid");
    }

    const float amount = std::abs(strength) * delta_seconds;
    if (amount <= 0.0f) {
        return false;
    }

    const int min_x = std::max(0, static_cast<int>(
        std::floor((center.x - radius + kHalfExtent) / kSpacing)));
    const int max_x = std::min(kCells, static_cast<int>(
        std::ceil((center.x + radius + kHalfExtent) / kSpacing)));
    const int min_z = std::max(0, static_cast<int>(
        std::floor((center.y - radius + kHalfExtent) / kSpacing)));
    const int max_z = std::min(kCells, static_cast<int>(
        std::ceil((center.y + radius + kHalfExtent) / kSpacing)));
    bool changed = false;

    for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
            const glm::vec2 sample{
                static_cast<float>(x) * kSpacing - kHalfExtent,
                static_cast<float>(z) * kSpacing - kHalfExtent,
            };
            const float distance = glm::length(sample - center);
            if (distance >= radius) {
                continue;
            }
            const float falloff = 1.0f - distance / radius;
            float& height = heights_[grid_index(x, z)];
            float next_height = height;
            if (brush == TerrainBrush::raise || brush == TerrainBrush::lower) {
                const float direction = brush == TerrainBrush::raise ? 1.0f : -1.0f;
                next_height += direction * amount * falloff * falloff;
            } else {
                const float blend = std::clamp(amount * falloff * falloff, 0.0f, 1.0f);
                next_height += (flatten_height - height) * blend;
            }
            next_height = std::clamp(next_height, kMinimumHeight, kMaximumHeight);
            if (next_height != height) {
                height = next_height;
                changed = true;
            }
        }
    }

    if (changed) {
        ++revision_;
    }
    return changed;
}

bool Terrain::intersect_ray(
    glm::vec3 origin,
    glm::vec3 direction,
    float max_distance,
    glm::vec3& hit) const
{
    const float direction_length = glm::length(direction);
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) ||
        !std::isfinite(origin.z) || !std::isfinite(direction_length) ||
        direction_length <= 0.00001f || !std::isfinite(max_distance) ||
        max_distance <= 0.0f) {
        throw std::invalid_argument("terrain ray must have finite non-zero direction and range");
    }
    direction /= direction_length;

    float previous_distance = 0.0f;
    bool previous_below = false;
    for (float distance = 0.0f; distance <= max_distance; distance += kRayStep) {
        const glm::vec3 point = origin + direction * distance;
        const glm::vec2 horizontal{point.x, point.z};
        if (!contains(horizontal)) {
            previous_below = false;
            previous_distance = distance;
            continue;
        }

        const bool below = point.y <= height_at(horizontal);
        if (below) {
            float lower = previous_distance;
            float upper = distance;
            if (!previous_below) {
                for (int iteration = 0; iteration < 12; ++iteration) {
                    const float middle = (lower + upper) * 0.5f;
                    const glm::vec3 candidate = origin + direction * middle;
                    const glm::vec2 candidate_horizontal{candidate.x, candidate.z};
                    if (contains(candidate_horizontal) &&
                        candidate.y <= height_at(candidate_horizontal)) {
                        upper = middle;
                    } else {
                        lower = middle;
                    }
                }
            } else {
                upper = distance;
            }
            hit = origin + direction * upper;
            return true;
        }
        previous_distance = distance;
        previous_below = below;
    }
    return false;
}

std::vector<TerrainVertex> Terrain::vertices() const
{
    std::vector<TerrainVertex> result;
    result.reserve(static_cast<std::size_t>(kCells * kCells * 6));
    const auto append = [this, &result](int x, int z) {
        result.push_back({
            {
                static_cast<float>(x) * kSpacing - kHalfExtent,
                height_at_grid(x, z),
                static_cast<float>(z) * kSpacing - kHalfExtent,
            },
            normal_at(x, z),
        });
    };

    for (int z = 0; z < kCells; ++z) {
        for (int x = 0; x < kCells; ++x) {
            append(x, z);
            append(x, z + 1);
            append(x + 1, z + 1);
            append(x, z);
            append(x + 1, z + 1);
            append(x + 1, z);
        }
    }
    return result;
}

std::uint64_t Terrain::revision() const
{
    return revision_;
}

void Terrain::save(const std::filesystem::path& path) const
{
    if (path.empty()) {
        throw std::invalid_argument("terrain save path cannot be empty");
    }
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not open terrain file for writing: " + path.string());
    }

    output << kFileSignature << ' ' << kFileVersion << ' '
        << kVerticesPerSide << ' ' << kVerticesPerSide << '\n'
        << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (const float height : heights_) {
        output << height << '\n';
    }
    output.flush();
    if (!output) {
        throw std::runtime_error("failed while writing terrain file: " + path.string());
    }
}

void Terrain::load(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open terrain file: " + path.string());
    }

    std::string signature;
    int version = 0;
    int width = 0;
    int height = 0;
    if (!(input >> signature >> version >> width >> height) ||
        signature != kFileSignature || version != kFileVersion ||
        width != kVerticesPerSide || height != kVerticesPerSide) {
        throw std::runtime_error("terrain file has an unsupported header: " + path.string());
    }

    std::vector<float> loaded_heights(heights_.size());
    for (float& sample : loaded_heights) {
        if (!(input >> sample) || !std::isfinite(sample) ||
            sample < kMinimumHeight || sample > kMaximumHeight) {
            throw std::runtime_error("terrain file contains invalid or incomplete heights: " +
                path.string());
        }
    }
    std::string trailing_data;
    if (input >> trailing_data) {
        throw std::runtime_error("terrain file contains unexpected trailing data: " +
            path.string());
    }

    heights_ = std::move(loaded_heights);
    ++revision_;
}

float Terrain::height_at_grid(int x, int z) const
{
    return heights_[grid_index(x, z)];
}

glm::vec3 Terrain::normal_at(int x, int z) const
{
    const int left = std::max(0, x - 1);
    const int right = std::min(kCells, x + 1);
    const int back = std::max(0, z - 1);
    const int front = std::min(kCells, z + 1);
    const float dx = (height_at_grid(right, z) - height_at_grid(left, z)) /
        (static_cast<float>(right - left) * kSpacing);
    const float dz = (height_at_grid(x, front) - height_at_grid(x, back)) /
        (static_cast<float>(front - back) * kSpacing);
    return glm::normalize(glm::vec3{-dx, 1.0f, -dz});
}

}
