#include "presentation/equipment_presenter.hpp"

#include "equipment/attachment_pose.hpp"

namespace mmo::presentation {

EquipmentPresenter::EquipmentPresenter(
    const assets::AssetSystem& assets, renderer::Renderer& renderer)
    : assets_(assets), renderer_(renderer)
{
}

std::vector<renderer::RigidAttachment> EquipmentPresenter::attachments(
    const equipment::EquipmentManager& equipment, const animation::Pose& pose)
{
    std::vector<renderer::RigidAttachment> draws;
    for (const auto& attachment : equipment.resolved_attachments()) {
        const auto& definition = equipment.get_equipped(attachment.slot)->definition();
        const std::string path = definition.model_path.generic_string();
        if (!loaded_models_.contains(path)) {
            (void)renderer_.register_static_model(path, assets_.load_model(definition.model_path));
            loaded_models_.insert(path);
        }
        draws.push_back({path, equipment::attachment_matrix(attachment, pose)});
    }
    return draws;
}

}
