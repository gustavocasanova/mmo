#pragma once

#include "animation/skeleton.hpp"
#include "game/items/weapons.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace mmo::assets {
struct Model;
}

namespace mmo::character {
class Character;
}

namespace mmo::game::client {

[[nodiscard]] std::size_t add_backward_walk_clip(assets::Model& model);
[[nodiscard]] std::size_t find_clip_index(
    const std::vector<animation::AnimationClip>& clips,
    std::string_view name);
void compose_running_animations(assets::Model& model);
void bind_locomotion_animations(character::Character& character);
[[nodiscard]] std::optional<std::string_view> find_weapon_animation(
    const items::WeaponDefinition& weapon,
    const std::vector<animation::AnimationClip>& clips);
[[nodiscard]] bool has_animation(
    const std::vector<animation::AnimationClip>& clips,
    std::string_view name);

}