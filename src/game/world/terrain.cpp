#include "game/world/terrain.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace mmo::game::world {
namespace {

constexpr char kFileSignature[] = "MMO_TERRAIN";
constexpr int kFileVersion = 3;
constexpr int kLegacyVerticesPerSide = 97;
constexpr char kRegionSignature[] = "MMO_HEIGHT_REGION";
constexpr int kRegionVersion = 2;
constexpr float kMinimumHeight = -100.0f;
constexpr float kMaximumHeight = 100.0f;
constexpr float kRayStep = 0.25f;
constexpr glm::vec3 kMaterialColors[]{
    {0.31f, 0.48f, 0.23f},
    {0.43f, 0.29f, 0.17f},
    {0.43f, 0.45f, 0.46f},
    {0.72f, 0.63f, 0.39f},
};

std::size_t grid_index(int x, int z)
{
    return static_cast<std::size_t>(z * Terrain::kVerticesPerSide + x);
}

std::size_t chunk_index(int x, int z)
{
    return static_cast<std::size_t>(z * Terrain::kChunksPerSide + x);
}

std::size_t region_sample_index(int x, int z, int width)
{
    return static_cast<std::size_t>(z * width + x);
}

}

Terrain::Terrain()
    : chunk_revisions_(static_cast<std::size_t>(kChunksPerSide * kChunksPerSide), 1)
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
    float flatten_height,
    TerrainMaterial material)
{
    if (!contains(center) || !std::isfinite(radius) || radius <= 0.0f ||
        radius > kHalfExtent * 2.0f ||
        !std::isfinite(strength) || !std::isfinite(delta_seconds) ||
        delta_seconds < 0.0f || !std::isfinite(flatten_height) ||
        (brush != TerrainBrush::raise && brush != TerrainBrush::lower &&
            brush != TerrainBrush::flatten && brush != TerrainBrush::paint_material)) {
        throw std::invalid_argument("terrain brush parameters must be finite and valid");
    }
    if (brush == TerrainBrush::paint_material) {
        return paint_material(center, radius, strength, delta_seconds, material);
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
    int changed_min_x = kCells;
    int changed_min_z = kCells;
    int changed_max_x = 0;
    int changed_max_z = 0;

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
            const float height = height_at_grid(x, z);
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
                set_height_at_grid(x, z, next_height);
                changed = true;
                changed_min_x = std::min(changed_min_x, x);
                changed_min_z = std::min(changed_min_z, z);
                changed_max_x = std::max(changed_max_x, x);
                changed_max_z = std::max(changed_max_z, z);
            }
        }
    }

    if (changed) {
        ++revision_;
        mark_chunks_changed(changed_min_x, changed_min_z, changed_max_x, changed_max_z);
    }
    return changed;
}

bool Terrain::paint_material(
    glm::vec2 center,
    float radius,
    float strength,
    float delta_seconds,
    TerrainMaterial material)
{
    if (!contains(center) || !std::isfinite(radius) || radius <= 0.0f ||
        radius > kHalfExtent * 2.0f || !std::isfinite(strength) ||
        !std::isfinite(delta_seconds) || delta_seconds < 0.0f ||
        material < TerrainMaterial::grass || material >= TerrainMaterial::count) {
        throw std::invalid_argument("terrain material brush parameters are invalid");
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
    const int material_index = static_cast<int>(material);
    bool changed = false;
    int changed_min_x = kCells;
    int changed_min_z = kCells;
    int changed_max_x = 0;
    int changed_max_z = 0;

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
            const float blend = std::clamp(amount * falloff * falloff, 0.0f, 1.0f);
            glm::vec4 weights = material_weights_at_grid(x, z);
            const glm::vec4 previous = weights;
            for (int index = 0; index < static_cast<int>(TerrainMaterial::count); ++index) {
                if (index == material_index) {
                    weights[index] += (1.0f - weights[index]) * blend;
                } else {
                    weights[index] *= 1.0f - blend;
                }
            }
            if (weights != previous) {
                set_material_weights_at_grid(x, z, weights);
                changed = true;
                changed_min_x = std::min(changed_min_x, x);
                changed_min_z = std::min(changed_min_z, z);
                changed_max_x = std::max(changed_max_x, x);
                changed_max_z = std::max(changed_max_z, z);
            }
        }
    }

    if (changed) {
        ++revision_;
        mark_chunks_changed(changed_min_x, changed_min_z, changed_max_x, changed_max_z);
    }
    return changed;
}

glm::vec4 Terrain::material_weights_at(glm::vec2 position) const
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
    const glm::vec4 w00 = material_weights_at_grid(x, z);
    const glm::vec4 w01 = material_weights_at_grid(x, z + 1);
    const glm::vec4 w10 = material_weights_at_grid(x + 1, z);
    const glm::vec4 w11 = material_weights_at_grid(x + 1, z + 1);
    if (tz >= tx) {
        return w00 * (1.0f - tz) + w01 * (tz - tx) + w11 * tx;
    }
    return w00 * (1.0f - tx) + w11 * tz + w10 * (tx - tz);
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

std::vector<TerrainChunk> Terrain::chunks_near(
    glm::vec2 center,
    int radius_in_chunks) const
{
    const std::vector<TerrainChunkCoordinate> coordinates =
        chunk_coordinates_near(center, radius_in_chunks);
    std::vector<TerrainChunk> result;
    result.reserve(coordinates.size());
    for (const TerrainChunkCoordinate& coordinate : coordinates) {
        result.push_back(chunk_geometry(coordinate.x, coordinate.z));
    }
    return result;
}

std::vector<TerrainChunkCoordinate> Terrain::chunk_coordinates_near(
    glm::vec2 center,
    int radius_in_chunks) const
{
    if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
        radius_in_chunks < 0) {
        throw std::invalid_argument("terrain chunk streaming parameters are invalid");
    }

    const float grid_x = std::clamp(
        (center.x + kHalfExtent) / kSpacing, 0.0f, static_cast<float>(kCells));
    const float grid_z = std::clamp(
        (center.y + kHalfExtent) / kSpacing, 0.0f, static_cast<float>(kCells));
    const int center_grid_x = static_cast<int>(grid_x);
    const int center_grid_z = static_cast<int>(grid_z);
    const int center_chunk_x = std::min(center_grid_x / kChunkCells, kChunksPerSide - 1);
    const int center_chunk_z = std::min(center_grid_z / kChunkCells, kChunksPerSide - 1);
    const int radius = std::min(radius_in_chunks, kChunksPerSide);
    const int min_chunk_x = std::max(0, center_chunk_x - radius);
    const int max_chunk_x = std::min(kChunksPerSide - 1, center_chunk_x + radius);
    const int min_chunk_z = std::max(0, center_chunk_z - radius);
    const int max_chunk_z = std::min(kChunksPerSide - 1, center_chunk_z + radius);

    std::vector<TerrainChunkCoordinate> result;
    result.reserve(static_cast<std::size_t>(
        (max_chunk_x - min_chunk_x + 1) * (max_chunk_z - min_chunk_z + 1)));
    for (int chunk_z = min_chunk_z; chunk_z <= max_chunk_z; ++chunk_z) {
        for (int chunk_x = min_chunk_x; chunk_x <= max_chunk_x; ++chunk_x) {
            result.push_back({chunk_x, chunk_z});
        }
    }
    return result;
}

TerrainChunk Terrain::chunk_geometry(int x, int z) const
{
    TerrainChunk chunk{x, z, chunk_revision(x, z), {}};
    chunk.vertices.reserve(static_cast<std::size_t>(kChunkCells * kChunkCells * 6));
    const auto append = [this, &chunk](int grid_x, int grid_z) {
        chunk.vertices.push_back({
            {
                static_cast<float>(grid_x) * kSpacing - kHalfExtent,
                height_at_grid(grid_x, grid_z),
                static_cast<float>(grid_z) * kSpacing - kHalfExtent,
            },
            normal_at(grid_x, grid_z),
            material_color_at_grid(grid_x, grid_z),
        });
    };
    const int start_x = x * kChunkCells;
    const int start_z = z * kChunkCells;
    for (int grid_z = start_z; grid_z < start_z + kChunkCells; ++grid_z) {
        for (int grid_x = start_x; grid_x < start_x + kChunkCells; ++grid_x) {
            append(grid_x, grid_z);
            append(grid_x, grid_z + 1);
            append(grid_x + 1, grid_z + 1);
            append(grid_x, grid_z);
            append(grid_x + 1, grid_z + 1);
            append(grid_x + 1, grid_z);
        }
    }
    return chunk;
}

void Terrain::preload_regions_near(glm::vec2 center, int radius_in_chunks) const
{
    const std::vector<TerrainChunkCoordinate> coordinates =
        chunk_coordinates_near(center, radius_in_chunks);
    for (const TerrainChunkCoordinate& coordinate : coordinates) {
        (void)region_page(coordinate.x, coordinate.z);
    }
}

std::uint64_t Terrain::chunk_revision(int x, int z) const
{
    if (x < 0 || x >= kChunksPerSide || z < 0 || z >= kChunksPerSide) {
        throw std::out_of_range("terrain chunk coordinates are outside the world");
    }
    return chunk_revisions_[chunk_index(x, z)];
}

std::uint64_t Terrain::revision() const
{
    return revision_;
}

void Terrain::configure_region_storage(
    std::filesystem::path directory,
    std::size_t resident_region_limit)
{
    if (directory.empty() || resident_region_limit == 0) {
        throw std::invalid_argument(
            "terrain region storage requires a directory and positive cache limit");
    }
    if (!region_storage_directory_.empty() &&
        region_storage_directory_ != directory) {
        throw std::invalid_argument(
            "terrain region storage cannot be changed after configuration");
    }
    std::filesystem::create_directories(directory);
    region_storage_directory_ = std::move(directory);
    resident_region_limit_ = resident_region_limit;
    for (auto& [key, page] : resident_regions_) {
        const int x = static_cast<int>(key % kChunksPerSide);
        const int z = static_cast<int>(key / kChunksPerSide);
        flush_region(x, z, page);
    }
    evict_regions();
}

std::size_t Terrain::resident_region_count() const
{
    return resident_regions_.size();
}

std::size_t Terrain::region_storage_limit() const
{
    return resident_region_limit_;
}

void Terrain::invalidate_render_chunks()
{
    ++revision_;
    for (std::uint64_t& chunk_revision : chunk_revisions_) {
        ++chunk_revision;
    }
}

void Terrain::mark_chunks_changed(int min_x, int min_z, int max_x, int max_z)
{
    const int first_chunk_x = std::max(0, (min_x - 1) / kChunkCells);
    const int first_chunk_z = std::max(0, (min_z - 1) / kChunkCells);
    const int last_chunk_x = std::min(kChunksPerSide - 1, (max_x + 1) / kChunkCells);
    const int last_chunk_z = std::min(kChunksPerSide - 1, (max_z + 1) / kChunkCells);
    for (int z = first_chunk_z; z <= last_chunk_z; ++z) {
        for (int x = first_chunk_x; x <= last_chunk_x; ++x) {
            ++chunk_revisions_[chunk_index(x, z)];
        }
    }
}

void Terrain::write(std::ostream& output) const
{
    output << kFileSignature << ' ' << kFileVersion << ' '
        << kVerticesPerSide << ' ' << kVerticesPerSide << '\n'
        << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (int z = 0; z <= kCells; ++z) {
        for (int x = 0; x <= kCells; ++x) {
            output << height_at_grid(x, z) << '\n';
        }
    }
    for (int z = 0; z <= kCells; ++z) {
        for (int x = 0; x <= kCells; ++x) {
            const glm::vec4 weights = material_weights_at_grid(x, z);
            output << weights.x << ' ' << weights.y << ' '
                << weights.z << ' ' << weights.w << '\n';
        }
    }
    if (!output) {
        throw std::runtime_error("failed while writing terrain data");
    }
    if (!region_storage_directory_.empty()) {
        for (auto& [key, page] : resident_regions_) {
            const int x = static_cast<int>(key % kChunksPerSide);
            const int z = static_cast<int>(key / kChunksPerSide);
            flush_region(x, z, page);
        }
    }
}

void Terrain::read(std::istream& input)
{
    std::string signature;
    int version = 0;
    int width = 0;
    int height = 0;
    if (!(input >> signature >> version >> width >> height) ||
        signature != kFileSignature ||
        !(((version == kFileVersion || version == 2) &&
              width == kVerticesPerSide && height == kVerticesPerSide) ||
            (version == 1 && width == kLegacyVerticesPerSide &&
                height == kLegacyVerticesPerSide))) {
        throw std::runtime_error("terrain data has an unsupported header");
    }

    std::vector<float> loaded_heights(
        static_cast<std::size_t>(kVerticesPerSide * kVerticesPerSide), 0.0f);
    std::vector<glm::vec4> loaded_materials(
        static_cast<std::size_t>(kVerticesPerSide * kVerticesPerSide),
        {1.0f, 0.0f, 0.0f, 0.0f});
    if (version == 1) {
        const int offset = (kCells - (kLegacyVerticesPerSide - 1)) / 2;
        for (int z = 0; z < kLegacyVerticesPerSide; ++z) {
            for (int x = 0; x < kLegacyVerticesPerSide; ++x) {
                float sample = 0.0f;
                if (!(input >> sample) || !std::isfinite(sample) ||
                    sample < kMinimumHeight || sample > kMaximumHeight) {
                    throw std::runtime_error(
                        "terrain data contains invalid or incomplete heights");
                }
                loaded_heights[grid_index(x + offset, z + offset)] = sample;
            }
        }
    } else {
        for (float& sample : loaded_heights) {
            if (!(input >> sample) || !std::isfinite(sample) ||
                sample < kMinimumHeight || sample > kMaximumHeight) {
                throw std::runtime_error(
                    "terrain data contains invalid or incomplete heights");
            }
        }
        if (version >= 3) {
            for (glm::vec4& weights : loaded_materials) {
                if (!(input >> weights.x >> weights.y >> weights.z >> weights.w) ||
                    !std::isfinite(weights.x) || !std::isfinite(weights.y) ||
                    !std::isfinite(weights.z) || !std::isfinite(weights.w) ||
                    glm::any(glm::lessThan(weights, glm::vec4{0.0f})) ||
                    glm::any(glm::greaterThan(weights, glm::vec4{1.0f})) ||
                    std::abs(glm::dot(weights, glm::vec4{1.0f}) - 1.0f) > 0.001f) {
                    throw std::runtime_error(
                        "terrain data contains invalid material weights");
                }
            }
        }
    }
    resident_regions_.clear();
    region_access_clock_ = 0;
    for (int region_z = 0; region_z < kChunksPerSide; ++region_z) {
        for (int region_x = 0; region_x < kChunksPerSide; ++region_x) {
            const int page_width = region_x == kChunksPerSide - 1
                ? kCells - region_x * kChunkCells + 1 : kChunkCells;
            const int page_height = region_z == kChunksPerSide - 1
                ? kCells - region_z * kChunkCells + 1 : kChunkCells;
            RegionPage page{
                page_width, page_height,
                std::vector<float>(static_cast<std::size_t>(page_width * page_height)),
                std::vector<glm::vec4>(
                    static_cast<std::size_t>(page_width * page_height),
                    {1.0f, 0.0f, 0.0f, 0.0f}),
                ++region_access_clock_, true,
            };
            for (int local_z = 0; local_z < page_height; ++local_z) {
                for (int local_x = 0; local_x < page_width; ++local_x) {
                    const int grid_x = region_x * kChunkCells + local_x;
                    const int grid_z = region_z * kChunkCells + local_z;
                    page.samples[region_sample_index(local_x, local_z, page_width)] =
                        loaded_heights[grid_index(grid_x, grid_z)];
                    page.materials[region_sample_index(local_x, local_z, page_width)] =
                        loaded_materials[grid_index(grid_x, grid_z)];
                }
            }
            const std::size_t key = chunk_index(region_x, region_z);
            resident_regions_.emplace(key, std::move(page));
            evict_regions();
        }
    }
    if (!region_storage_directory_.empty()) {
        for (auto& [key, page] : resident_regions_) {
            flush_region(
                static_cast<int>(key % kChunksPerSide),
                static_cast<int>(key / kChunksPerSide), page);
        }
    }
    ++revision_;
    for (std::uint64_t& chunk_revision : chunk_revisions_) {
        ++chunk_revision;
    }
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

    write(output);
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

    Terrain loaded;
    loaded.read(input);
    std::string trailing_data;
    if (input >> trailing_data) {
        throw std::runtime_error("terrain file contains unexpected trailing data: " +
            path.string());
    }

    if (!region_storage_directory_.empty()) {
        loaded.configure_region_storage(
            region_storage_directory_, resident_region_limit_);
    }
    *this = std::move(loaded);
    ++revision_;
}

float Terrain::height_at_grid(int x, int z) const
{
    if (x < 0 || x > kCells || z < 0 || z > kCells) {
        throw std::out_of_range("terrain sample is outside its bounds");
    }
    const int region_x = std::min(x / kChunkCells, kChunksPerSide - 1);
    const int region_z = std::min(z / kChunkCells, kChunksPerSide - 1);
    const RegionPage& page = region_page(region_x, region_z);
    const int local_x = x - region_x * kChunkCells;
    const int local_z = z - region_z * kChunkCells;
    return page.samples[region_sample_index(local_x, local_z, page.width)];
}

glm::vec4 Terrain::material_weights_at_grid(int x, int z) const
{
    if (x < 0 || x > kCells || z < 0 || z > kCells) {
        throw std::out_of_range("terrain material sample is outside its bounds");
    }
    const int region_x = std::min(x / kChunkCells, kChunksPerSide - 1);
    const int region_z = std::min(z / kChunkCells, kChunksPerSide - 1);
    const RegionPage& page = region_page(region_x, region_z);
    const int local_x = x - region_x * kChunkCells;
    const int local_z = z - region_z * kChunkCells;
    return page.materials[region_sample_index(local_x, local_z, page.width)];
}

void Terrain::set_material_weights_at_grid(int x, int z, glm::vec4 weights)
{
    const int region_x = std::min(x / kChunkCells, kChunksPerSide - 1);
    const int region_z = std::min(z / kChunkCells, kChunksPerSide - 1);
    RegionPage& page = region_page(region_x, region_z);
    const int local_x = x - region_x * kChunkCells;
    const int local_z = z - region_z * kChunkCells;
    page.materials[region_sample_index(local_x, local_z, page.width)] = weights;
    page.dirty = true;
}

glm::vec3 Terrain::material_color_at_grid(int x, int z) const
{
    const glm::vec4 weights = material_weights_at_grid(x, z);
    return kMaterialColors[0] * weights.x +
        kMaterialColors[1] * weights.y +
        kMaterialColors[2] * weights.z +
        kMaterialColors[3] * weights.w;
}

void Terrain::set_height_at_grid(int x, int z, float height)
{
    const int region_x = std::min(x / kChunkCells, kChunksPerSide - 1);
    const int region_z = std::min(z / kChunkCells, kChunksPerSide - 1);
    RegionPage& page = region_page(region_x, region_z);
    const int local_x = x - region_x * kChunkCells;
    const int local_z = z - region_z * kChunkCells;
    page.samples[region_sample_index(local_x, local_z, page.width)] = height;
    page.dirty = true;
}

Terrain::RegionPage& Terrain::region_page(int x, int z) const
{
    if (x < 0 || x >= kChunksPerSide || z < 0 || z >= kChunksPerSide) {
        throw std::out_of_range("terrain region coordinates are outside the world");
    }
    const std::size_t key = chunk_index(x, z);
    const auto found = resident_regions_.find(key);
    if (found != resident_regions_.end()) {
        found->second.last_access = ++region_access_clock_;
        return found->second;
    }

    const int width = x == kChunksPerSide - 1 ? kCells - x * kChunkCells + 1
                                               : kChunkCells;
    const int height = z == kChunksPerSide - 1 ? kCells - z * kChunkCells + 1
                                                : kChunkCells;
    RegionPage page{
        width, height,
        std::vector<float>(static_cast<std::size_t>(width * height), 0.0f),
        std::vector<glm::vec4>(static_cast<std::size_t>(width * height),
            {1.0f, 0.0f, 0.0f, 0.0f}),
        ++region_access_clock_, false,
    };
    const std::filesystem::path path = region_path(x, z);
    if (!region_storage_directory_.empty() && std::filesystem::exists(path)) {
        std::ifstream input(path);
        std::string signature;
        int version = 0;
        int stored_x = -1;
        int stored_z = -1;
        int stored_width = 0;
        int stored_height = 0;
        if (!(input >> signature >> version >> stored_x >> stored_z >>
                stored_width >> stored_height) ||
            signature != kRegionSignature ||
            (version != 1 && version != kRegionVersion) ||
            stored_x != x || stored_z != z ||
            stored_width != width || stored_height != height) {
            throw std::runtime_error("terrain region file has an unsupported header: " +
                path.string());
        }
        for (std::size_t index = 0; index < page.samples.size(); ++index) {
            float& sample = page.samples[index];
            if (!(input >> sample) || !std::isfinite(sample) ||
                sample < kMinimumHeight || sample > kMaximumHeight) {
                throw std::runtime_error("terrain region file contains invalid heights: " +
                    path.string());
            }
            if (version >= 2) {
                glm::vec4& weights = page.materials[index];
                if (!(input >> weights.x >> weights.y >> weights.z >> weights.w) ||
                    !std::isfinite(weights.x) || !std::isfinite(weights.y) ||
                    !std::isfinite(weights.z) || !std::isfinite(weights.w) ||
                    glm::any(glm::lessThan(weights, glm::vec4{0.0f})) ||
                    glm::any(glm::greaterThan(weights, glm::vec4{1.0f})) ||
                    std::abs(glm::dot(weights, glm::vec4{1.0f}) - 1.0f) > 0.001f) {
                    throw std::runtime_error(
                        "terrain region contains invalid material weights: " +
                        path.string());
                }
            }
        }
        std::string trailing;
        if (input >> trailing) {
            throw std::runtime_error("terrain region file contains trailing data: " +
                path.string());
        }
    }

    auto [inserted, created] = resident_regions_.emplace(key, std::move(page));
    if (!created) {
        throw std::logic_error("terrain region cache insertion unexpectedly failed");
    }
    evict_regions();
    const auto resident = resident_regions_.find(key);
    if (resident == resident_regions_.end()) {
        throw std::logic_error("terrain region cache evicted the page being accessed");
    }
    return resident->second;
}

std::filesystem::path Terrain::region_path(int x, int z) const
{
    if (region_storage_directory_.empty()) {
        return {};
    }
    return region_storage_directory_ /
        ("region_" + std::to_string(x) + "_" + std::to_string(z) + ".mmohm");
}

void Terrain::flush_region(int x, int z, RegionPage& page) const
{
    if (!page.dirty || region_storage_directory_.empty()) {
        return;
    }
    const std::filesystem::path path = region_path(x, z);
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not open terrain region for writing: " +
            path.string());
    }
    output << kRegionSignature << ' ' << kRegionVersion << ' '
        << x << ' ' << z << ' ' << page.width << ' ' << page.height << '\n'
        << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (std::size_t index = 0; index < page.samples.size(); ++index) {
        const glm::vec4 weights = page.materials[index];
        output << page.samples[index] << ' '
            << weights.x << ' ' << weights.y << ' '
            << weights.z << ' ' << weights.w << '\n';
    }
    output.flush();
    if (!output) {
        throw std::runtime_error("failed while writing terrain region: " + path.string());
    }
    page.dirty = false;
}

void Terrain::evict_regions() const
{
    if (region_storage_directory_.empty()) {
        return;
    }
    while (resident_regions_.size() > resident_region_limit_) {
        auto oldest = resident_regions_.end();
        for (auto candidate = resident_regions_.begin();
            candidate != resident_regions_.end(); ++candidate) {
            if (oldest == resident_regions_.end() ||
                candidate->second.last_access < oldest->second.last_access) {
                oldest = candidate;
            }
        }
        const int x = static_cast<int>(oldest->first % kChunksPerSide);
        const int z = static_cast<int>(oldest->first / kChunksPerSide);
        flush_region(x, z, oldest->second);
        resident_regions_.erase(oldest);
    }
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
