#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mmo::math {
// Right-handed world, Y up, character forward +Z. Angles are radians.
// GLM matrices are column-major; transforms compose parent * local.
using Vec3 = glm::vec3;
using Mat4 = glm::mat4;
inline Mat4 identity_matrix() { return Mat4{1.0f}; }
inline Mat4 multiply(const Mat4& a, const Mat4& b) { return a * b; }
inline Mat4 translation_matrix(Vec3 v) { return glm::translate(Mat4{1.0f}, v); }
inline Mat4 scale_matrix(Vec3 v) { return glm::scale(Mat4{1.0f}, v); }
inline Mat4 rotation_x_matrix(float a) { return glm::rotate(Mat4{1.0f}, a, Vec3{1, 0, 0}); }
inline Mat4 rotation_y_matrix(float a) { return glm::rotate(Mat4{1.0f}, a, Vec3{0, 1, 0}); }
inline Mat4 rotation_z_matrix(float a) { return glm::rotate(Mat4{1.0f}, a, Vec3{0, 0, 1}); }
}
