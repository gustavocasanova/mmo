#pragma once

namespace mmo::editor {

enum class EngineMode {
    game,
    world_editor,
};

class WorldEditor {
public:
    [[nodiscard]] EngineMode mode() const;
    [[nodiscard]] bool is_active() const;

    // Returns true only on the pressed edge that changes the active mode.
    bool update_toggle(bool toggle_pressed);

private:
    EngineMode mode_ = EngineMode::game;
    bool toggle_was_pressed_ = false;
};

}
