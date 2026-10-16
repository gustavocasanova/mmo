#include "equipment/attachment_pose.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmo::equipment {
animation::Matrix4 attachment_matrix(
    const ResolvedEquipmentAttachment& attachment, const animation::Pose& pose)
{
    if (attachment.bone_index >= pose.global_matrices.size()) {
        throw std::out_of_range("equipment attachment bone is absent from the pose");
    }
    const auto& t = attachment.local_transform;
    const glm::quat q{t.rotation_offset.w, t.rotation_offset.x,
        t.rotation_offset.y, t.rotation_offset.z};
    if (!std::isfinite(glm::dot(q,q)) || glm::dot(q,q) < 0.000001f) {
        throw std::invalid_argument("equipment attachment rotation is invalid");
    }
    const glm::mat4 local = glm::translate(glm::mat4{1},
        {t.position_offset[0],t.position_offset[1],t.position_offset[2]}) *
        glm::mat4_cast(glm::normalize(q)) * glm::scale(glm::mat4{1},
        {t.scale[0],t.scale[1],t.scale[2]});
    const glm::mat4 matrix = glm::make_mat4(pose.global_matrices[attachment.bone_index].data()) * local;
    animation::Matrix4 result;
    std::copy_n(glm::value_ptr(matrix), result.size(), result.begin());
    if (!std::all_of(result.begin(),result.end(),[](float v){ return std::isfinite(v); })) {
        throw std::invalid_argument("equipment attachment produced a non-finite matrix");
    }
    return result;
}
}
