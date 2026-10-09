#include "editor/editor_history.hpp"

#include <algorithm>
#include <utility>

namespace mmo::editor {

EditorSnapshot capture_snapshot(
    const game::world::Terrain& terrain,
    const SelectionManager& selection)
{
    return {terrain, selection.objects(), selection.selected()};
}

bool EditorHistory::can_undo() const
{
    return cursor_ > 0;
}

bool EditorHistory::can_redo() const
{
    return cursor_ < entries_.size();
}

std::size_t EditorHistory::undo_count() const
{
    return cursor_;
}

std::size_t EditorHistory::redo_count() const
{
    return entries_.size() - cursor_;
}

void EditorHistory::record(
    EditorSnapshot before,
    EditorSnapshot after,
    std::string coalesce_key)
{
    if (cursor_ < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(cursor_),
            entries_.end());
    }
    if (!coalesce_key.empty() && cursor_ > 0 &&
        entries_[cursor_ - 1].coalesce_key == coalesce_key) {
        entries_[cursor_ - 1].after = std::move(after);
        return;
    }
    entries_.push_back({
        std::move(before), std::move(after), std::move(coalesce_key)});
    ++cursor_;
    if (entries_.size() > kMaximumEntries) {
        entries_.erase(entries_.begin());
        --cursor_;
    }
}

void EditorHistory::break_coalescing()
{
    if (cursor_ > 0) {
        entries_[cursor_ - 1].coalesce_key.clear();
    }
}

void EditorHistory::clear()
{
    entries_.clear();
    cursor_ = 0;
}

bool EditorHistory::undo(
    game::world::Terrain& terrain,
    SelectionManager& selection)
{
    if (!can_undo()) {
        return false;
    }
    const EditorSnapshot& snapshot = entries_[--cursor_].before;
    terrain = snapshot.terrain;
    terrain.invalidate_render_chunks();
    selection.replace_objects(snapshot.objects, snapshot.selected);
    return true;
}

bool EditorHistory::redo(
    game::world::Terrain& terrain,
    SelectionManager& selection)
{
    if (!can_redo()) {
        return false;
    }
    const EditorSnapshot& snapshot = entries_[cursor_++].after;
    terrain = snapshot.terrain;
    terrain.invalidate_render_chunks();
    selection.replace_objects(snapshot.objects, snapshot.selected);
    return true;
}

}
