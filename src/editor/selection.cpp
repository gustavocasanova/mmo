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
{
    objects_.reserve(objects.size());
    std::unordered_set<SelectionId> ids;
    for (SelectableObject& object : objects) {
        if (object.id == 0 || !ids.insert(object.id).second ||
            !is_finite(object.local_bounds.minimum) ||
            !is_finite(object.local_bounds.maximum) ||
            glm::any(glm::greaterThan(
                object.local_bounds.minimum, object.local_bounds.maximum))) {
            throw std::invalid_argument("selectable objects require unique IDs and valid bounds");
        }
        const glm::vec3 center =
            (object.local_bounds.minimum + object.local_bounds.maximum) * 0.5f;
        object.position = center;
        object.local_bounds.minimum -= center;
        object.local_bounds.maximum -= center;
        objects_.push_back(std::move(object));
    }
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
        const glm::vec3 local_origin = glm::vec3{
            inverse_model * glm::vec4{origin, 1.0f}};
        const glm::vec3 local_direction = glm::normalize(glm::vec3{
            inverse_model * glm::vec4{direction, 0.0f}});
        const std::optional<float> local_distance = ray_box_distance(
            local_origin, local_direction, object.local_bounds,
            max_distance * std::max({object.scale.x, object.scale.y, object.scale.z}));
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

std::optional<SelectionBounds> SelectionManager::bounds_for(SelectionId id) const
{
    const auto object = object_for(id);
    return object ? std::optional<SelectionBounds>{world_bounds(*object)} : std::nullopt;
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

TransformMode SelectionManager::transform_mode() const
{
    return transform_mode_;
}

TransformAxis SelectionManager::transform_axis() const
{
    return transform_axis_;
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

void SelectionManager::set_transform_space(TransformSpace space)
{
    transform_space_ = space;
}

void SelectionManager::set_transform_axis(TransformAxis axis)
{
    transform_axis_ = axis;
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
    glm::vec3 axis = axis_vector(transform_axis_);
    if (transform_space_ == TransformSpace::local &&
        transform_axis_ != TransformAxis::uniform) {
        const glm::vec3 local_axis = axis;
        const glm::mat4 rotation = object_matrix(*object);
        axis = glm::normalize(glm::vec3{
            rotation * glm::vec4{local_axis / object->scale, 0.0f}});
    }
    switch (transform_mode_) {
    case TransformMode::translate:
        object->position += axis * glm::dot(translation, axis);
        if (snapping_enabled_) {
            object->position[static_cast<int>(transform_axis_)] =
                snap_value(object->position[static_cast<int>(transform_axis_)],
                    translation_snap_);
        }
        break;
    case TransformMode::rotate:
        if (transform_axis_ == TransformAxis::uniform) {
            object->rotation_degrees += rotation_degrees;
        } else {
            object->rotation_degrees += axis_vector(transform_axis_) *
                glm::dot(rotation_degrees, axis_vector(transform_axis_));
        }
        if (snapping_enabled_) {
            const int component = static_cast<int>(transform_axis_);
            object->rotation_degrees[component] =
                snap_value(object->rotation_degrees[component], rotation_snap_degrees_);
        }
        break;
    case TransformMode::scale:
        if (transform_axis_ == TransformAxis::uniform) {
            const float amount = (scale_delta.x + scale_delta.y + scale_delta.z) / 3.0f;
            object->scale += glm::vec3{amount};
            if (snapping_enabled_) {
                object->scale = {
                    snap_value(object->scale.x, scale_snap_),
                    snap_value(object->scale.y, scale_snap_),
                    snap_value(object->scale.z, scale_snap_),
                };
            }
            object->scale = glm::max(object->scale, glm::vec3{scale_snap_});
            break;
        }
        object->scale += axis_vector(transform_axis_) *
            glm::dot(scale_delta, axis_vector(transform_axis_));
        if (transform_axis_ == TransformAxis::x ||
            transform_axis_ == TransformAxis::y ||
            transform_axis_ == TransformAxis::z) {
            if (snapping_enabled_) {
                const int component = static_cast<int>(transform_axis_);
                object->scale[component] = snap_value(
                    object->scale[component], scale_snap_);
            }
            object->scale = glm::max(object->scale, glm::vec3{scale_snap_});
        }
        break;
    }
}

}
