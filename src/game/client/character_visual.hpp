#pragma once
#include "game/characters/character.hpp"
#include "scene/draw_item.hpp"
#include <vector>

namespace mmo::game::client {
class CharacterVisual {
public:
    explicit CharacterVisual(assets::MeshId cube) : cube_(cube) {}
    void update(float delta_seconds, bool walking);
    void append_draws(const characters::Character& character, std::vector<scene::DrawItem>& draws) const;
private:
    assets::MeshId cube_;
    float walk_phase_ = 0.0f;
};
}
