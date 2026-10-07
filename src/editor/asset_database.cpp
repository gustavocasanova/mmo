#include "editor/asset_database.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace mmo::editor {
namespace {

constexpr std::string_view kPrefabHeader{"MMO_PREFAB 1"};
constexpr std::string_view kModelPrefix{"model="};

bool has_extension(const std::filesystem::path& path, std::string_view extension)
{
    std::string actual = path.extension().string();
    std::transform(actual.begin(), actual.end(), actual.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return actual == extension;
}

bool is_safe_relative_path(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || !path.root_name().empty()) {
        return false;
    }
    return std::none_of(path.begin(), path.end(),
        [](const std::filesystem::path& part) { return part == ".."; });
}

bool has_valid_prefab_name(std::string_view name)
{
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    return std::all_of(name.begin(), name.end(),
        [](unsigned char character) {
            return std::isalnum(character) != 0 || character == '_' ||
                character == '-' || character == '.';
        });
}

template <typename Visitor>
void visit_files(
    const std::filesystem::path& root,
    std::string_view operation,
    Visitor&& visitor)
{
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(root, error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
        throw std::filesystem::filesystem_error(
            std::string(operation), root, error);
    }
    while (iterator != end) {
        const std::filesystem::path path = iterator->path();
        const bool regular_file = iterator->is_regular_file(error);
        if (error) {
            throw std::filesystem::filesystem_error(
                "could not inspect asset file", path, error);
        }
        if (regular_file) {
            visitor(path);
        }
        iterator.increment(error);
        if (error) {
            throw std::filesystem::filesystem_error(
                std::string(operation), root, error);
        }
    }
}

}

AssetDatabase::AssetDatabase(std::filesystem::path project_root)
    : project_root_(std::filesystem::absolute(std::move(project_root)).lexically_normal())
{
    std::error_code error;
    if (!std::filesystem::is_directory(project_root_, error) || error) {
        throw std::invalid_argument("asset database root is not a directory: " +
            project_root_.string());
    }
}

void AssetDatabase::refresh()
{
    std::vector<std::filesystem::path> model_paths;
    constexpr std::array<std::string_view, 4> roots{
        "assets", "content", "personagem", "Universal Animation Library[Standard]"};

    for (const std::string_view relative_root : roots) {
        const std::filesystem::path root = project_root_ / relative_root;
        std::error_code error;
        if (!std::filesystem::exists(root, error)) {
            if (error) {
                throw std::filesystem::filesystem_error(
                    "could not inspect asset directory", root, error);
            }
            continue;
        }
        if (!std::filesystem::is_directory(root, error) || error) {
            throw std::filesystem::filesystem_error(
                "asset root is not a directory", root, error);
        }

        visit_files(root, "could not scan asset directory",
            [&](const std::filesystem::path& file_path) {
            const std::filesystem::path relative_path =
                file_path.lexically_relative(project_root_);
            if (has_extension(relative_path, ".glb") ||
                has_extension(relative_path, ".gltf")) {
                model_paths.push_back(relative_path);
            }
        });
    }

    std::sort(model_paths.begin(), model_paths.end());
    std::vector<AssetEntry> refreshed;
    refreshed.reserve(model_paths.size());
    for (const std::filesystem::path& path : model_paths) {
        refreshed.push_back({path, path, AssetKind::model});
    }
    entries_ = refreshed;

    for (const std::string_view relative_root : roots) {
        const std::filesystem::path root = project_root_ / relative_root;
        std::error_code error;
        if (!std::filesystem::exists(root, error)) {
            if (error) {
                throw std::filesystem::filesystem_error(
                    "could not inspect asset directory", root, error);
            }
            continue;
        }
        visit_files(root, "could not scan prefab directory",
            [&](const std::filesystem::path& file_path) {
            const std::filesystem::path relative_path =
                file_path.lexically_relative(project_root_);
            if (has_extension(relative_path, ".mmoprefab")) {
                refreshed.push_back(read_prefab(relative_path));
            }
        });
    }

    std::sort(refreshed.begin(), refreshed.end(),
        [](const AssetEntry& left, const AssetEntry& right) {
            return left.relative_path < right.relative_path;
        });
    entries_ = std::move(refreshed);
}

const std::vector<AssetEntry>& AssetDatabase::entries() const
{
    return entries_;
}

std::filesystem::path AssetDatabase::resolve_model_path(
    const std::filesystem::path& asset_path) const
{
    const auto entry = std::find_if(entries_.begin(), entries_.end(),
        [&asset_path](const AssetEntry& candidate) {
            return candidate.relative_path == asset_path;
        });
    if (entry == entries_.end()) {
        throw std::invalid_argument("asset is not present in the asset database: " +
            asset_path.string());
    }
    const std::filesystem::path model_path =
        project_root_ / entry->model_path;
    std::error_code error;
    if (!std::filesystem::is_regular_file(model_path, error) || error) {
        throw std::runtime_error("asset model file is unavailable: " + model_path.string());
    }
    return model_path;
}

bool AssetDatabase::create_prefab(
    const std::filesystem::path& model_path,
    std::string name,
    AssetEntry& created,
    std::string& error)
{
    const auto model = std::find_if(entries_.begin(), entries_.end(),
        [&model_path](const AssetEntry& candidate) {
            return candidate.kind == AssetKind::model &&
                candidate.relative_path == model_path;
        });
    if (model == entries_.end()) {
        error = "Select a model from the asset database before creating a prefab.";
        return false;
    }
    if (!has_valid_prefab_name(name)) {
        error = "Prefab names may contain only letters, numbers, '.', '_' and '-'.";
        return false;
    }

    const std::filesystem::path relative_path =
        std::filesystem::path{"content"} / "prefabs" / (name + ".mmoprefab");
    const std::filesystem::path absolute_path = project_root_ / relative_path;
    std::error_code filesystem_error;
    if (!std::filesystem::create_directories(absolute_path.parent_path(), filesystem_error) &&
        filesystem_error) {
        error = "Could not create the prefab directory: " + filesystem_error.message();
        return false;
    }
    if (std::filesystem::exists(absolute_path, filesystem_error) || filesystem_error) {
        error = filesystem_error
            ? "Could not check for an existing prefab: " + filesystem_error.message()
            : "A prefab with that name already exists.";
        return false;
    }

    std::ofstream output(absolute_path, std::ios::binary | std::ios::out);
    if (!output) {
        error = "Could not write prefab file: " + absolute_path.string();
        return false;
    }
    output << kPrefabHeader << '\n'
           << kModelPrefix << model->relative_path.generic_string() << '\n';
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(absolute_path, ignored);
        error = "Could not finish writing prefab file: " + absolute_path.string();
        return false;
    }

    created = {relative_path, model->relative_path, AssetKind::prefab};
    entries_.push_back(created);
    std::sort(entries_.begin(), entries_.end(),
        [](const AssetEntry& left, const AssetEntry& right) {
            return left.relative_path < right.relative_path;
        });
    error.clear();
    return true;
}

AssetEntry AssetDatabase::read_prefab(
    const std::filesystem::path& relative_path) const
{
    const std::filesystem::path absolute_path = project_root_ / relative_path;
    std::ifstream input(absolute_path, std::ios::binary);
    std::string header;
    std::string model_line;
    std::string extra_line;
    if (!input || !std::getline(input, header) ||
        !std::getline(input, model_line) || header != kPrefabHeader ||
        !model_line.starts_with(kModelPrefix) || std::getline(input, extra_line)) {
        throw std::runtime_error("invalid prefab manifest: " + absolute_path.string());
    }

    const std::filesystem::path model_path{
        model_line.substr(kModelPrefix.size())};
    if (!is_safe_relative_path(model_path) ||
        (!has_extension(model_path, ".glb") &&
            !has_extension(model_path, ".gltf"))) {
        throw std::runtime_error("prefab has an unsafe or unsupported model path: " +
            absolute_path.string());
    }
    const auto model = std::find_if(entries_.begin(), entries_.end(),
        [&model_path](const AssetEntry& candidate) {
            return candidate.kind == AssetKind::model &&
                candidate.relative_path == model_path;
        });
    if (model == entries_.end()) {
        throw std::runtime_error("prefab references a model that is not indexed: " +
            model_path.string());
    }
    return {relative_path, model_path, AssetKind::prefab};
}

}
