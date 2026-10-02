#pragma once

#include "character/character_model.hpp"

#include <cstdint>
#include <memory>

namespace mmo::character {

struct CharacterTransform {
    animation::Vector3 position{0.0f, 0.0f, 0.0f};
    animation::Quaternion rotation{};
    animation::Vector3 scale{1.0f, 1.0f, 1.0f};
};

struct CharacterStats {
    float health = 100.0f;
    float max_health = 100.0f;
    std::uint32_t level = 1;
};

class Character {
public:
    explicit Character(std::shared_ptr<const assets::Model> body_model);

    [[nodiscard]] CharacterTransform& transform();
    [[nodiscard]] const CharacterTransform& transform() const;
    [[nodiscard]] CharacterStats stats() const;
    void set_stats(CharacterStats stats);
    [[nodiscard]] CharacterModel& model();
    [[nodiscard]] const CharacterModel& model() const;

private:
    CharacterTransform transform_;
    CharacterStats stats_;
    CharacterModel model_;
};

}