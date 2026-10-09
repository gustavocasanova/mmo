#include "editor/editor_ui.hpp"

#include "editor/asset_database.hpp"
#include "editor/editor_history.hpp"
#include "editor/selection.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mmo::editor {
namespace {

std::string asset_label(const AssetEntry& asset)
{
    return asset.relative_path.generic_string() +
        (asset.kind == AssetKind::prefab ? " [Prefab]" : "");
}

bool contains_case_insensitive(std::string_view text, std::string_view query)
{
    if (query.empty()) {
        return true;
    }
    return std::search(text.begin(), text.end(), query.begin(), query.end(),
        [](unsigned char left, unsigned char right) {
            return std::tolower(left) == std::tolower(right);
        }) != text.end();
}

}

EditorUI::EditorUI(::GLFWwindow* window)
{
    if (window == nullptr) {
        throw std::invalid_argument("editor UI requires a valid GLFW window");
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    context_created_ = true;
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = {12.0f, 10.0f};
    style.ItemSpacing = {8.0f, 7.0f};
    ImVec4* const colors = style.Colors;
    colors[ImGuiCol_Header] = {0.18f, 0.36f, 0.42f, 0.8f};
    colors[ImGuiCol_HeaderHovered] = {0.22f, 0.49f, 0.56f, 0.9f};
    colors[ImGuiCol_Button] = {0.14f, 0.31f, 0.37f, 0.85f};
    colors[ImGuiCol_ButtonHovered] = {0.2f, 0.45f, 0.52f, 1.0f};

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        context_created_ = false;
        throw std::runtime_error("could not initialize ImGui GLFW backend");
    }
    glfw_backend_initialized_ = true;
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        ImGui_ImplGlfw_Shutdown();
        glfw_backend_initialized_ = false;
        ImGui::DestroyContext();
        context_created_ = false;
        throw std::runtime_error("could not initialize ImGui OpenGL backend");
    }
    opengl_backend_initialized_ = true;
}

EditorUI::~EditorUI()
{
    if (opengl_backend_initialized_) {
        ImGui_ImplOpenGL3_Shutdown();
    }
    if (glfw_backend_initialized_) {
        ImGui_ImplGlfw_Shutdown();
    }
    if (context_created_) {
        ImGui::DestroyContext();
    }
}

void EditorUI::begin_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

EditorUIActions EditorUI::draw(
    bool visible,
    SelectionManager& selection,
    AssetDatabase& assets,
    const EditorHistory& history,
    bool& terrain_tools_active,
    game::world::TerrainBrush& terrain_brush,
    game::world::TerrainMaterial& terrain_material,
    float& brush_radius)
{
    if (!visible) {
        return {};
    }

    EditorUIActions actions;
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    const ImVec2 work_position = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;
    ImGui::SetNextWindowPos(
        {work_position.x + 8.0f, work_position.y + 8.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {std::max(240.0f, work_size.x - 16.0f), 48.0f}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags toolbar_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Editor Toolbar", nullptr, toolbar_flags)) {
        const auto mode_button = [](const char* label, bool active) {
            if (active) {
                ImGui::PushStyleColor(
                    ImGuiCol_Button, ImVec4{0.19f, 0.47f, 0.53f, 1.0f});
            }
            const bool clicked = ImGui::Button(label);
            if (active) {
                ImGui::PopStyleColor();
            }
            return clicked;
        };
        actions.request_save = ImGui::Button("Save");
        ImGui::SameLine();
        actions.request_load = ImGui::Button("Load");
        ImGui::SameLine();
        actions.request_undo = ImGui::Button(
            ("Undo " + std::to_string(history.undo_count())).c_str());
        ImGui::SameLine();
        actions.request_redo = ImGui::Button(
            ("Redo " + std::to_string(history.redo_count())).c_str());
        ImGui::SameLine();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (mode_button("Move",
                selection.transform_mode() == TransformMode::translate)) {
            selection.set_transform_mode(TransformMode::translate);
        }
        ImGui::SameLine();
        if (mode_button("Rotate",
                selection.transform_mode() == TransformMode::rotate)) {
            selection.set_transform_mode(TransformMode::rotate);
        }
        ImGui::SameLine();
        if (mode_button("Scale",
                selection.transform_mode() == TransformMode::scale)) {
            selection.set_transform_mode(TransformMode::scale);
        }
        ImGui::SameLine();
        bool snapping = selection.snapping_enabled();
        if (ImGui::Checkbox("Snap", &snapping)) {
            selection.set_snapping_enabled(snapping);
        }
        ImGui::SameLine();
        if (ImGui::Button(
                selection.transform_space() == TransformSpace::world
                    ? "World" : "Local")) {
            selection.set_transform_space(
                selection.transform_space() == TransformSpace::world
                    ? TransformSpace::local : TransformSpace::world);
        }
        ImGui::SameLine();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (mode_button(
                terrain_tools_active ? "Terrain: On" : "Terrain: Off",
                terrain_tools_active)) {
            terrain_tools_active = !terrain_tools_active;
        }
        if (terrain_tools_active) {
            ImGui::SameLine();
            const char* brush_names[] = {"Raise", "Lower", "Flatten", "Paint"};
            int brush_index = terrain_brush == game::world::TerrainBrush::raise ? 0 :
                terrain_brush == game::world::TerrainBrush::lower ? 1 :
                terrain_brush == game::world::TerrainBrush::flatten ? 2 : 3;
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("##TerrainTool", &brush_index, brush_names, 4)) {
                terrain_brush = brush_index == 0
                    ? game::world::TerrainBrush::raise
                    : brush_index == 1 ? game::world::TerrainBrush::lower
                    : brush_index == 2 ? game::world::TerrainBrush::flatten
                    : game::world::TerrainBrush::paint_material;
            }
            if (terrain_brush == game::world::TerrainBrush::paint_material) {
                ImGui::SameLine();
                const char* material_names[] = {"Grass", "Dirt", "Rock", "Sand"};
                int material_index = static_cast<int>(terrain_material);
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Combo(
                        "##TerrainMaterial", &material_index, material_names, 4)) {
                    terrain_material =
                        static_cast<game::world::TerrainMaterial>(material_index);
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::SliderFloat("##BrushSize", &brush_radius, 1.0f, 12.0f, "%.1f");
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        {work_position.x + 8.0f, work_position.y + 64.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {258.0f, std::max(180.0f, work_size.y - 72.0f)}, ImGuiCond_Always);
    if (ImGui::Begin("Scene")) {
        if (ImGui::BeginTabBar("SceneTabs")) {
            if (ImGui::BeginTabItem("Objects")) {
                ImGui::TextDisabled("%zu objects", selection.objects().size());
                ImGui::BeginChild("ObjectList", {0.0f, -42.0f}, false);
                for (const SelectableObject& object : selection.objects()) {
                    const bool is_selected = selection.selected() == object.id;
                    const std::string label = object.name + "##" +
                        std::to_string(object.id);
                    if (ImGui::Selectable(label.c_str(), is_selected) && !is_selected) {
                        (void)selection.select(object.id);
                    }
                }
                ImGui::EndChild();
                if (ImGui::Button("Duplicate") && selection.selected()) {
                    actions.world_changed =
                        selection.duplicate_selected().has_value() ||
                        actions.world_changed;
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete") && selection.selected()) {
                    actions.world_changed =
                        selection.delete_selected() || actions.world_changed;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Assets")) {
                ImGui::InputTextWithHint(
                    "##AssetFilter", "Search...", asset_filter_.data(),
                    asset_filter_.size());
                ImGui::TextDisabled("%zu assets", assets.entries().size());
                ImGui::BeginChild("AssetList", {0.0f, -82.0f}, true);
                for (const AssetEntry& asset : assets.entries()) {
                    const std::string label = asset_label(asset);
                    if (!contains_case_insensitive(label, asset_filter_.data())) {
                        continue;
                    }
                    const bool selected = selected_asset_path_ == asset.relative_path;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        selected_asset_path_ = asset.relative_path;
                        if (asset.kind == AssetKind::model) {
                            const std::string name = asset.relative_path.stem().string();
                            prefab_name_.fill('\0');
                            std::memcpy(prefab_name_.data(), name.data(),
                                std::min(name.size(), prefab_name_.size() - 1));
                        }
                    }
                    if (ImGui::BeginDragDropSource()) {
                        const std::string payload =
                            asset.relative_path.generic_string();
                        ImGui::SetDragDropPayload(
                            "MMO_ASSET_PATH", payload.c_str(), payload.size() + 1);
                        ImGui::TextUnformatted(label.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                ImGui::EndChild();

                const auto selected_asset = std::find_if(
                    assets.entries().begin(), assets.entries().end(),
                    [this](const AssetEntry& asset) {
                        return asset.relative_path == selected_asset_path_;
                    });
                if (selected_asset != assets.entries().end() &&
                    selected_asset->kind == AssetKind::model) {
                    ImGui::InputTextWithHint(
                        "##PrefabName", "Prefab name", prefab_name_.data(),
                        prefab_name_.size());
                    if (ImGui::Button("Create Prefab")) {
                        AssetEntry created;
                        if (assets.create_prefab(selected_asset->relative_path,
                                prefab_name_.data(), created, status_)) {
                            status_ = "Created " +
                                created.relative_path.generic_string();
                            status_is_error_ = false;
                        } else {
                            status_is_error_ = true;
                        }
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!status_.empty()) {
            ImGui::Separator();
            ImGui::TextColored(status_is_error_
                    ? ImVec4{1.0f, 0.35f, 0.3f, 1.0f}
                    : ImVec4{0.45f, 0.9f, 0.5f, 1.0f},
                "%s", status_.c_str());
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        {work_position.x + work_size.x - 290.0f, work_position.y + 64.0f},
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {282.0f, std::max(180.0f, work_size.y - 72.0f)}, ImGuiCond_Always);
    if (ImGui::Begin("Inspector")) {
        const std::optional<SelectionId> selected = selection.selected();
        const std::optional<SelectableObject> object =
            selected ? selection.object_for(*selected) : std::nullopt;
        if (!object) {
            ImGui::TextUnformatted("Select an object to inspect it.");
        } else {
            std::array<char, 65> name{};
            const std::size_t name_size =
                std::min(object->name.size(), name.size() - 1);
            std::memcpy(name.data(), object->name.data(), name_size);
            if (ImGui::InputText("Name", name.data(), name.size())) {
                actions.world_changed =
                    selection.rename(object->id, name.data()) || actions.world_changed;
            }

            glm::vec3 position = object->position;
            glm::vec3 rotation = object->rotation_degrees;
            glm::vec3 scale = object->scale;
            bool transform_changed = ImGui::DragFloat3(
                "Position", &position.x, 0.05f);
            transform_changed = ImGui::DragFloat3(
                "Rotation", &rotation.x, 1.0f) || transform_changed;
            transform_changed = ImGui::DragFloat3(
                "Scale", &scale.x, 0.02f, 0.1f, 1000.0f) || transform_changed;
            if (transform_changed) {
                scale = glm::max(scale, glm::vec3{0.1f});
                actions.world_changed = selection.set_transform(
                    object->id, position, rotation, scale) || actions.world_changed;
            }
            if (!object->asset_path.empty()) {
                ImGui::Separator();
                ImGui::TextWrapped("Asset: %s", object->asset_path.c_str());
            }
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        {work_position.x + 274.0f, work_position.y + 64.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {std::max(120.0f, work_size.x - 556.0f),
            std::max(120.0f, work_size.y - 72.0f)}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags viewport_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("Viewport", nullptr, viewport_flags)) {
        ImGui::TextDisabled("Drop an asset here  |  RMB: look  |  F1: game/editor");
        ImVec2 area = ImGui::GetContentRegionAvail();
        area.x = std::max(1.0f, area.x);
        area.y = std::max(1.0f, area.y);
        ImGui::InvisibleButton("Viewport Drop Target", area);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("MMO_ASSET_PATH")) {
                if (payload->Data != nullptr && payload->DataSize > 1) {
                    const std::string path(
                        static_cast<const char*>(payload->Data),
                        static_cast<std::size_t>(payload->DataSize - 1));
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    actions.asset_drop = AssetDrop{
                        std::filesystem::path{path},
                        {mouse.x - viewport->Pos.x, mouse.y - viewport->Pos.y}};
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::End();
    return actions;
}

void EditorUI::set_status(std::string message, bool is_error)
{
    status_ = std::move(message);
    status_is_error_ = is_error;
}

void EditorUI::render()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool EditorUI::wants_mouse_capture() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool EditorUI::wants_keyboard_capture() const
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

}
