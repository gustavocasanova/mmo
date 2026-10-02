#pragma once

#include "animation/animation_controller.hpp"
#include "character/character_body.hpp"
#include "equipment/equipment.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace mmo::character {

struct CharacterAppearance {
    std::string species = "human";
    std::uint32_t body_variant = 0;
    std::array<float, 3> skin_color{0.72f, 0.48f, 0.32f};
    std::array<float, 3> hair_color{0.18f, 0.12f, 0.08f};
    std::array<float, 3> eye_color{0.20f, 0.35f, 0.48f};
};

class CharacterModel {
public:
    explicit CharacterModel(std::shared_ptr<const assets::Model> body_model);

    [[nodiscard]] const CharacterBody& body() const;
    [[nodiscard]] equipment::EquipmentManager& equipment();
    [[nodiscard]] const equipment::EquipmentManager& equipment() const;
    [[nodiscard]] animation::AnimationController& animation();
    [[nodiscard]] const animation::AnimationController& animation() const;
    [[nodiscard]] CharacterAppearance& appearance();
    [[nodiscard]] const CharacterAppearance& appearance() const;
    void update(float delta_seconds);

private:
    CharacterBody body_;
    equipment::EquipmentManager equipment_;
    animation::AnimationController animation_;
    CharacterAppearance appearance_;
};

}