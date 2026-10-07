#include "editor/world_editor.hpp"

namespace mmo::editor {

EngineMode WorldEditor::mode() const
{
    return mode_;
}

bool WorldEditor::is_active() const
{
    return mode_ == EngineMode::world_editor;
}

bool WorldEditor::update_toggle(bool toggle_pressed)
{
    const bool just_pressed = toggle_pressed && !toggle_was_pressed_;
    toggle_was_pressed_ = toggle_pressed;
    if (!just_pressed) {
        return false;
    }

    mode_ = is_active() ? EngineMode::game : EngineMode::world_editor;
    return true;
}

}
