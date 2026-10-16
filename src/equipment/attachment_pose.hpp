#pragma once
#include "equipment/equipment_types.hpp"

namespace mmo::equipment {
// Rigid attachments follow a joint's global pose, NOT its inverse-bind skin matrix.
// Result is in character/model space; placement and display scaling belong to rendering.
[[nodiscard]] animation::Matrix4 attachment_matrix(
    const ResolvedEquipmentAttachment& attachment, const animation::Pose& pose);
}
