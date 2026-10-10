#pragma once

#include "assets/model.hpp"
#include "character/character.hpp"

#include <cstddef>

namespace mmo::app {

// Appends a reversed Walk_Loop as "Walk_Backward_Loop" (once) and returns its clip index.
std::size_t add_backward_walk_clip(assets::Model& model);
// Binds the UAL locomotion clips to the animation states and starts in Idle.
void bind_animations(character::Character& character);

}
