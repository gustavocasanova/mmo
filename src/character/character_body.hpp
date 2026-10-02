#pragma once

#include "assets/model.hpp"

#include <memory>

namespace mmo::character {

class CharacterBody {
public:
    explicit CharacterBody(std::shared_ptr<const assets::Model> model);

    [[nodiscard]] const assets::Model& model() const;
    [[nodiscard]] const animation::Skeleton& skeleton() const;

private:
    std::shared_ptr<const assets::Model> model_;
};

}