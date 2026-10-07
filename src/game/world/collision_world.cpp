#include "game/world/collision_world.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace mmo::game::world {
namespace {

bool finite(math::Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool segment_box_intersection(
    math::Vec3 origin,
    math::Vec3 destination,
    math::Vec3 minimum,
    math::Vec3 maximum,
    bool horizontal_only,
    float& entry)
{
    float lower = 0.0f;
    float upper = 1.0f;
    const int axes = horizontal_only ? 2 : 3;
    for (int component = 0; component < axes; ++component) {
        const int axis = horizontal_only && component == 1 ? 2 : component;
        const float start = origin[axis];
        const float delta = destination[axis] - start;
        if (std::abs(delta) <= 0.000001f) {
            if (start < minimum[axis] || start > maximum[axis]) {
                return false;
            }
            continue;
        }
        float first = (minimum[axis] - start) / delta;
        float second = (maximum[axis] - start) / delta;
        if (first > second) {
            std::swap(first, second);
        }
        lower = std::max(lower, first);
        upper = std::min(upper, second);
        if (lower > upper) {
            return false;
        }
    }
    entry = lower;
    return upper >= 0.0f && lower <= 1.0f;
}

}

CollisionWorld::CollisionWorld(MovementBounds bounds, std::vector<CollisionBox> boxes)
    : bounds_(bounds), boxes_(std::move(boxes))
{
    if (!std::isfinite(bounds_.min_x) || !std::isfinite(bounds_.max_x) ||
        !std::isfinite(bounds_.min_z) || !std::isfinite(bounds_.max_z) ||
        bounds_.min_x >= bounds_.max_x || bounds_.min_z >= bounds_.max_z) {
        throw std::invalid_argument("collision world bounds must be finite and non-empty");
    }
    for (const CollisionBox& box : boxes_) {
        if (!finite(box.minimum) || !finite(box.maximum) ||
            box.minimum.x >= box.maximum.x || box.minimum.y >= box.maximum.y ||
            box.minimum.z >= box.maximum.z) {
            throw std::invalid_argument("collision boxes must be finite and non-empty");
        }
    }
}

const MovementBounds& CollisionWorld::bounds() const
{
    return bounds_;
}

const std::vector<CollisionBox>& CollisionWorld::boxes() const
{
    return boxes_;
}

float CollisionWorld::camera_distance(
    math::Vec3 origin,
    math::Vec3 destination,
    float radius) const
{
    if (!finite(origin) || !finite(destination) ||
        !std::isfinite(radius) || radius < 0.0f) {
        throw std::invalid_argument("camera collision query must be finite");
    }
    const math::Vec3 segment = destination - origin;
    const float length = glm::length(segment);
    if (length <= 0.000001f) {
        return 0.0f;
    }

    float nearest = length;
    for (const CollisionBox& box : boxes_) {
        const math::Vec3 expansion{radius};
        float entry = 0.0f;
        if (segment_box_intersection(origin, destination,
                box.minimum - expansion, box.maximum + expansion, false, entry)) {
            nearest = std::min(nearest, std::max(0.0f, entry * length));
        }
    }
    const float ground_height = radius + 0.05f;
    if (origin.y > ground_height && destination.y < ground_height) {
        const float entry = (origin.y - ground_height) / (origin.y - destination.y);
        nearest = std::min(nearest, entry * length);
    }
    return nearest;
}

math::Vec3 CollisionWorld::move_character(
    math::Vec3 position,
    math::Vec3 displacement,
    float radius,
    float height) const
{
    if (!finite(position) || !finite(displacement) ||
        !std::isfinite(radius) || radius < 0.0f ||
        !std::isfinite(height) || height <= 0.0f) {
        throw std::invalid_argument("character collision query must be finite");
    }

    const auto move_axis = [this, radius, height](
        math::Vec3 current, float amount, int axis) {
        if (std::abs(amount) <= 0.000001f) {
            return current;
        }
        math::Vec3 candidate = current;
        candidate[axis] += amount;
        float earliest = 1.0f;
        bool blocked = false;
        for (const CollisionBox& box : boxes_) {
            if (current.y + height <= box.minimum.y ||
                current.y >= box.maximum.y) {
                continue;
            }
            math::Vec3 minimum = box.minimum;
            math::Vec3 maximum = box.maximum;
            minimum.x -= radius;
            maximum.x += radius;
            minimum.z -= radius;
            maximum.z += radius;
            float entry = 0.0f;
            if (segment_box_intersection(current, candidate,
                    minimum, maximum, true, entry) && entry < earliest) {
                earliest = entry;
                blocked = true;
            }
        }
        if (blocked) {
            candidate = current;
            candidate[axis] += amount * std::max(0.0f, earliest - 0.001f);
        }
        return candidate;
    };

    position = move_axis(position, displacement.x, 0);
    position = move_axis(position, displacement.z, 2);
    const float radius_x = std::min(radius, (bounds_.max_x - bounds_.min_x) * 0.5f);
    const float radius_z = std::min(radius, (bounds_.max_z - bounds_.min_z) * 0.5f);
    position.x = std::clamp(
        position.x, bounds_.min_x + radius_x, bounds_.max_x - radius_x);
    position.z = std::clamp(
        position.z, bounds_.min_z + radius_z, bounds_.max_z - radius_z);
    return position;
}

}
