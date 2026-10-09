#pragma once

#include "game/world/terrain.hpp"

#include <glm/vec2.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>

struct GLFWwindow;

namespace mmo::editor {

class AssetDatabase;
class EditorHistory;
class SelectionManager;

struct AssetDrop {
    std::filesystem::path asset_path;
    glm::vec2 cursor_position{0.0f};
};

struct EditorUIActions {
    bool world_changed = false;
    bool request_save = false;
    bool request_load = false;
    bool request_undo = false;
    bool request_redo = false;
    std::optional<AssetDrop> asset_drop;
};

class EditorUI {
public:
    explicit EditorUI(::GLFWwindow* window);
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;
    ~EditorUI();

    void begin_frame();
    [[nodiscard]] EditorUIActions draw(
        bool visible,
        SelectionManager& selection,
        AssetDatabase& assets,
        const EditorHistory& history,
        bool& terrain_tools_active,
        game::world::TerrainBrush& terrain_brush,
        game::world::TerrainMaterial& terrain_material,
        float& brush_radius);
    void set_status(std::string message, bool is_error);
    void render();
    [[nodiscard]] bool wants_mouse_capture() const;
    [[nodiscard]] bool wants_keyboard_capture() const;

private:
    bool glfw_backend_initialized_ = false;
    bool opengl_backend_initialized_ = false;
    bool context_created_ = false;
    std::array<char, 128> asset_filter_{};
    std::array<char, 128> prefab_name_{};
    std::filesystem::path selected_asset_path_;
    std::string status_;
    bool status_is_error_ = false;
};

}
