#include "character/character_body.hpp"

#include <stdexcept>
#include <utility>

namespace mmo::character {

CharacterBody::CharacterBody(std::shared_ptr<const assets::Model> model)
    : model_(std::move(model))
{
    if (!model_) {
        throw std::invalid_argument("character body requires a model asset");
    }
    std::string reason;
    if (!model_->validate(&reason)) {
        throw std::invalid_argument("character body model is invalid: " + reason);
    }
}

const assets::Model& CharacterBody::model() const
{
    return *model_;
}

const animation::Skeleton& CharacterBody::skeleton() const
{
    return model_->skeleton;
}

}