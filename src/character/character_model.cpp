#include "character/character_model.hpp"

#include <utility>

namespace mmo::character {
namespace {

// Candidate names for the first torso bone, checked against the real skeleton at load time.
constexpr std::array<std::string_view, 3> kUpperBodyRootCandidates{
    "spine_01", "Spine", "spine"};

}

CharacterModel::CharacterModel(std::shared_ptr<const assets::Model> body_model)
    : body_(std::move(body_model))
    , equipment_(body_.skeleton())
    , animation_(body_.skeleton(), body_.model().animations)
{
    for (const std::string_view candidate : kUpperBodyRootCandidates) {
        if (body_.skeleton().find_bone(candidate)) {
            animation_.set_upper_body_mask(
                animation::BoneMask::branch(body_.skeleton(), candidate));
            break;
        }
    }
}

const CharacterBody& CharacterModel::body() const
{
    return body_;
}

equipment::EquipmentManager& CharacterModel::equipment()
{
    return equipment_;
}

const equipment::EquipmentManager& CharacterModel::equipment() const
{
    return equipment_;
}

animation::AnimationController& CharacterModel::animation()
{
    return animation_;
}

const animation::AnimationController& CharacterModel::animation() const
{
    return animation_;
}

CharacterAppearance& CharacterModel::appearance()
{
    return appearance_;
}

const CharacterAppearance& CharacterModel::appearance() const
{
    return appearance_;
}

void CharacterModel::update(float delta_seconds)
{
    animation_.update(delta_seconds);
}

}