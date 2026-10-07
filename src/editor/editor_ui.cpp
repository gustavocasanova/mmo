#include "editor/editor_ui.hpp"

#include "editor/asset_database.hpp"
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
    AssetDatabase& assets)
{
    if (!visible) {
        return {};
    }

    EditorUIActions actions;
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    const ImVec2 work_position = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;
    ImGui::SetNextWindowPos(
        {work_position.x + 284.0f, work_position.y + 48.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {std::max(240.0f, work_size.x - 594.0f),
            std::max(180.0f, work_size.y - 300.0f)}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags viewport_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("Asset Drop Viewport", nullptr, viewport_flags)) {
        ImGui::TextUnformatted("Drop an asset here to add it to the world.");
        ImVec2 area = ImGui::GetContentRegionAvail();
        area.x = std::max(1.0f, area.x);
        area.y = std::max(1.0f, area.y);
        ImGui::InvisibleButton("Asset Drop Target", area);
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

    ImGui::SetNextWindowPos(
        {work_position.x + 12.0f, work_position.y + 48.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({270.0f, std::max(260.0f, work_size.y - 60.0f)},
        ImGuiCond_FirstUseEver);
    if (ImGui::Begin("World Hierarchy")) {
        ImGui::Text("%zu objects", selection.objects().size());
        ImGui::Separator();
        for (const SelectableObject& object : selection.objects()) {
            const bool is_selected = selection.selected() == object.id;
            const std::string label = object.name + "##" + std::to_string(object.id);
            if (ImGui::Selectable(label.c_str(), is_selected) && !is_selected) {
                (void)selection.select(object.id);
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Duplicate") && selection.selected()) {
            actions.world_changed =
                selection.duplicate_selected().has_value() || actions.world_changed;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete") && selection.selected()) {
            actions.world_changed =
                selection.delete_selected() || actions.world_changed;
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        {work_position.x + work_size.x - 310.0f, work_position.y + 48.0f},
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({298.0f, std::max(240.0f, work_size.y - 60.0f)},
        ImGuiCond_FirstUseEver);
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
            ImGui::Separator();
            ImGui::Text("ID: %llu",
                static_cast<unsigned long long>(object->id));
            ImGui::Text("Transform: %s / %s / snap %s",
                selection.transform_mode() == TransformMode::translate ? "Move" :
                    selection.transform_mode() == TransformMode::rotate ? "Rotate" : "Scale",
                selection.transform_space() == TransformSpace::world ? "World" : "Local",
                selection.snapping_enabled() ? "On" : "Off");
            if (!object->asset_path.empty()) {
                ImGui::TextWrapped("Asset: %s", object->asset_path.c_str());
            }
            ImGui::TextUnformatted(
                "Shortcuts: M/R/T mode, X/Y/Z/U axis, G snap, C space");
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        {work_position.x + 284.0f, work_position.y + work_size.y - 242.0f},
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        {std::max(240.0f, work_size.x - 594.0f), 230.0f},
        ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Asset Browser")) {
        ImGui::InputTextWithHint(
            "##AssetFilter", "Search assets...", asset_filter_.data(), asset_filter_.size());
        ImGui::SameLine();
        ImGui::Text("%zu assets", assets.entries().size());
        ImGui::BeginChild("AssetList", {0.0f, 118.0f}, true);
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
                const std::string payload = asset.relative_path.generic_string();
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
            ImGui::SetNextItemWidth(180.0f);
            ImGui::InputTextWithHint(
                "##PrefabName", "Prefab name", prefab_name_.data(), prefab_name_.size());
            ImGui::SameLine();
            if (ImGui::Button("Create Prefab")) {
                AssetEntry created;
                if (assets.create_prefab(selected_asset->relative_path,
                        prefab_name_.data(), created, status_)) {
                    status_ = "Created " + created.relative_path.generic_string();
                    status_is_error_ = false;
                } else {
                    status_is_error_ = true;
                }
            }
        }
        if (!status_.empty()) {
            ImGui::TextColored(status_is_error_
                    ? ImVec4{1.0f, 0.35f, 0.3f, 1.0f}
                    : ImVec4{0.45f, 0.9f, 0.5f, 1.0f},
                "%s", status_.c_str());
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
