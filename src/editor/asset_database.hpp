#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mmo::editor {

enum class AssetKind {
    model,
    prefab,
};

struct AssetEntry {
    std::filesystem::path relative_path;
    std::filesystem::path model_path;
    AssetKind kind;
};

class AssetDatabase {
public:
    explicit AssetDatabase(std::filesystem::path project_root);

    void refresh();
    [[nodiscard]] const std::vector<AssetEntry>& entries() const;
    [[nodiscard]] std::filesystem::path resolve_model_path(
        const std::filesystem::path& asset_path) const;
    [[nodiscard]] bool create_prefab(
        const std::filesystem::path& model_path,
        std::string name,
        AssetEntry& created,
        std::string& error);

private:
    [[nodiscard]] AssetEntry read_prefab(
        const std::filesystem::path& relative_path) const;

    std::filesystem::path project_root_;
    std::vector<AssetEntry> entries_;
};

}
