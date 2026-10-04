#pragma once

#include "assets/model.hpp"

namespace mmo::assets {

class GltfModelLoader final : public ModelLoader {
public:
    [[nodiscard]] Model load(const std::filesystem::path& path) const override;
};

}
