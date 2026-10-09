#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mmo::editor {

using SelectionId = std::uint64_t;

enum class TransformMode {
    translate,
    rotate,
    scale,
};

enum class TransformAxis {
    x,
    y,
    z,
    uniform,
};

enum class TransformSpace {
    world,
    local,
};

struct SelectionBounds {
    glm::vec3 minimum;
    glm::vec3 maximum;
};

struct SelectableObject {
    SelectionId id;
    SelectionBounds local_bounds;
    glm::vec3 position{0.0f};
    glm::vec3 rotation_degrees{0.0f};
    glm::vec3 scale{1.0f};
    std::string name{"Object"};
    std::string asset_path;
};

struct SelectionHit {
    SelectionId id;
    float distance;
    glm::vec3 position;
};

class SelectionManager {
public:
    explicit SelectionManager(std::vector<SelectableObject> objects);

    [[nodiscard]] std::optional<SelectionHit> select_ray(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance);
    [[nodiscard]] bool select(SelectionId id);
    void clear();
    [[nodiscard]] std::optional<SelectionId> selected() const;
    [[nodiscard]] std::optional<SelectionBounds> bounds_for(SelectionId id) const;
    [[nodiscard]] std::optional<SelectableObject> object_for(SelectionId id) const;
    [[nodiscard]] const std::vector<SelectableObject>& objects() const;
    void replace_objects(
        std::vector<SelectableObject> objects,
        std::optional<SelectionId> selected = std::nullopt);
    [[nodiscard]] SelectionId add_object(SelectableObject object);
    [[nodiscard]] TransformMode transform_mode() const;
    [[nodiscard]] TransformAxis transform_axis() const;
    [[nodiscard]] glm::vec3 transform_axis_direction() const;
    [[nodiscard]] bool snapping_enabled() const;
    [[nodiscard]] TransformSpace transform_space() const;
    void set_transform_mode(TransformMode mode);
    void set_transform_axis(TransformAxis axis);
    void set_transform_space(TransformSpace space);
    void set_snapping_enabled(bool enabled);
    void transform_selected(
        glm::vec3 translation,
        glm::vec3 rotation_degrees,
        glm::vec3 scale_delta);
    [[nodiscard]] bool rename(SelectionId id, std::string_view name);
    [[nodiscard]] std::optional<SelectionId> duplicate_selected();
    [[nodiscard]] bool delete_selected();
    [[nodiscard]] bool set_transform(
        SelectionId id,
        glm::vec3 position,
        glm::vec3 rotation_degrees,
        glm::vec3 scale);

private:
    [[nodiscard]] static SelectionBounds world_bounds(const SelectableObject& object);

    std::vector<SelectableObject> objects_;
    std::optional<SelectionId> selected_;
    SelectionId next_id_ = 1;
    TransformMode transform_mode_ = TransformMode::translate;
    TransformAxis transform_axis_ = TransformAxis::x;
    TransformSpace transform_space_ = TransformSpace::world;
    bool snapping_enabled_ = true;
    float translation_snap_ = 1.0f;
    float rotation_snap_degrees_ = 15.0f;
    float scale_snap_ = 0.1f;
};

}
