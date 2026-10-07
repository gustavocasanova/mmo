#pragma once

#include "game/world/movement_bounds.hpp"
#include "math/transform.hpp"

#include <vector>

namespace mmo::game::world {

struct CollisionBox {
    math::Vec3 minimum;
    math::Vec3 maximum;
};

class CollisionWorld {
public:
    explicit CollisionWorld(
        MovementBounds bounds = {},
        std::vector<CollisionBox> boxes = {});

    [[nodiscard]] const MovementBounds& bounds() const;
    [[nodiscard]] const std::vector<CollisionBox>& boxes() const;
    [[nodiscard]] float camera_distance(
        math::Vec3 origin,
        math::Vec3 destination,
        float radius) const;
    [[nodiscard]] math::Vec3 move_character(
        math::Vec3 position,
        math::Vec3 displacement,
        float radius,
        float height = 1.8f) const;

private:
    MovementBounds bounds_;
    std::vector<CollisionBox> boxes_;
};

}
