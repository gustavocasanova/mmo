#pragma once
#include "assets/mesh_catalog.hpp"
#include "game/client/character_visual.hpp"
#include "scene/draw_item.hpp"
#include <span>
#include <vector>

namespace mmo::game::client {
// Composes the current prototype. Content decisions live here, never in Renderer.
class DemoScene {
public:
    explicit DemoScene(assets::MeshCatalog& catalog);
    std::span<const scene::DrawItem> update(float delta_seconds, const characters::Character& character);
private:
    assets::MeshId ground_;
    CharacterVisual character_;
    std::vector<scene::DrawItem> draws_;
};
}
