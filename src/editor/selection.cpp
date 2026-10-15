#include "editor/selection.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace mmo::editor {
namespace {

bool is_finite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::mat4 object_matrix(const SelectableObject& object)
{
    glm::mat4 model{1.0f};
    model = glm::translate(model, object.position);
    model = glm::rotate(model, glm::radians(object.rotation_degrees.y),
        {0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, glm::radians(object.rotation_degrees.x),
        {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(object.rotation_degrees.z),
        {0.0f, 0.0f, 1.0f});
    return glm::scale(model, object.scale);
}

std::optional<float> ray_box_distance(
    glm::vec3 origin,
    glm::vec3 direction,
    const SelectionBounds& bounds,
    float max_distance)
{
    float near_distance = 0.0f;
    float far_distance = max_distance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) <= 0.000001f) {
            if (origin[axis] < bounds.minimum[axis] ||
                origin[axis] > bounds.maximum[axis]) {
                return std::nullopt;
            }
            continue;
        }

        float first = (bounds.minimum[axis] - origin[axis]) / direction[axis];
        float second = (bounds.maximum[axis] - origin[axis]) / direction[axis];
        if (first > second) {
            std::swap(first, second);
        }
        near_distance = std::max(near_distance, first);
        far_distance = std::min(far_distance, second);
        if (near_distance > far_distance) {
            return std::nullopt;
        }
    }
    return near_distance <= max_distance && far_distance >= 0.0f
        ? std::optional<float>{near_distance}
        : std::nullopt;
}

float snap_value(float value, float increment)
{
    return std::round(value / increment) * increment;
}

glm::vec3 axis_vector(TransformAxis axis)
{
    switch (axis) {
    case TransformAxis::x: return {1.0f, 0.0f, 0.0f};
    case TransformAxis::y: return {0.0f, 1.0f, 0.0f};
    case TransformAxis::z: return {0.0f, 0.0f, 1.0f};
    case TransformAxis::uniform: return {1.0f, 1.0f, 1.0f};
    }
    throw std::invalid_argument("unknown transform axis");
}

}

SelectionManager::SelectionManager(std::vector<SelectableObject> objects)
    : objects_(std::move(objects))
{
    std::unordered_set<SelectionId> ids;
    for (SelectableObject& object : objects_) {
        if (object.id == 0 || !ids.insert(object.id).second ||
            !is_finite(object.local_bounds.minimum) ||
            !is_finite(object.local_bounds.maximum) ||
            !is_finite(object.position) || !is_finite(object.rotation_degrees) ||
            !is_finite(object.scale) ||
            glm::any(glm::lessThanEqual(object.scale, glm::vec3{0.0f})) ||
            glm::any(glm::greaterThan(
                object.local_bounds.minimum, object.local_bounds.maximum))) {
            throw std::invalid_argument("selectable objects require unique IDs and valid bounds");
        }
        if (object.name.empty()) {
            object.name = "Object " + std::to_string(object.id);
        } else if (object.name.size() > 64 ||
            std::none_of(object.name.begin(), object.name.end(),
                [](unsigned char character) { return std::isspace(character) == 0; })) {
            throw std::invalid_argument("object names must contain 1 to 64 non-space bytes");
        }
        if (object.id == std::numeric_limits<SelectionId>::max()) {
            next_id_ = 0;
        } else if (next_id_ != 0) {
            next_id_ = std::max(next_id_, object.id + 1);
        }
        const glm::vec3 center =
            (object.local_bounds.minimum + object.local_bounds.maximum) * 0.5f;
        object.position = center;
        object.local_bounds.minimum -= center;
        object.local_bounds.maximum -= center;
    }
}

bool SelectionManager::select(SelectionId id)
{
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [id](const SelectableObject& candidate) { return candidate.id == id; });
    if (object == objects_.end()) {
        return false;
    }
    selected_ = id;
    return true;
}

std::optional<SelectionHit> SelectionManager::select_ray(
    glm::vec3 origin,
    glm::vec3 direction,
    float max_distance)
{
    const float direction_length = glm::length(direction);
    if (!is_finite(origin) || !is_finite(direction) ||
        !std::isfinite(direction_length) || direction_length <= 0.000001f ||
        !std::isfinite(max_distance) || max_distance <= 0.0f) {
        throw std::invalid_argument("selection ray must have finite origin, direction and range");
    }
    direction /= direction_length;

    std::optional<SelectionHit> nearest;
    for (const SelectableObject& object : objects_) {
        const glm::mat4 model = object_matrix(object);
        const glm::mat4 inverse_model = glm::inverse(model);
        const glm::vec3 local_origin{
            inverse_model * glm::vec4{origin, 1.0f}};
        const glm::vec3 unnormalized_local_direction{
            inverse_model * glm::vec4{direction, 0.0f}};
        const float local_direction_length = glm::length(unnormalized_local_direction);
        if (!std::isfinite(local_direction_length) || local_direction_length <= 0.000001f) {
            continue;
        }
        const glm::vec3 local_direction =
            unnormalized_local_direction / local_direction_length;
        const std::optional<float> local_distance = ray_box_distance(
            local_origin, local_direction, object.local_bounds,
            max_distance * local_direction_length);
        if (!local_distance) {
            continue;
        }
        const glm::vec3 local_hit = local_origin + local_direction * *local_distance;
        const glm::vec3 world_hit = glm::vec3{model * glm::vec4{local_hit, 1.0f}};
        const float world_distance = glm::length(world_hit - origin);
        if (world_distance <= max_distance &&
            (!nearest || world_distance < nearest->distance)) {
            nearest = SelectionHit{object.id, world_distance, world_hit};
        }
    }

    selected_ = nearest ? std::optional<SelectionId>{nearest->id} : std::nullopt;
    return nearest;
}

void SelectionManager::clear()
{
    selected_.reset();
}

std::optional<SelectionId> SelectionManager::selected() const
{
    return selected_;
}

std::optional<SelectionBounds> SelectionManager::bounds_for(SelectionId id) const
{
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [id](const SelectableObject& candidate) { return candidate.id == id; });
    if (object == objects_.end()) {
        return std::nullopt;
    }
    return world_bounds(*object);
}

SelectionBounds SelectionManager::world_bounds(const SelectableObject& object)
{
    const glm::mat4 model = object_matrix(object);
    glm::vec3 minimum{std::numeric_limits<float>::max()};
    glm::vec3 maximum{std::numeric_limits<float>::lowest()};
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const glm::vec3 local{
                    x == 0 ? object.local_bounds.minimum.x : object.local_bounds.maximum.x,
                    y == 0 ? object.local_bounds.minimum.y : object.local_bounds.maximum.y,
                    z == 0 ? object.local_bounds.minimum.z : object.local_bounds.maximum.z,
                };
                const glm::vec3 world = glm::vec3{model * glm::vec4{local, 1.0f}};
                minimum = glm::min(minimum, world);
                maximum = glm::max(maximum, world);
            }
        }
    }
    return {minimum, maximum};
}

std::optional<SelectableObject> SelectionManager::object_for(SelectionId id) const
{
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [id](const SelectableObject& candidate) { return candidate.id == id; });
    return object == objects_.end()
        ? std::nullopt
        : std::optional<SelectableObject>{*object};
}

const std::vector<SelectableObject>& SelectionManager::objects() const
{
    return objects_;
}

void SelectionManager::replace_objects(
    std::vector<SelectableObject> objects,
    std::optional<SelectionId> selected)
{
    SelectionManager validated{objects};
    objects_ = std::move(objects);
    next_id_ = validated.next_id_;
    selected_.reset();
    if (selected) {
        (void)select(*selected);
    }
}

SelectionId SelectionManager::add_object(SelectableObject object)
{
    if (!is_finite(object.local_bounds.minimum) ||
        !is_finite(object.local_bounds.maximum) ||
        !is_finite(object.position) || !is_finite(object.rotation_degrees) ||
        !is_finite(object.scale) ||
        glm::any(glm::lessThanEqual(object.scale, glm::vec3{0.0f})) ||
        glm::any(glm::greaterThan(
            object.local_bounds.minimum, object.local_bounds.maximum))) {
        throw std::invalid_argument("new selectable object has invalid bounds or transform");
    }
    if (!object.name.empty() &&
        (object.name.size() > 64 ||
            std::none_of(object.name.begin(), object.name.end(),
                [](unsigned char character) { return std::isspace(character) == 0; }))) {
        throw std::invalid_argument("object names must contain 1 to 64 non-space bytes");
    }
    if (next_id_ == 0) {
        throw std::overflow_error("no object IDs remain for a new object");
    }
    object.id = next_id_;
    if (next_id_ == std::numeric_limits<SelectionId>::max()) {
        next_id_ = 0;
    } else {
        ++next_id_;
    }
    if (object.name.empty()) {
        object.name = "Object " + std::to_string(object.id);
    }
    const glm::vec3 center =
        (object.local_bounds.minimum + object.local_bounds.maximum) * 0.5f;
    object.position += center;
    object.local_bounds.minimum -= center;
    object.local_bounds.maximum -= center;
    const SelectionId id = object.id;
    objects_.push_back(std::move(object));
    selected_ = id;
    return id;
}

bool SelectionManager::rename(SelectionId id, std::string_view name)
{
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [id](const SelectableObject& candidate) { return candidate.id == id; });
    if (object == objects_.end()) {
        return false;
    }
    const bool has_non_space = std::any_of(name.begin(), name.end(),
        [](unsigned char character) {
            return std::isspace(character) == 0;
        });
    if (!has_non_space || name.size() > 64) {
        throw std::invalid_argument("object names must contain 1 to 64 non-space bytes");
    }
    if (object->name == name) {
        return false;
    }
    object->name.assign(name);
    return true;
}

std::optional<SelectionId> SelectionManager::duplicate_selected()
{
    if (!selected_) {
        return std::nullopt;
    }
    if (next_id_ == 0) {
        throw std::overflow_error("no object IDs remain for duplication");
    }
    const auto source = std::find_if(objects_.begin(), objects_.end(),
        [this](const SelectableObject& candidate) { return candidate.id == *selected_; });
    if (source == objects_.end()) {
        selected_.reset();
        return std::nullopt;
    }

    SelectableObject duplicate = *source;
    duplicate.id = next_id_;
    if (next_id_ == std::numeric_limits<SelectionId>::max()) {
        next_id_ = 0;
    } else {
        ++next_id_;
    }
    duplicate.position.x += 1.0f;
    const std::string suffix = " Copy";
    duplicate.name = source->name.substr(0, 64 - suffix.size()) + suffix;
    unsigned int copy_index = 2;
    while (std::any_of(objects_.begin(), objects_.end(),
        [&duplicate](const SelectableObject& object) {
            return object.name == duplicate.name;
        })) {
        const std::string numbered_suffix = " Copy " + std::to_string(copy_index++);
        duplicate.name = source->name.substr(
            0, 64 - std::min<std::size_t>(64, numbered_suffix.size())) + numbered_suffix;
    }
    const SelectionId duplicate_id = duplicate.id;
    objects_.push_back(std::move(duplicate));
    selected_ = duplicate_id;
    return duplicate_id;
}

bool SelectionManager::delete_selected()
{
    if (!selected_) {
        return false;
    }
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [this](const SelectableObject& candidate) { return candidate.id == *selected_; });
    if (object == objects_.end()) {
        selected_.reset();
        return false;
    }
    objects_.erase(object);
    selected_.reset();
    return true;
}

bool SelectionManager::set_transform(
    SelectionId id,
    glm::vec3 position,
    glm::vec3 rotation_degrees,
    glm::vec3 scale)
{
    if (!is_finite(position) || !is_finite(rotation_degrees) ||
        !is_finite(scale) || glm::any(glm::lessThanEqual(scale, glm::vec3{0.0f}))) {
        throw std::invalid_argument("object transforms must be finite and scale positive");
    }
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [id](const SelectableObject& candidate) { return candidate.id == id; });
    if (object == objects_.end()) {
        return false;
    }
    if (object->position == position &&
        object->rotation_degrees == rotation_degrees && object->scale == scale) {
        return false;
    }
    object->position = position;
    object->rotation_degrees = rotation_degrees;
    object->scale = scale;
    return true;
}

TransformMode SelectionManager::transform_mode() const
{
    return transform_mode_;
}

TransformAxis SelectionManager::transform_axis() const
{
    return transform_axis_;
}

glm::vec3 SelectionManager::transform_axis_direction() const
{
    const glm::vec3 axis = axis_vector(transform_axis_);
    if (transform_axis_ == TransformAxis::uniform ||
        transform_space_ == TransformSpace::world || !selected_) {
        return glm::normalize(axis);
    }
    const auto object = object_for(*selected_);
    if (!object) {
        return glm::normalize(axis);
    }
    return glm::normalize(glm::mat3{object_matrix(*object)} * axis);
}

bool SelectionManager::snapping_enabled() const
{
    return snapping_enabled_;
}

TransformSpace SelectionManager::transform_space() const
{
    return transform_space_;
}

void SelectionManager::set_transform_mode(TransformMode mode)
{
    transform_mode_ = mode;
}

void SelectionManager::set_transform_axis(TransformAxis axis)
{
    transform_axis_ = axis;
}

void SelectionManager::set_transform_space(TransformSpace space)
{
    transform_space_ = space;
}

void SelectionManager::set_snapping_enabled(bool enabled)
{
    snapping_enabled_ = enabled;
}

void SelectionManager::transform_selected(
    glm::vec3 translation,
    glm::vec3 rotation_degrees,
    glm::vec3 scale_delta)
{
    if (!selected_) {
        return;
    }
    if (!is_finite(translation) || !is_finite(rotation_degrees) ||
        !is_finite(scale_delta)) {
        throw std::invalid_argument("object transform values must be finite");
    }
    const auto object = std::find_if(objects_.begin(), objects_.end(),
        [this](const SelectableObject& candidate) { return candidate.id == *selected_; });
    if (object == objects_.end()) {
        selected_.reset();
        return;
    }

    const glm::vec3 axis = axis_vector(transform_axis_);
    switch (transform_mode_) {
    case TransformMode::translate: {
        glm::vec3 applied_translation = translation;
        glm::vec3 world_axis = axis;
        if (transform_axis_ != TransformAxis::uniform) {
            if (transform_space_ == TransformSpace::local) {
                world_axis = transform_axis_direction();
            }
            applied_translation = world_axis * glm::dot(translation, world_axis);
        }
        object->position += applied_translation;
        if (snapping_enabled_) {
            if (transform_axis_ == TransformAxis::uniform) {
                object->position = {
                    snap_value(object->position.x, translation_snap_),
                    snap_value(object->position.y, translation_snap_),
                    snap_value(object->position.z, translation_snap_),
                };
            } else if (transform_space_ == TransformSpace::world) {
                const int component = static_cast<int>(transform_axis_);
                object->position[component] =
                    snap_value(object->position[component], translation_snap_);
            } else {
                const float coordinate = glm::dot(object->position, world_axis);
                object->position += world_axis *
                    (snap_value(coordinate, translation_snap_) - coordinate);
            }
        }
        break;
    }
    case TransformMode::rotate:
        if (transform_axis_ == TransformAxis::uniform) {
            object->rotation_degrees += rotation_degrees;
            if (snapping_enabled_) {
                object->rotation_degrees = {
                    snap_value(object->rotation_degrees.x, rotation_snap_degrees_),
                    snap_value(object->rotation_degrees.y, rotation_snap_degrees_),
                    snap_value(object->rotation_degrees.z, rotation_snap_degrees_),
                };
            }
        } else {
            const int component = static_cast<int>(transform_axis_);
            object->rotation_degrees[component] += rotation_degrees[component];
            if (snapping_enabled_) {
                object->rotation_degrees[component] = snap_value(
                    object->rotation_degrees[component], rotation_snap_degrees_);
            }
        }
        break;
    case TransformMode::scale:
        if (transform_axis_ == TransformAxis::uniform) {
            const float amount =
                (scale_delta.x + scale_delta.y + scale_delta.z) / 3.0f;
            object->scale += glm::vec3{amount};
            if (snapping_enabled_) {
                object->scale = {
                    snap_value(object->scale.x, scale_snap_),
                    snap_value(object->scale.y, scale_snap_),
                    snap_value(object->scale.z, scale_snap_),
                };
            }
        } else {
            const int component = static_cast<int>(transform_axis_);
            object->scale[component] += scale_delta[component];
            if (snapping_enabled_) {
                object->scale[component] =
                    snap_value(object->scale[component], scale_snap_);
            }
        }
        object->scale = glm::max(object->scale, glm::vec3{scale_snap_});
        break;
    }
}

}
