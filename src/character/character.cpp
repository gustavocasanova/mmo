#include "character/character.hpp"

#include <algorithm>
#include <utility>

namespace mmo::character {

Character::Character(std::shared_ptr<const assets::Model> body_model)
    : model_(std::move(body_model))
{
}

CharacterTransform& Character::transform()
{
    return transform_;
}

const CharacterTransform& Character::transform() const
{
    return transform_;
}

CharacterStats Character::stats() const
{
    return stats_;
}

void Character::set_stats(CharacterStats stats)
{
    stats.max_health = std::max(stats.max_health, 1.0f);
    stats.health = std::clamp(stats.health, 0.0f, stats.max_health);
    stats.level = std::max(stats.level, 1U);
    stats_ = stats;
}

CharacterModel& Character::model()
{
    return model_;
}

const CharacterModel& Character::model() const
{
    return model_;
}

}