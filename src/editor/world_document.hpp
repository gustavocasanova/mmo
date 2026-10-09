#pragma once

#include "editor/selection.hpp"
#include "game/world/terrain.hpp"

#include <filesystem>
#include <vector>

namespace mmo::editor {

struct WorldDocumentData {
    game::world::Terrain terrain;
    std::vector<SelectableObject> objects;
};

class WorldDocument {
public:
    static void save(
        const std::filesystem::path& path,
        const game::world::Terrain& terrain,
        const std::vector<SelectableObject>& objects);
    [[nodiscard]] static WorldDocumentData load(const std::filesystem::path& path);
};

}
