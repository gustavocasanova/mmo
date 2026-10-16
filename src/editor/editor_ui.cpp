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
        (asset.kind == AssetKind::prefab ? " [Pré-fabricado]" : "");
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
    world::TerrainBrush& terrain_brush,
    world::TerrainMaterial& terrain_material,
    float& brush_radius,
    float& brush_strength)
{
    if (!visible) {
        return {};
    }

    EditorUIActions actions;
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Arquivo")) {
            actions.request_save = ImGui::MenuItem("Salvar mundo", "Ctrl+S");
            actions.request_load = ImGui::MenuItem("Carregar mundo", "Ctrl+L");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Editar")) {
            actions.request_undo = ImGui::MenuItem(
                ("Desfazer (" + std::to_string(history.undo_count()) + ")").c_str(),
                "Ctrl+Z", false, history.can_undo());
            actions.request_redo = ImGui::MenuItem(
                ("Refazer (" + std::to_string(history.redo_count()) + ")").c_str(),
                "Ctrl+Y", false, history.can_redo());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Modo")) {
            ImGui::MenuItem(
                "Ferramentas de terreno", "F2", &terrain_tools_active);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Janela")) {
            ImGui::MenuItem("Hierarquia e ativos", nullptr, &scene_panel_visible_);
            ImGui::MenuItem("Propriedades", nullptr, &inspector_panel_visible_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ajuda")) {
            ImGui::TextDisabled("F1  Alternar jogo/editor");
            ImGui::TextDisabled("F2  Ferramentas de terreno");
            ImGui::TextDisabled("Ctrl+S  Salvar mundo");
            ImGui::TextDisabled("Ctrl+Z / Ctrl+Y  Desfazer / Refazer");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    const ImVec2 work_position = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    ImGui::SetNextWindowPos(
        {work_position.x + 8.0f, work_position.y + 8.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {std::max(240.0f, work_size.x - 16.0f), 48.0f}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags toolbar_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Barra de ferramentas", nullptr, toolbar_flags)) {
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
        actions.request_save = ImGui::Button("Salvar") || actions.request_save;
        ImGui::SameLine();
        actions.request_load = ImGui::Button("Carregar") || actions.request_load;
        ImGui::SameLine();
        actions.request_undo = ImGui::Button(
            ("Desfazer " + std::to_string(history.undo_count())).c_str()) ||
            actions.request_undo;
        ImGui::SameLine();
        actions.request_redo = ImGui::Button(
            ("Refazer " + std::to_string(history.redo_count())).c_str()) ||
            actions.request_redo;
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (mode_button("Mover",
                selection.transform_mode() == TransformMode::translate)) {
            selection.set_transform_mode(TransformMode::translate);
        }
        ImGui::SameLine();
        if (mode_button("Girar",
                selection.transform_mode() == TransformMode::rotate)) {
            selection.set_transform_mode(TransformMode::rotate);
        }
        ImGui::SameLine();
        if (mode_button("Escalar",
                selection.transform_mode() == TransformMode::scale)) {
            selection.set_transform_mode(TransformMode::scale);
        }
        ImGui::SameLine();
        bool snapping = selection.snapping_enabled();
        if (ImGui::Checkbox("Encaixe", &snapping)) {
            selection.set_snapping_enabled(snapping);
        }
        ImGui::SameLine();
        if (ImGui::Button(
                selection.transform_space() == TransformSpace::world
                    ? "Global" : "Local")) {
            selection.set_transform_space(
                selection.transform_space() == TransformSpace::world
                    ? TransformSpace::local : TransformSpace::world);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (mode_button(
                terrain_tools_active ? "Terreno: ativo" : "Terreno: inativo",
                terrain_tools_active)) {
            terrain_tools_active = !terrain_tools_active;
        }
        if (terrain_tools_active) {
            ImGui::SameLine();
            const char* brush_names[] = {"Raise", "Lower", "Flatten", "Paint"};
            int brush_index = terrain_brush == world::TerrainBrush::raise ? 0 :
                terrain_brush == world::TerrainBrush::lower ? 1 :
                terrain_brush == world::TerrainBrush::flatten ? 2 : 3;
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("##TerrainTool", &brush_index, brush_names, 4)) {
                terrain_brush = brush_index == 0
                    ? world::TerrainBrush::raise
                    : brush_index == 1 ? world::TerrainBrush::lower
                    : brush_index == 2 ? world::TerrainBrush::flatten
                    : world::TerrainBrush::paint_material;
            }
            if (terrain_brush == world::TerrainBrush::paint_material) {
                ImGui::SameLine();
                const char* material_names[] = {"Grass", "Dirt", "Rock", "Sand"};
                int material_index = static_cast<int>(terrain_material);
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Combo(
                        "##TerrainMaterial", &material_index, material_names, 4)) {
                    terrain_material =
                        static_cast<world::TerrainMaterial>(material_index);
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::SliderFloat("##BrushSize", &brush_radius, 1.0f, 12.0f, "%.1f");
        }
    }
    ImGui::End();

    const float scene_width = scene_panel_visible_ ? 258.0f : 0.0f;
    const float inspector_width = inspector_panel_visible_ ? 282.0f : 0.0f;
    const float scene_gap = scene_panel_visible_ ? 16.0f : 0.0f;
    const float inspector_gap = inspector_panel_visible_ ? 16.0f : 0.0f;
    if (scene_panel_visible_) {
        ImGui::SetNextWindowPos(
            {work_position.x + 8.0f, work_position.y + 64.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            {scene_width, std::max(180.0f, work_size.y - 72.0f)}, ImGuiCond_Always);
    }
    if (scene_panel_visible_) {
        if (ImGui::Begin("Hierarquia")) {
            if (ImGui::BeginTabBar("SceneTabs")) {
                if (ImGui::BeginTabItem("Objetos")) {
                    ImGui::TextDisabled("%zu objetos", selection.objects().size());
                    ImGui::BeginChild("ObjectList", {0.0f, -42.0f}, false);
                    for (const SelectableObject& object : selection.objects()) {
                        const bool is_selected = selection.selected() == object.id;
                        const std::string label = object.name + "##" +
                            std::to_string(object.id);
                        if (ImGui::Selectable(label.c_str(), is_selected) &&
                            !is_selected) {
                            (void)selection.select(object.id);
                        }
                    }
                    ImGui::EndChild();
                    if (ImGui::Button("Duplicar") && selection.selected()) {
                        actions.world_changed =
                            selection.duplicate_selected().has_value() ||
                            actions.world_changed;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Excluir") && selection.selected()) {
                        actions.world_changed =
                            selection.delete_selected() || actions.world_changed;
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Ativos")) {
                    ImGui::InputTextWithHint(
                        "##AssetFilter", "Pesquisar ativos...", asset_filter_.data(),
                        asset_filter_.size());
                    ImGui::TextDisabled("%zu ativos", assets.entries().size());
                    ImGui::BeginChild("AssetList", {0.0f, -82.0f}, true);
                    for (const AssetEntry& asset : assets.entries()) {
                        const std::string label = asset_label(asset);
                        if (!contains_case_insensitive(label, asset_filter_.data())) {
                            continue;
                        }
                        const bool selected =
                            selected_asset_path_ == asset.relative_path;
                        if (ImGui::Selectable(label.c_str(), selected)) {
                            selected_asset_path_ = asset.relative_path;
                            if (asset.kind == AssetKind::model) {
                                const std::string name =
                                    asset.relative_path.stem().string();
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
                            "##PrefabName", "Nome do pré-fabricado", prefab_name_.data(),
                            prefab_name_.size());
                        if (ImGui::Button("Criar pré-fabricado")) {
                            AssetEntry created;
                            if (assets.create_prefab(selected_asset->relative_path,
                                    prefab_name_.data(), created, status_)) {
                                status_ = "Criado: " +
                                    created.relative_path.generic_string();
                                status_is_error_ = false;
                            } else {
                                status_is_error_ = true;
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Terreno")) {
                    bool enabled = terrain_tools_active;
                    if (ImGui::Checkbox("Ativar edição do terreno", &enabled)) {
                        terrain_tools_active = enabled;
                    }
                    ImGui::Separator();
                    const char* brush_names[] = {
                        "Elevar", "Abaixar", "Nivelar", "Pintar material"};
                    int brush_index =
                        terrain_brush == world::TerrainBrush::raise ? 0 :
                        terrain_brush == world::TerrainBrush::lower ? 1 :
                        terrain_brush == world::TerrainBrush::flatten ? 2 : 3;
                    if (ImGui::Combo("Ferramenta", &brush_index, brush_names, 4)) {
                        terrain_brush = brush_index == 0
                            ? world::TerrainBrush::raise
                            : brush_index == 1 ? world::TerrainBrush::lower
                            : brush_index == 2 ? world::TerrainBrush::flatten
                            : world::TerrainBrush::paint_material;
                    }
                    if (terrain_brush == world::TerrainBrush::paint_material) {
                        const char* material_names[] = {
                            "Grama", "Terra", "Rocha", "Areia"};
                        int material_index = static_cast<int>(terrain_material);
                        if (ImGui::Combo(
                                "Material", &material_index, material_names, 4)) {
                            terrain_material = static_cast<world::TerrainMaterial>(
                                material_index);
                        }
                    }
                    ImGui::SliderFloat(
                        "Raio do pincel", &brush_radius, 1.0f, 12.0f, "%.1f m");
                    ImGui::SliderFloat(
                        "Força", &brush_strength, 0.1f, 5.0f, "%.1f");
                    ImGui::TextDisabled(
                        "Use o botão esquerdo para aplicar no terreno.");
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
    }

    if (inspector_panel_visible_) {
        ImGui::SetNextWindowPos(
            {work_position.x + work_size.x - inspector_width - 8.0f,
                work_position.y + 64.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            {inspector_width, std::max(180.0f, work_size.y - 72.0f)},
            ImGuiCond_Always);
    }
    if (inspector_panel_visible_) {
        if (ImGui::Begin("Propriedades")) {
            const std::optional<SelectionId> selected = selection.selected();
            const std::optional<SelectableObject> object =
                selected ? selection.object_for(*selected) : std::nullopt;
            if (!object) {
                ImGui::TextUnformatted(
                    "Selecione um objeto para ver suas propriedades.");
            } else {
                std::array<char, 65> name{};
                const std::size_t name_size =
                    std::min(object->name.size(), name.size() - 1);
                std::memcpy(name.data(), object->name.data(), name_size);
                if (ImGui::InputText("Nome", name.data(), name.size())) {
                    actions.world_changed = selection.rename(object->id, name.data()) ||
                        actions.world_changed;
                }

                glm::vec3 position = object->position;
                glm::vec3 rotation = object->rotation_degrees;
                glm::vec3 scale = object->scale;
                bool transform_changed = ImGui::DragFloat3(
                    "Posição", &position.x, 0.05f);
                transform_changed = ImGui::DragFloat3(
                    "Rotação", &rotation.x, 1.0f) || transform_changed;
                transform_changed = ImGui::DragFloat3(
                    "Escala", &scale.x, 0.02f, 0.1f, 1000.0f) || transform_changed;
                if (transform_changed) {
                    scale = glm::max(scale, glm::vec3{0.1f});
                    actions.world_changed = selection.set_transform(
                        object->id, position, rotation, scale) || actions.world_changed;
                }
                if (!object->asset_path.empty()) {
                    ImGui::Separator();
                    ImGui::TextWrapped("Ativo: %s", object->asset_path.c_str());
                }
            }
        }
        ImGui::End();
    }

    ImGui::SetNextWindowPos(
        {work_position.x + 8.0f + scene_width + scene_gap,
            work_position.y + 64.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {std::max(120.0f, work_size.x - 16.0f - scene_width - scene_gap -
                inspector_width - inspector_gap),
            std::max(120.0f, work_size.y - 72.0f)}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags viewport_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("Visão de cena", nullptr, viewport_flags)) {
        ImGui::TextDisabled(
            "Arraste um ativo para a cena  |  Botão direito: olhar  |  F1: jogo/editor");
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
