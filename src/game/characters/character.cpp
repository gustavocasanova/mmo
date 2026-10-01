#include "game/characters/character.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmo::game::characters {
void Character::rotate(float radians)
{
    if (!std::isfinite(radians)) throw std::invalid_argument("rotation must be finite");
    yaw_ = std::remainder(yaw_ + radians, 6.28318530f);
}
void Character::update(float delta_seconds, MovementInput input, const world::MovementBounds& bounds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0 ||
        !std::isfinite(input.forward) || !std::isfinite(input.strafe) ||
        !std::isfinite(bounds.min_x) || !std::isfinite(bounds.max_x) ||
        !std::isfinite(bounds.min_z) || !std::isfinite(bounds.max_z) ||
        bounds.min_x > bounds.max_x || bounds.min_z > bounds.max_z) {
        throw std::invalid_argument("invalid movement input or bounds");
    }
    const float forward = std::clamp(input.forward, -1.0f, 1.0f);
    const float strafe = std::clamp(input.strafe, -1.0f, 1.0f);
    float x = std::sin(yaw_) * forward + std::cos(yaw_) * strafe;
    float z = std::cos(yaw_) * forward - std::sin(yaw_) * strafe;
    const float length = std::sqrt(x * x + z * z);
    walking_ = length > 0.0f;
    if (walking_) {
        x /= length;
        z /= length;
        position_.x = std::clamp(position_.x + x * move_speed_ * delta_seconds, bounds.min_x, bounds.max_x);
        position_.z = std::clamp(position_.z + z * move_speed_ * delta_seconds, bounds.min_z, bounds.max_z);
    }
}
}
