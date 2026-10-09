#include "editor/world_document.hpp"

#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace mmo::editor {
namespace {

constexpr char kWorldSignature[] = "MMO_WORLD";
constexpr int kWorldVersion = 1;
constexpr std::size_t kMaximumObjects = 100000;

std::filesystem::path region_storage_path(const std::filesystem::path& world_path)
{
    return std::filesystem::path{world_path.string() + ".regions"};
}

void validate_objects(const std::vector<SelectableObject>& objects)
{
    (void)SelectionManager{objects};
}

}

void WorldDocument::save(
    const std::filesystem::path& path,
    const game::world::Terrain& terrain,
    const std::vector<SelectableObject>& objects)
{
    if (path.empty()) {
        throw std::invalid_argument("world document path cannot be empty");
    }
    validate_objects(objects);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not open world document for writing: " + path.string());
    }

    output << kWorldSignature << ' ' << kWorldVersion << '\n';
    terrain.write(output);
    output << objects.size() << '\n'
        << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (const SelectableObject& object : objects) {
        output << object.id << ' ' << std::quoted(object.name) << ' '
            << std::quoted(object.asset_path) << ' '
            << object.local_bounds.minimum.x << ' '
            << object.local_bounds.minimum.y << ' '
            << object.local_bounds.minimum.z << ' '
            << object.local_bounds.maximum.x << ' '
            << object.local_bounds.maximum.y << ' '
            << object.local_bounds.maximum.z << ' '
            << object.position.x << ' ' << object.position.y << ' ' << object.position.z << ' '
            << object.rotation_degrees.x << ' ' << object.rotation_degrees.y << ' '
            << object.rotation_degrees.z << ' '
            << object.scale.x << ' ' << object.scale.y << ' ' << object.scale.z << '\n';
    }
    output.flush();
    if (!output) {
        throw std::runtime_error("failed while writing world document: " + path.string());
    }
}

WorldDocumentData WorldDocument::load(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open world document: " + path.string());
    }

    std::string signature;
    int version = 0;
    if (!(input >> signature >> version) ||
        signature != kWorldSignature || version != kWorldVersion) {
        throw std::runtime_error("world document has an unsupported header: " + path.string());
    }

    WorldDocumentData result;
    result.terrain.read(input);
    std::size_t object_count = 0;
    if (!(input >> object_count) || object_count > kMaximumObjects) {
        throw std::runtime_error("world document has an invalid object count: " + path.string());
    }
    result.objects.reserve(object_count);
    for (std::size_t index = 0; index < object_count; ++index) {
        SelectableObject object{};
        if (!(input >> object.id >> std::quoted(object.name) >>
                std::quoted(object.asset_path) >>
                object.local_bounds.minimum.x >> object.local_bounds.minimum.y >>
                object.local_bounds.minimum.z >> object.local_bounds.maximum.x >>
                object.local_bounds.maximum.y >> object.local_bounds.maximum.z >>
                object.position.x >> object.position.y >> object.position.z >>
                object.rotation_degrees.x >> object.rotation_degrees.y >>
                object.rotation_degrees.z >> object.scale.x >> object.scale.y >>
                object.scale.z)) {
            throw std::runtime_error(
                "world document contains an invalid or incomplete object: " + path.string());
        }
        result.objects.push_back(std::move(object));
    }
    std::string trailing_data;
    if (input >> trailing_data) {
        throw std::runtime_error("world document contains unexpected trailing data: " +
            path.string());
    }
    validate_objects(result.objects);
    result.terrain.configure_region_storage(region_storage_path(path));
    return result;
}

}
