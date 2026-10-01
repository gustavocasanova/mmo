#pragma once
#include "math/transform.hpp"
#include "game/world/movement_bounds.hpp"

namespace mmo::game::characters {
struct MovementInput { float forward = 0; float strafe = 0; };
class Character {
public:
    const math::Vec3& position() const { return position_; }
    float yaw() const { return yaw_; }
    bool walking() const { return walking_; }
    void rotate(float radians);
    void update(float delta_seconds, MovementInput input, const world::MovementBounds& bounds);
private:
    math::Vec3 position_{0.0f};
    float yaw_ = 0.0f;
    bool walking_ = false;
    float move_speed_ = 4.2f;
};
}
