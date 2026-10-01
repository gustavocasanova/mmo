#include "game/client/demo_scene.hpp"
#include "assets/procedural_meshes.hpp"

namespace mmo::game::client {
DemoScene::DemoScene(assets::MeshCatalog& catalog)
    : ground_(catalog.add(assets::make_ground())), character_(catalog.add(assets::make_cube()))
{
    draws_.reserve(11);
}
std::span<const scene::DrawItem> DemoScene::update(float delta_seconds, const characters::Character& character)
{
    draws_.clear();
    draws_.push_back({ground_, math::Mat4{1.0f}, {math::Vec3{1.0f}, scene::SurfacePattern::checker_grid}});
    character_.update(delta_seconds, character.walking());
    character_.append_draws(character, draws_);
    return draws_;
}
}
