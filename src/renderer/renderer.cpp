#include "renderer/renderer.hpp"

#include "platform/window.hpp"
#include "renderer/camera.hpp"
#include "renderer/character.hpp"
#include "renderer/material.hpp"
#include "renderer/mesh.hpp"
#include "renderer/shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace mmo::renderer {
namespace {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    std::array<float, 16> values{};
};

struct Mat3 {
    std::array<float, 9> values{};
};

Mat4 identity_matrix()
{
    Mat4 result;
    result.values[0] = 1.0f;
    result.values[5] = 1.0f;
    result.values[10] = 1.0f;
    result.values[15] = 1.0f;
    return result;
}

Mat4 from_glm(const glm::mat4& matrix)
{
    Mat4 result;
    const float* values = glm::value_ptr(matrix);
    std::copy(values, values + 16, result.values.begin());
    return result;
}

Mat4 multiply(const Mat4& left, const Mat4& right)
{
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int index = 0; index < 4; ++index) {
                result.values[column * 4 + row] +=
                    left.values[index * 4 + row] * right.values[column * 4 + index];
            }
        }
    }
    return result;
}

Mat4 translation_matrix(Vec3 offset)
{
    Mat4 result = identity_matrix();
    result.values[12] = offset.x;
    result.values[13] = offset.y;
    result.values[14] = offset.z;
    return result;
}

Mat4 scale_matrix(Vec3 scale)
{
    Mat4 result;
    result.values[0] = scale.x;
    result.values[5] = scale.y;
    result.values[10] = scale.z;
    result.values[15] = 1.0f;
    return result;
}

Mat4 rotation_x_matrix(float angle)
{
    Mat4 result = identity_matrix();
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    result.values[5] = cosine;
    result.values[6] = sine;
    result.values[9] = -sine;
    result.values[10] = cosine;
    return result;
}

Mat4 rotation_y_matrix(float angle)
{
    Mat4 result = identity_matrix();
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    result.values[0] = cosine;
    result.values[2] = -sine;
    result.values[8] = sine;
    result.values[10] = cosine;
    return result;
}

Vec3 subtract(Vec3 left, Vec3 right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 multiply(Vec3 value, float scalar)
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float dot(Vec3 left, Vec3 right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(Vec3 left, Vec3 right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

Vec3 normalize(Vec3 value)
{
    const float length_squared = dot(value, value);
    if (length_squared <= 0.000001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return multiply(value, 1.0f / std::sqrt(length_squared));
}

Mat3 normal_matrix(const Mat4& model)
{
    const float a = model.values[0];
    const float b = model.values[4];
    const float c = model.values[8];
    const float d = model.values[1];
    const float e = model.values[5];
    const float f = model.values[9];
    const float g = model.values[2];
    const float h = model.values[6];
    const float i = model.values[10];
    const float determinant = a * (e * i - f * h) -
        b * (d * i - f * g) + c * (d * h - e * g);

    if (std::abs(determinant) <= 0.000001f) {
        Mat3 result;
        result.values[0] = 1.0f;
        result.values[4] = 1.0f;
        result.values[8] = 1.0f;
        return result;
    }

    const float inverse_determinant = 1.0f / determinant;
    Mat3 result;
    result.values[0] = (e * i - f * h) * inverse_determinant;
    result.values[1] = (f * g - d * i) * inverse_determinant;
    result.values[2] = (d * h - e * g) * inverse_determinant;
    result.values[3] = (c * h - b * i) * inverse_determinant;
    result.values[4] = (a * i - c * g) * inverse_determinant;
    result.values[5] = (b * g - a * h) * inverse_determinant;
    result.values[6] = (b * f - c * e) * inverse_determinant;
    result.values[7] = (c * d - a * f) * inverse_determinant;
    result.values[8] = (a * e - b * d) * inverse_determinant;
    return result;
}

Mat4 perspective_matrix(float field_of_view, float aspect_ratio, float near_plane, float far_plane)
{
    const float scale = 1.0f / std::tan(field_of_view * 0.5f);
    Mat4 result;
    result.values[0] = scale / aspect_ratio;
    result.values[5] = scale;
    result.values[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.values[11] = -1.0f;
    result.values[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

Mat4 look_at_matrix(Vec3 eye, Vec3 target, Vec3 up)
{
    const Vec3 forward = normalize(subtract(target, eye));
    const Vec3 side = normalize(cross(forward, up));
    const Vec3 corrected_up = cross(side, forward);

    Mat4 result = identity_matrix();
    result.values[0] = side.x;
    result.values[1] = corrected_up.x;
    result.values[2] = -forward.x;
    result.values[4] = side.y;
    result.values[5] = corrected_up.y;
    result.values[6] = -forward.y;
    result.values[8] = side.z;
    result.values[9] = corrected_up.z;
    result.values[10] = -forward.z;
    result.values[12] = -dot(side, eye);
    result.values[13] = -dot(corrected_up, eye);
    result.values[14] = dot(forward, eye);
    return result;
}

using Vertex = MeshVertex;

void append_vertex(std::vector<Vertex>& vertices, Vec3 position, Vec3 normal, Vec3 color)
{
    vertices.push_back({
        {position.x, position.y, position.z},
        {normal.x, normal.y, normal.z},
        {color.x, color.y, color.z},
    });
}

void append_quad(
    std::vector<Vertex>& vertices,
    Vec3 first,
    Vec3 second,
    Vec3 third,
    Vec3 fourth,
    Vec3 normal,
    Vec3 color)
{
    append_vertex(vertices, first, normal, color);
    append_vertex(vertices, second, normal, color);
    append_vertex(vertices, third, normal, color);
    append_vertex(vertices, first, normal, color);
    append_vertex(vertices, third, normal, color);
    append_vertex(vertices, fourth, normal, color);
}

std::vector<Vertex> make_ground_vertices()
{
    std::vector<Vertex> vertices;
    vertices.reserve(6);
    constexpr Vec3 normal{0.0f, 1.0f, 0.0f};
    constexpr Vec3 color{1.0f, 1.0f, 1.0f};
    append_quad(vertices,
        {-48.0f, 0.0f, -48.0f},
        {-48.0f, 0.0f, 48.0f},
        {48.0f, 0.0f, 48.0f},
        {48.0f, 0.0f, -48.0f},
        normal, color);
    return vertices;
}

constexpr char kVertexShaderSource[] = R"(#version 330 core
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_color;
layout (location = 3) in vec4 a_bone_indices;
layout (location = 4) in vec4 a_bone_weights;
out vec3 v_color;
out vec3 v_normal;
out vec3 v_world_position;
uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_bone_matrices[48];
uniform int u_is_skinned;
uniform vec3 u_normal_column0;
uniform vec3 u_normal_column1;
uniform vec3 u_normal_column2;
uniform vec3 u_tint;
void main() {
    mat4 skin = mat4(1.0);
    if (u_is_skinned == 1) {
        skin = mat4(0.0);
        for (int influence = 0; influence < 4; ++influence) {
            if (a_bone_weights[influence] > 0.0) {
                int bone = int(a_bone_indices[influence]);
                skin += u_bone_matrices[bone] * a_bone_weights[influence];
            }
        }
    }
    vec4 world_position = u_model * skin * vec4(a_position, 1.0);
    v_world_position = world_position.xyz;
    vec3 skinned_normal = mat3(skin) * a_normal;
    v_normal = normalize(
        u_normal_column0 * skinned_normal.x +
        u_normal_column1 * skinned_normal.y +
        u_normal_column2 * skinned_normal.z);
    v_color = a_color * u_tint;
    gl_Position = u_projection * u_view * world_position;
}
)";

constexpr char kFragmentShaderSource[] = R"(#version 330 core
in vec3 v_color;
in vec3 v_normal;
in vec3 v_world_position;
out vec4 out_color;
uniform int u_is_ground;
void main() {
    vec3 surface_color = v_color;
    if (u_is_ground == 1) {
        float checker = mod(floor(v_world_position.x) + floor(v_world_position.z), 2.0);
        vec3 grass = mix(vec3(0.22, 0.31, 0.18), vec3(0.27, 0.36, 0.22), checker);
        vec2 tile_uv = fract(v_world_position.xz);
        float edge = min(min(tile_uv.x, 1.0 - tile_uv.x), min(tile_uv.y, 1.0 - tile_uv.y));
        float cell_shade = mix(0.92, 1.0, checker);
        surface_color = mix(vec3(0.16, 0.24, 0.14), grass, smoothstep(0.0, 0.035, edge))
            * v_color * cell_shade;
    }
    float diffuse = max(dot(normalize(v_normal), normalize(vec3(-0.4, 1.0, 0.3))), 0.0);
    float lighting = 0.38 + diffuse * 0.62;
    out_color = vec4(surface_color * lighting, 1.0);
}
)";

void log_gl_info()
{
    const auto* version = glGetString(GL_VERSION);
    const auto* vendor = glGetString(GL_VENDOR);
    const auto* renderer = glGetString(GL_RENDERER);
    const auto* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);

    std::cout << "[gl] version  : " << (version != nullptr ? reinterpret_cast<const char*>(version) : "?") << '\n';
    std::cout << "[gl] vendor   : " << (vendor != nullptr ? reinterpret_cast<const char*>(vendor) : "?") << '\n';
    std::cout << "[gl] renderer : " << (renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "?") << '\n';
    std::cout << "[gl] glsl     : " << (glsl != nullptr ? reinterpret_cast<const char*>(glsl) : "?") << '\n';
}

}

struct Renderer::Impl {
    Camera camera;
    Shader program{kVertexShaderSource, kFragmentShaderSource};
    Mesh ground{make_ground_vertices()};
    CharacterEquipment equipment{make_default_character_equipment()};
    Mesh character{make_character_vertices(equipment)};
    std::vector<glm::mat4> skinning_matrices;
    Material ground_material{{1.0f, 1.0f, 1.0f}, true};
    Material character_material{{1.0f, 1.0f, 1.0f}, false};
};

Renderer::Renderer()
{
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(platform::Window::get_proc_address)) == 0) {
        throw std::runtime_error("gladLoadGL failed; could not load OpenGL 3.3 entry points");
    }

    log_gl_info();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    impl_ = std::make_unique<Impl>();
}

Renderer::~Renderer() = default;

void Renderer::set_character_equipment(const CharacterEquipment& equipment)
{
    impl_->equipment = equipment;
    impl_->character.update(make_character_vertices(impl_->equipment));
}

void Renderer::set_skinning_matrices(std::span<const animation::Matrix4> matrices)
{
    if (matrices.size() > kMaxSkinningBones) {
        throw std::length_error("OpenGL skinning palette exceeds the 48-bone limit");
    }
    impl_->skinning_matrices.clear();
    impl_->skinning_matrices.reserve(matrices.size());
    for (const animation::Matrix4& matrix : matrices) {
        impl_->skinning_matrices.push_back(glm::make_mat4(matrix.data()));
    }
}

void Renderer::render(
    int framebuffer_width,
    int framebuffer_height,
    float player_x,
    float player_z,
    float player_yaw,
    const CameraView& camera) const
{
    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.48f, 0.66f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    impl_->program.bind();
    impl_->program.set_mat4_array("u_bone_matrices[0]", impl_->skinning_matrices);

    impl_->camera.set_projection(framebuffer_width, framebuffer_height);
    impl_->camera.set_view(
        {camera.eye_x, camera.eye_y, camera.eye_z},
        {camera.focus_x, camera.focus_y, camera.focus_z});
    const Mat4 projection = from_glm(impl_->camera.projection_matrix());
    const Mat4 view = from_glm(impl_->camera.view_matrix());
    const Mat4 ground_model = identity_matrix();

    const auto set_matrices = [this, &projection, &view](const Mat4& model) {
        const Mat3 normal = normal_matrix(model);
        impl_->program.set_mat4("u_projection", glm::make_mat4(projection.values.data()));
        impl_->program.set_mat4("u_view", glm::make_mat4(view.values.data()));
        impl_->program.set_mat4("u_model", glm::make_mat4(model.values.data()));
        impl_->program.set_vec3("u_normal_column0",
            {normal.values[0], normal.values[1], normal.values[2]});
        impl_->program.set_vec3("u_normal_column1",
            {normal.values[3], normal.values[4], normal.values[5]});
        impl_->program.set_vec3("u_normal_column2",
            {normal.values[6], normal.values[7], normal.values[8]});
    };

    set_matrices(ground_model);
    impl_->program.set_int("u_is_skinned", 0);
    impl_->ground_material.apply(impl_->program);
    impl_->ground.draw();

    const Mat4 character_model = multiply(
        translation_matrix({player_x, 0.0f, player_z}),
        rotation_y_matrix(player_yaw));
    set_matrices(character_model);
    impl_->program.set_int("u_is_skinned",
        impl_->character.is_skinned() && !impl_->skinning_matrices.empty() ? 1 : 0);
    impl_->character_material.apply(impl_->program);
    impl_->character.draw();
}

}