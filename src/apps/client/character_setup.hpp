#pragma once

#include "assets/model.hpp"
#include "character/character.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace mmo::app {

// Appends a reversed Walk_Loop as "Walk_Backward_Loop" (once) and returns its clip index.
std::size_t add_backward_walk_clip(assets::Model& model);
// Index of the named clip; throws when the UAL model lacks it.
std::size_t find_clip_index(
    const std::vector<animation::AnimationClip>& clips, std::string_view name);
// Rebuilds the run clips with the optional melee pack's leg motion (lower body only).
void compose_running_animations(assets::Model& model);
// Binds the UAL locomotion clips to the animation states and starts in Idle.
void bind_animations(character::Character& character);

}
