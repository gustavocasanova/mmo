#pragma once
#include "math/transform.hpp"
namespace mmo::scene {
enum class SurfacePattern { solid, checker_grid };
struct Material {
    math::Vec3 tint{1.0f};
    SurfacePattern pattern = SurfacePattern::solid;
};
}
