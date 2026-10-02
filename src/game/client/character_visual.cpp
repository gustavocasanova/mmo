#include "game/client/character_visual.hpp"
#include <cmath>

namespace mmo::game::client {
using namespace math;
namespace {
Mat4 object_matrix(const Mat4& actor, Vec3 position, Vec3 size)
{
    return multiply(actor, multiply(translation_matrix(position), scale_matrix(size)));
}

Mat4 limb_matrix(const Mat4& actor, float pivot_x, float swing)
{
    const Mat4 pivot = translation_matrix({pivot_x, 1.63f, 0.0f});
    const Mat4 rotation = multiply(rotation_z_matrix(pivot_x * 0.12f), rotation_x_matrix(swing));
    const Mat4 center = translation_matrix({0.0f, -0.34f, 0.0f});
    const Mat4 size = scale_matrix({0.22f, 0.70f, 0.25f});
    return multiply(actor, multiply(pivot, multiply(rotation, multiply(center, size))));
}

}
void CharacterVisual::update(float delta_seconds, bool walking)
{
    walk_phase_ = walking ? walk_phase_ + delta_seconds * 9.0f : 0.0f;
}
void CharacterVisual::append_draws(const characters::Character& character,
    std::vector<scene::DrawItem>& draws) const
{
    const Mat4 actor = translation_matrix(character.position()) * rotation_y_matrix(character.yaw());
    const auto draw_cube = [this, &draws](const Mat4& model, Vec3 color) {
        draws.push_back({cube_, model, {color, scene::SurfacePattern::solid}});
    };
    draw_cube(object_matrix(actor, {0.0f, 1.31f, 0.0f}, {0.56f, 0.86f, 0.38f}),
        {0.27f, 0.43f, 0.42f});
    draw_cube(object_matrix(actor, {0.0f, 0.96f, 0.0f}, {0.58f, 0.13f, 0.40f}),
        {0.36f, 0.25f, 0.16f});
    draw_cube(object_matrix(actor, {0.0f, 2.03f, 0.0f}, {0.46f, 0.48f, 0.43f}),
        {0.77f, 0.57f, 0.39f});
    draw_cube(object_matrix(actor, {0.0f, 2.23f, -0.015f}, {0.49f, 0.17f, 0.46f}),
        {0.24f, 0.19f, 0.14f});
    draw_cube(object_matrix(actor, {-0.105f, 2.08f, 0.218f}, {0.045f, 0.055f, 0.025f}),
        {0.12f, 0.10f, 0.08f});
    draw_cube(object_matrix(actor, {0.105f, 2.08f, 0.218f}, {0.045f, 0.055f, 0.025f}),
        {0.12f, 0.10f, 0.08f});

    const float leg_swing = std::sin(walk_phase_) * 0.48f;
    const Mat4 left_leg = multiply(actor, multiply(
        translation_matrix({-0.16f, 0.89f, 0.0f}), multiply(rotation_x_matrix(leg_swing),
            multiply(translation_matrix({0.0f, -0.42f, 0.0f}),
                scale_matrix({0.26f, 0.84f, 0.31f})))));
    const Mat4 right_leg = multiply(actor, multiply(
        translation_matrix({0.16f, 0.89f, 0.0f}), multiply(rotation_x_matrix(-leg_swing),
            multiply(translation_matrix({0.0f, -0.42f, 0.0f}),
                scale_matrix({0.26f, 0.84f, 0.31f})))));
    draw_cube(left_leg, {0.37f, 0.27f, 0.19f});
    draw_cube(right_leg, {0.37f, 0.27f, 0.19f});
    draw_cube(limb_matrix(actor, -0.39f, -leg_swing), {0.77f, 0.57f, 0.39f});
    draw_cube(limb_matrix(actor, 0.39f, leg_swing), {0.77f, 0.57f, 0.39f});
}
}
