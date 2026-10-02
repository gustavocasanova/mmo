#include "game/client/demo_scene.hpp"
#include "assets/procedural_meshes.hpp"

namespace mmo::game::client {
DemoScene::DemoScene(assets::MeshCatalog& catalog)
    : ground_(catalog.add(assets::make_ground())), character_(catalog.add({make_character_vertices(make_default_character_equipment())}))
{
    draws_.reserve(2);
}
assets::MeshId DemoScene::set_equipment(assets::MeshCatalog& catalog, const CharacterEquipment& equipment)
{
    catalog.replace(character_, {make_character_vertices(equipment)});
    return character_;
}
std::span<const scene::DrawItem> DemoScene::update(float delta_seconds, const characters::Character& character)
{
    (void)delta_seconds;
    draws_.clear();
    draws_.push_back({ground_, math::Mat4{1.0f}, {math::Vec3{1.0f}, scene::SurfacePattern::checker_grid}});
    draws_.push_back({character_, math::translation_matrix(character.position()) *
        math::rotation_y_matrix(character.yaw()), {math::Vec3{1.0f}, scene::SurfacePattern::solid}});
    return draws_;
}
}
