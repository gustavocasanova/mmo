#pragma once

#include "editor/selection.hpp"
#include "game/world/terrain.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace mmo::editor {

struct EditorSnapshot {
    game::world::Terrain terrain;
    std::vector<SelectableObject> objects;
    std::optional<SelectionId> selected;
};

[[nodiscard]] EditorSnapshot capture_snapshot(
    const game::world::Terrain& terrain,
    const SelectionManager& selection);

class EditorHistory {
public:
    static constexpr std::size_t kMaximumEntries = 128;

    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::size_t undo_count() const;
    [[nodiscard]] std::size_t redo_count() const;
    void record(EditorSnapshot before, EditorSnapshot after, std::string coalesce_key);
    void break_coalescing();
    void clear();
    [[nodiscard]] bool undo(game::world::Terrain& terrain, SelectionManager& selection);
    [[nodiscard]] bool redo(game::world::Terrain& terrain, SelectionManager& selection);

private:
    struct Entry {
        EditorSnapshot before;
        EditorSnapshot after;
        std::string coalesce_key;
    };

    std::vector<Entry> entries_;
    std::size_t cursor_ = 0;
};

}
