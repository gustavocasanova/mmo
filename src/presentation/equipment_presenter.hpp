#pragma once

#include "assets/model.hpp"
#include "equipment/equipment_manager.hpp"
#include "renderer/renderer.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace mmo::presentation {

// Adapts CPU equipment/pose data to rendering; owns no gameplay rules.
class EquipmentPresenter {
public:
    EquipmentPresenter(const assets::AssetSystem& assets, renderer::Renderer& renderer);
    // Registers models on first use and returns this frame's rigid attachments.
    [[nodiscard]] std::vector<renderer::RigidAttachment> attachments(
        const equipment::EquipmentManager& equipment, const animation::Pose& pose);

private:
    const assets::AssetSystem& assets_;
    renderer::Renderer& renderer_;
    std::unordered_set<std::string> loaded_models_;
};

}
