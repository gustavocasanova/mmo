#include "renderer/renderer.hpp"

#include "platform/window.hpp"
#include "renderer/mesh.hpp"
#include "renderer/shader.hpp"
#include "scene/camera.hpp"
#include "scene/material.hpp"

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

std::vector<Vertex> make_model_vertices(const assets::Model& model)
{
    std::vector<Vertex> vertices;
    for (const assets::Mesh& mesh : model.meshes) {
        const auto append = [&vertices, &mesh](std::size_t index) {
            if (index >= mesh.vertices.size()) {
                throw std::runtime_error("validated model contains an invalid mesh index");
            }
            const assets::ModelVertex& source = mesh.vertices[index];
            vertices.push_back({
                {source.position[0], source.position[1], source.position[2]},
                {source.normal[0], source.normal[1], source.normal[2]},
                {source.color[0], source.color[1], source.color[2]},
                source.bone_indices,
                source.bone_weights,
            });
        };
        if (mesh.indices.empty()) {
            for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
                append(index);
            }
        } else {
            for (std::uint32_t index : mesh.indices) {
                append(index);
            }
        }
    }
    return vertices;
}

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

std::vector<Vertex> make_brush_ring_vertices()
{
    std::vector<Vertex> vertices;
    constexpr int segments = 48;
    vertices.reserve(segments * 6);
    constexpr float inner_ratio = 0.96f;
    constexpr float two_pi = 6.28318530718f;
    constexpr Vec3 normal{0.0f, 1.0f, 0.0f};
    constexpr Vec3 color{1.0f, 1.0f, 1.0f};
    for (int segment = 0; segment < segments; ++segment) {
        const float first_angle = two_pi * static_cast<float>(segment) / segments;
        const float second_angle = two_pi * static_cast<float>(segment + 1) / segments;
        const Vec3 outer_first{std::cos(first_angle), 0.0f, std::sin(first_angle)};
        const Vec3 outer_second{std::cos(second_angle), 0.0f, std::sin(second_angle)};
        const Vec3 inner_first{outer_first.x * inner_ratio, 0.0f,
            outer_first.z * inner_ratio};
        const Vec3 inner_second{outer_second.x * inner_ratio, 0.0f,
            outer_second.z * inner_ratio};
        append_vertex(vertices, outer_first, normal, color);
        append_vertex(vertices, inner_second, normal, color);
        append_vertex(vertices, outer_second, normal, color);
        append_vertex(vertices, outer_first, normal, color);
        append_vertex(vertices, inner_first, normal, color);
        append_vertex(vertices, inner_second, normal, color);
    }
    return vertices;
}

std::vector<Vertex> make_box_vertices()
{
    std::vector<Vertex> vertices;
    vertices.reserve(36);
    constexpr Vec3 color{1.0f, 1.0f, 1.0f};
    append_quad(vertices,
        {-0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f},
        {0.5f, 1.0f, 0.5f}, {-0.5f, 1.0f, 0.5f},
        {0.0f, 0.0f, 1.0f}, color);
    append_quad(vertices,
        {0.5f, 0.0f, -0.5f}, {-0.5f, 0.0f, -0.5f},
        {-0.5f, 1.0f, -0.5f}, {0.5f, 1.0f, -0.5f},
        {0.0f, 0.0f, -1.0f}, color);
    append_quad(vertices,
        {0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, -0.5f},
        {0.5f, 1.0f, -0.5f}, {0.5f, 1.0f, 0.5f},
        {1.0f, 0.0f, 0.0f}, color);
    append_quad(vertices,
        {-0.5f, 0.0f, -0.5f}, {-0.5f, 0.0f, 0.5f},
        {-0.5f, 1.0f, 0.5f}, {-0.5f, 1.0f, -0.5f},
        {-1.0f, 0.0f, 0.0f}, color);
    append_quad(vertices,
        {-0.5f, 1.0f, -0.5f}, {-0.5f, 1.0f, 0.5f},
        {0.5f, 1.0f, 0.5f}, {0.5f, 1.0f, -0.5f},
        {0.0f, 1.0f, 0.0f}, color);
    append_quad(vertices,
        {-0.5f, 0.0f, 0.5f}, {-0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, 0.5f},
        {0.0f, -1.0f, 0.0f}, color);
    return vertices;
}

std::vector<Vertex> make_placeholder_vertices()
{
    return {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{0.1f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{0.0f, 0.1f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    };
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
layout (std140) uniform SkinningMatrices {
    mat4 u_bone_matrices[66];
};
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
    scene::Camera camera;
    Shader program{kVertexShaderSource, kFragmentShaderSource};
    Mesh ground{make_ground_vertices()};
    Mesh brush_ring{make_brush_ring_vertices()};
    Mesh wall{make_box_vertices()};
    Mesh gizmo{make_box_vertices()};
    Mesh character{make_placeholder_vertices()};
    std::vector<glm::mat4> skinning_matrices;
    std::vector<glm::mat4> bind_pose_matrices;
    unsigned int skinning_buffer = 0;
    glm::vec3 character_offset{0.0f};
    float character_scale = 1.0f;
    scene::Material ground_material{{1.0f, 1.0f, 1.0f},
        scene::SurfacePattern::checker_grid};
    bool brush_cursor_visible = false;
    scene::Material character_material{{1.0f, 1.0f, 1.0f},
        scene::SurfacePattern::solid};

    Impl()
    {
        program.bind_uniform_block("SkinningMatrices", 0);
        glGenBuffers(1, &skinning_buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, skinning_buffer);
        glBufferData(GL_UNIFORM_BUFFER,
            static_cast<GLsizeiptr>(sizeof(glm::mat4) * kMaxSkinningBones),
            nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, skinning_buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    ~Impl()
    {
        if (skinning_buffer != 0) {
            glDeleteBuffers(1, &skinning_buffer);
        }
    }
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

void Renderer::set_character_model(const assets::Model& model)
{
    std::string reason;
    if (!model.validate(&reason)) {
        throw std::invalid_argument("cannot render invalid character model: " + reason);
    }
    const std::vector<MeshVertex> vertices = make_model_vertices(model);
    if (vertices.empty()) {
        throw std::invalid_argument("character model has no renderable vertices");
    }

    glm::vec3 minimum{vertices.front().position.x, vertices.front().position.y,
        vertices.front().position.z};
    glm::vec3 maximum = minimum;
    for (const MeshVertex& vertex : vertices) {
        minimum.x = std::min(minimum.x, vertex.position.x);
        minimum.y = std::min(minimum.y, vertex.position.y);
        minimum.z = std::min(minimum.z, vertex.position.z);
        maximum.x = std::max(maximum.x, vertex.position.x);
        maximum.y = std::max(maximum.y, vertex.position.y);
        maximum.z = std::max(maximum.z, vertex.position.z);
    }
    const float height = maximum.y - minimum.y;
    if (height <= 0.0001f) {
        throw std::invalid_argument("character model has no usable vertical bounds");
    }

    constexpr float kTargetCharacterHeight = 1.8f;
    impl_->character_scale = kTargetCharacterHeight / height;
    impl_->character_offset = {
        -(minimum.x + maximum.x) * 0.5f * impl_->character_scale,
        -minimum.y * impl_->character_scale,
        -(minimum.z + maximum.z) * 0.5f * impl_->character_scale,
    };
    impl_->character.update(vertices);
    const animation::Animator bind_pose(model.skeleton);
    impl_->bind_pose_matrices.reserve(bind_pose.pose().skin_matrices.size());
    for (const animation::Matrix4& matrix : bind_pose.pose().skin_matrices) {
        impl_->bind_pose_matrices.push_back(glm::make_mat4(matrix.data()));
    }
}

void Renderer::set_skinning_matrices(std::span<const animation::Matrix4> matrices)
{
    if (matrices.size() > kMaxSkinningBones) {
        throw std::length_error("OpenGL skinning palette exceeds its bone limit");
    }
    impl_->skinning_matrices.clear();
    impl_->skinning_matrices.reserve(matrices.size());
    for (const animation::Matrix4& matrix : matrices) {
        impl_->skinning_matrices.push_back(glm::make_mat4(matrix.data()));
    }
}

void Renderer::set_terrain(const game::world::Terrain& terrain)
{
    const std::vector<game::world::TerrainVertex> terrain_vertices = terrain.vertices();
    std::vector<MeshVertex> vertices;
    vertices.reserve(terrain_vertices.size());
    for (const game::world::TerrainVertex& vertex : terrain_vertices) {
        vertices.push_back({
            vertex.position,
            vertex.normal,
            {1.0f, 1.0f, 1.0f},
            {},
            {},
        });
    }
    impl_->ground.update(vertices);
}

void Renderer::set_brush_cursor(
    const game::world::Terrain& terrain,
    glm::vec2 center,
    float radius,
    bool visible)
{
    impl_->brush_cursor_visible = visible;
    if (!visible) {
        return;
    }
    if (!terrain.contains(center) || !std::isfinite(radius) || radius <= 0.0f) {
        throw std::invalid_argument("terrain brush cursor parameters are invalid");
    }

    constexpr int segments = 48;
    constexpr float inner_ratio = 0.96f;
    constexpr float two_pi = 6.28318530718f;
    constexpr float vertical_offset = 0.035f;
    std::vector<MeshVertex> vertices;
    vertices.reserve(segments * 6);
    const auto make_point = [&](float angle, float distance) {
        const glm::vec2 horizontal{
            std::clamp(center.x + std::cos(angle) * distance,
                -game::world::Terrain::kHalfExtent, game::world::Terrain::kHalfExtent),
            std::clamp(center.y + std::sin(angle) * distance,
                -game::world::Terrain::kHalfExtent, game::world::Terrain::kHalfExtent),
        };
        return glm::vec3{
            horizontal.x, terrain.height_at(horizontal) + vertical_offset, horizontal.y};
    };
    for (int segment = 0; segment < segments; ++segment) {
        const float first_angle = two_pi * static_cast<float>(segment) / segments;
        const float second_angle = two_pi * static_cast<float>(segment + 1) / segments;
        const glm::vec3 outer_first = make_point(first_angle, radius);
        const glm::vec3 outer_second = make_point(second_angle, radius);
        const glm::vec3 inner_first = make_point(first_angle, radius * inner_ratio);
        const glm::vec3 inner_second = make_point(second_angle, radius * inner_ratio);
        constexpr glm::vec3 normal{0.0f, 1.0f, 0.0f};
        constexpr glm::vec3 color{1.0f, 1.0f, 1.0f};
        const auto append = [&vertices, &normal, &color](
            glm::vec3 position) {
            vertices.push_back({position, normal, color, {}, {}});
        };
        append(outer_first);
        append(inner_second);
        append(outer_second);
        append(outer_first);
        append(inner_first);
        append(inner_second);
    }
    impl_->brush_ring.update(vertices);
}

void Renderer::render(
    int framebuffer_width,
    int framebuffer_height,
    const CharacterPlacement& player,
    const scene::CameraPose& camera,
    const game::world::CollisionWorld& collision_world,
    std::optional<std::uint64_t> selected_object,
    std::span<const RenderBox> editor_boxes,
    bool show_editor_gizmo,
    glm::vec3 gizmo_position,
    GizmoMode gizmo_mode) const
{
    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.48f, 0.66f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    impl_->program.bind();
    const auto upload_skinning_matrices = [this](const std::vector<glm::mat4>& matrices) {
        if (matrices.empty()) {
            return;
        }
        glBindBuffer(GL_UNIFORM_BUFFER, impl_->skinning_buffer);
        glBufferSubData(GL_UNIFORM_BUFFER, 0,
            static_cast<GLsizeiptr>(
                sizeof(glm::mat4) * matrices.size()),
            matrices.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    };
    upload_skinning_matrices(impl_->skinning_matrices);

    impl_->camera.set_pose(camera);
    const Mat4 projection = from_glm(impl_->camera.projection(
        static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height)));
    const Mat4 view = from_glm(impl_->camera.view());
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
    impl_->program.set_vec3("u_tint", impl_->ground_material.tint);
    impl_->program.set_int("u_is_ground",
        impl_->ground_material.pattern == scene::SurfacePattern::checker_grid ? 1 : 0);
    impl_->ground.draw();

    if (impl_->brush_cursor_visible) {
        impl_->program.set_int("u_is_skinned", 0);
        impl_->program.set_int("u_is_ground", 0);
        impl_->program.set_vec3("u_tint", {1.0f, 0.82f, 0.18f});
        impl_->brush_ring.draw();
    }

    impl_->program.set_int("u_is_skinned", 0);
    impl_->program.set_int("u_is_ground", 0);
    impl_->program.set_int("u_is_skinned", 0);
    if (!editor_boxes.empty()) {
        for (const RenderBox& box : editor_boxes) {
            glm::mat4 model{1.0f};
            model = glm::translate(model, box.center);
            model = glm::rotate(model, glm::radians(box.rotation_degrees.y),
                {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, glm::radians(box.rotation_degrees.x),
                {1.0f, 0.0f, 0.0f});
            model = glm::rotate(model, glm::radians(box.rotation_degrees.z),
                {0.0f, 0.0f, 1.0f});
            model = glm::scale(model, box.half_extents * 2.0f * box.scale);
            model = glm::translate(model, {0.0f, -0.5f, 0.0f});
            set_matrices(from_glm(model));
            const bool selected = selected_object && *selected_object == box.id;
            impl_->program.set_vec3("u_tint", selected
                ? glm::vec3{1.0f, 0.72f, 0.12f}
                : glm::vec3{0.46f, 0.39f, 0.30f});
            impl_->wall.draw();
        }
    } else {
        std::uint64_t object_id = 1;
        for (const game::world::CollisionBox& box : collision_world.boxes()) {
            const Vec3 center{
                (box.minimum.x + box.maximum.x) * 0.5f,
                box.minimum.y,
                (box.minimum.z + box.maximum.z) * 0.5f,
            };
            const Vec3 size{
                box.maximum.x - box.minimum.x,
                box.maximum.y - box.minimum.y,
                box.maximum.z - box.minimum.z,
            };
            set_matrices(multiply(translation_matrix(center), scale_matrix(size)));
            const bool selected = selected_object && *selected_object == object_id;
            impl_->program.set_vec3("u_tint", selected
                ? glm::vec3{1.0f, 0.72f, 0.12f}
                : glm::vec3{0.46f, 0.39f, 0.30f});
            impl_->wall.draw();
            ++object_id;
        }
    }

    if (show_editor_gizmo) {
        const glm::mat4 origin = glm::translate(glm::mat4{1.0f}, gizmo_position);
        constexpr float shaft_radius = 0.035f;
        constexpr float shaft_length = 1.25f;
        const std::array<glm::vec3, 3> colors{
            glm::vec3{0.95f, 0.12f, 0.10f},
            glm::vec3{0.18f, 0.92f, 0.18f},
            glm::vec3{0.12f, 0.38f, 1.0f},
        };
        const std::array<glm::vec3, 3> axes{
            glm::vec3{1.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, 1.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, 1.0f},
        };
        const std::array<glm::vec3, 3> rotation_axes{
            glm::vec3{0.0f, 0.0f, -1.57079633f},
            glm::vec3{0.0f},
            glm::vec3{1.57079633f, 0.0f, 0.0f},
        };
        if (gizmo_mode == GizmoMode::rotate) {
            glDisable(GL_CULL_FACE);
            impl_->program.set_int("u_is_skinned", 0);
            impl_->program.set_int("u_is_ground", 0);
            impl_->program.set_vec3("u_tint", {1.0f, 1.0f, 1.0f});
            for (int axis = 0; axis < 3; ++axis) {
                std::vector<MeshVertex> ring;
                constexpr int segments = 64;
                constexpr float inner_radius = 0.96f;
                constexpr float outer_radius = 1.0f;
                ring.reserve(segments * 6);
                const auto point = [axis](float angle, float radius) {
                    const float first = std::cos(angle) * radius;
                    const float second = std::sin(angle) * radius;
                    if (axis == 0) return glm::vec3{0.0f, first, second};
                    if (axis == 1) return glm::vec3{first, 0.0f, second};
                    return glm::vec3{first, second, 0.0f};
                };
                for (int segment = 0; segment < segments; ++segment) {
                    const float first_angle = 6.28318530718f * segment / segments;
                    const float second_angle = 6.28318530718f * (segment + 1) / segments;
                    const glm::vec3 outer_first = point(first_angle, outer_radius);
                    const glm::vec3 outer_second = point(second_angle, outer_radius);
                    const glm::vec3 inner_first = point(first_angle, inner_radius);
                    const glm::vec3 inner_second = point(second_angle, inner_radius);
                    const glm::vec3 normal = axes[axis];
                    const glm::vec3 color = colors[axis];
                    for (const glm::vec3& vertex : {
                             outer_first, inner_first, outer_second,
                             outer_second, inner_first, inner_second}) {
                        ring.push_back({vertex, normal, color, {}, {}});
                    }
                }
                impl_->gizmo.update(ring);
                set_matrices(from_glm(origin));
                impl_->gizmo.draw();
            }
            glEnable(GL_CULL_FACE);
        } else {
            for (int axis = 0; axis < 3; ++axis) {
                glm::mat4 model = origin;
                if (gizmo_mode == GizmoMode::translate) {
                    model = glm::translate(model, axes[axis] * (shaft_length * 0.5f));
                    model = glm::rotate(model,
                        axis == 0 ? -1.57079633f : axis == 2 ? 1.57079633f : 0.0f,
                        axis == 0 ? glm::vec3{0.0f, 0.0f, 1.0f} :
                            glm::vec3{1.0f, 0.0f, 0.0f});
                    model = glm::scale(model,
                        {shaft_radius, shaft_length, shaft_radius});
                } else {
                    model = glm::translate(model, axes[axis] * (shaft_length * 0.5f));
                    model = glm::rotate(model, rotation_axes[axis].x,
                        {1.0f, 0.0f, 0.0f});
                    model = glm::rotate(model, rotation_axes[axis].z,
                        {0.0f, 0.0f, 1.0f});
                    model = glm::scale(model, {shaft_radius * 2.0f,
                        shaft_length, shaft_radius * 2.0f});
                }
                set_matrices(from_glm(model));
                impl_->program.set_vec3("u_tint", colors[axis]);
                impl_->gizmo.draw();
            }
        }
    }

    const auto character_matrix = [this](const CharacterPlacement& placement) {
        return multiply(
            translation_matrix({placement.x + impl_->character_offset.x,
                placement.y + impl_->character_offset.y,
                placement.z + impl_->character_offset.z}),
            multiply(rotation_y_matrix(placement.yaw),
                scale_matrix({impl_->character_scale, impl_->character_scale,
                    impl_->character_scale})));
    };
    upload_skinning_matrices(impl_->skinning_matrices);
    const Mat4 player_model = character_matrix(player);
    set_matrices(player_model);
    impl_->program.set_int("u_is_skinned",
        impl_->character.is_skinned() && !impl_->skinning_matrices.empty() ? 1 : 0);
    impl_->program.set_vec3("u_tint", impl_->character_material.tint);
    impl_->program.set_int("u_is_ground", 0);
    impl_->character.draw();
}

}