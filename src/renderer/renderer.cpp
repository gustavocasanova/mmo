#include "renderer/renderer.hpp"

#include "platform/window.hpp"

#include <glad/gl.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
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

Mat4 identity_matrix()
{
    Mat4 result;
    result.values[0] = 1.0f;
    result.values[5] = 1.0f;
    result.values[10] = 1.0f;
    result.values[15] = 1.0f;
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

Mat4 rotation_z_matrix(float angle)
{
    Mat4 result = identity_matrix();
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    result.values[0] = cosine;
    result.values[1] = sine;
    result.values[4] = -sine;
    result.values[5] = cosine;
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
    return multiply(value, 1.0f / std::sqrt(dot(value, value)));
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

struct Vertex {
    float position[3];
    float normal[3];
    float color[3];
};

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

std::vector<Vertex> make_cube_vertices()
{
    std::vector<Vertex> vertices;
    vertices.reserve(36);
    constexpr Vec3 color{1.0f, 1.0f, 1.0f};
    append_quad(vertices, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, color);
    append_quad(vertices, {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, color);
    append_quad(vertices, {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f},
        {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, color);
    append_quad(vertices, {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f},
        {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, color);
    append_quad(vertices, {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
        {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, color);
    append_quad(vertices, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, color);
    return vertices;
}

std::vector<Vertex> make_ground_vertices()
{
    std::vector<Vertex> vertices;
    constexpr int kGroundRadius = 48;
    vertices.reserve(kGroundRadius * 2 * kGroundRadius * 2 * 6);
    for (int cell_x = -kGroundRadius; cell_x < kGroundRadius; ++cell_x) {
        for (int cell_z = -kGroundRadius; cell_z < kGroundRadius; ++cell_z) {
            const float shade = (cell_x + cell_z) % 2 == 0 ? 0.92f : 1.0f;
            const Vec3 color{shade, shade, shade};
            append_quad(vertices,
                {static_cast<float>(cell_x), 0.0f, static_cast<float>(cell_z)},
                {static_cast<float>(cell_x), 0.0f, static_cast<float>(cell_z + 1)},
                {static_cast<float>(cell_x + 1), 0.0f, static_cast<float>(cell_z + 1)},
                {static_cast<float>(cell_x + 1), 0.0f, static_cast<float>(cell_z)},
                {0.0f, 1.0f, 0.0f}, color);
        }
    }
    return vertices;
}

constexpr char kVertexShaderSource[] = R"(#version 330 core
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_color;
out vec3 v_color;
out vec3 v_normal;
out vec3 v_world_position;
uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;
uniform vec3 u_tint;
void main() {
    vec4 world_position = u_model * vec4(a_position, 1.0);
    v_world_position = world_position.xyz;
    v_normal = normalize(mat3(transpose(inverse(u_model))) * a_normal);
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
        surface_color = mix(vec3(0.16, 0.24, 0.14), grass, smoothstep(0.0, 0.035, edge)) * v_color;
    }
    float diffuse = max(dot(normalize(v_normal), normalize(vec3(-0.4, 1.0, 0.3))), 0.0);
    float lighting = 0.38 + diffuse * 0.62;
    out_color = vec4(surface_color * lighting, 1.0);
}
)";

std::string shader_info_log(unsigned int shader)
{
    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }

    std::string log(static_cast<std::size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    if (!log.empty() && log.back() == '\0') {
        log.pop_back();
    }
    return log;
}

std::string program_info_log(unsigned int program)
{
    int length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }

    std::string log(static_cast<std::size_t>(length), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    if (!log.empty() && log.back() == '\0') {
        log.pop_back();
    }
    return log;
}

unsigned int compile_shader(unsigned int type, std::string_view source)
{
    const unsigned int shader = glCreateShader(type);
    if (shader == 0) {
        throw std::runtime_error("glCreateShader returned 0");
    }

    const char* source_data = source.data();
    const int source_length = static_cast<int>(source.size());
    glShaderSource(shader, 1, &source_data, &source_length);
    glCompileShader(shader);

    int compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        const std::string log = shader_info_log(shader);
        glDeleteShader(shader);
        throw std::runtime_error("shader compile failed:\n" + log);
    }
    return shader;
}

unsigned int create_program(unsigned int vertex_shader, unsigned int fragment_shader)
{
    const unsigned int program = glCreateProgram();
    if (program == 0) {
        throw std::runtime_error("glCreateProgram returned 0");
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        const std::string log = program_info_log(program);
        glDeleteProgram(program);
        throw std::runtime_error("program link failed:\n" + log);
    }
    return program;
}

class ShaderProgram {
public:
    ShaderProgram()
    {
        const unsigned int vertex_shader = compile_shader(GL_VERTEX_SHADER, kVertexShaderSource);
        unsigned int fragment_shader = 0;
        try {
            fragment_shader = compile_shader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
            id_ = create_program(vertex_shader, fragment_shader);
        } catch (...) {
            glDeleteShader(vertex_shader);
            if (fragment_shader != 0) {
                glDeleteShader(fragment_shader);
            }
            throw;
        }
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        projection_location_ = glGetUniformLocation(id_, "u_projection");
        view_location_ = glGetUniformLocation(id_, "u_view");
        model_location_ = glGetUniformLocation(id_, "u_model");
        tint_location_ = glGetUniformLocation(id_, "u_tint");
        ground_location_ = glGetUniformLocation(id_, "u_is_ground");
    }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ~ShaderProgram()
    {
        if (id_ != 0) {
            glDeleteProgram(id_);
        }
    }

    void bind() const
    {
        glUseProgram(id_);
    }

    void set_matrices(const Mat4& projection, const Mat4& view, const Mat4& model) const
    {
        glUniformMatrix4fv(projection_location_, 1, GL_FALSE, projection.values.data());
        glUniformMatrix4fv(view_location_, 1, GL_FALSE, view.values.data());
        glUniformMatrix4fv(model_location_, 1, GL_FALSE, model.values.data());
    }

    void set_tint(Vec3 tint) const
    {
        glUniform3f(tint_location_, tint.x, tint.y, tint.z);
    }

    void set_ground(bool is_ground) const
    {
        glUniform1i(ground_location_, is_ground ? 1 : 0);
    }

private:
    unsigned int id_ = 0;
    int projection_location_ = -1;
    int view_location_ = -1;
    int model_location_ = -1;
    int tint_location_ = -1;
    int ground_location_ = -1;
};

class GpuMesh {
public:
    explicit GpuMesh(const std::vector<Vertex>& vertices)
    {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        if (vao_ == 0 || vbo_ == 0) {
            if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
            if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
            throw std::runtime_error("failed to allocate VAO/VBO");
        }

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
            vertices.data(), GL_STATIC_DRAW);

        constexpr GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(offsetof(Vertex, color)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        vertex_count_ = static_cast<GLsizei>(vertices.size());
    }

    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    ~GpuMesh()
    {
        if (vbo_ != 0) {
            glDeleteBuffers(1, &vbo_);
        }
        if (vao_ != 0) {
            glDeleteVertexArrays(1, &vao_);
        }
    }

    void draw() const
    {
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
        glBindVertexArray(0);
    }

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    GLsizei vertex_count_ = 0;
};

Mat4 object_matrix(const Mat4& actor, Vec3 position, Vec3 size)
{
    return multiply(actor, multiply(translation_matrix(position), scale_matrix(size)));
}

Mat4 limb_matrix(const Mat4& actor, float pivot_x, float swing)
{
    const Mat4 pivot = translation_matrix({pivot_x, 1.63f, 0.0f});
    const Mat4 rotation = multiply(rotation_z_matrix(pivot_x * 0.12f), rotation_x_matrix(swing));
    const Mat4 center = translation_matrix({0.0f, -0.34f, 0.0f});
    const Mat4 size = scale_matrix({0.22f, 0.70f, 0.25f});
    return multiply(actor, multiply(pivot, multiply(rotation, multiply(center, size))));
}

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
    ShaderProgram program;
    GpuMesh ground{make_ground_vertices()};
    GpuMesh cube{make_cube_vertices()};
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

void Renderer::render(
    int framebuffer_width,
    int framebuffer_height,
    float player_x,
    float player_z,
    float player_yaw,
    float walk_phase,
    const CameraView& camera) const
{
    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.48f, 0.66f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    impl_->program.bind();

    const float aspect_ratio = static_cast<float>(framebuffer_width) /
        static_cast<float>(framebuffer_height);
    constexpr float kFieldOfView = 1.04719755f;
    const Mat4 projection = perspective_matrix(kFieldOfView, aspect_ratio, 0.1f, 160.0f);
    const Vec3 target{camera.focus_x, camera.focus_y, camera.focus_z};
    const Vec3 eye{camera.eye_x, camera.eye_y, camera.eye_z};
    const Mat4 view = look_at_matrix(eye, target, {0.0f, 1.0f, 0.0f});
    const Mat4 ground_model = identity_matrix();

    impl_->program.set_matrices(projection, view, ground_model);
    impl_->program.set_tint({1.0f, 1.0f, 1.0f});
    impl_->program.set_ground(true);
    impl_->ground.draw();

    const Mat4 actor = multiply(
        translation_matrix({player_x, 0.0f, player_z}), rotation_y_matrix(player_yaw));
    const auto draw_cube = [this, &projection, &view](const Mat4& model, Vec3 color) {
        impl_->program.set_matrices(projection, view, model);
        impl_->program.set_tint(color);
        impl_->program.set_ground(false);
        impl_->cube.draw();
    };

    draw_cube(object_matrix(actor, {0.0f, 1.31f, 0.0f}, {0.56f, 0.86f, 0.38f}),
        {0.27f, 0.43f, 0.42f});
    draw_cube(object_matrix(actor, {0.0f, 0.96f, 0.0f}, {0.58f, 0.13f, 0.40f}),
        {0.36f, 0.25f, 0.16f});
    draw_cube(object_matrix(actor, {0.0f, 2.03f, 0.0f}, {0.46f, 0.48f, 0.43f}),
        {0.77f, 0.57f, 0.39f});
    draw_cube(object_matrix(actor, {0.0f, 2.23f, -0.015f}, {0.49f, 0.17f, 0.46f}),
        {0.24f, 0.19f, 0.14f});
    draw_cube(object_matrix(actor, {-0.105f, 2.08f, 0.218f}, {0.045f, 0.055f, 0.025f}),
        {0.12f, 0.10f, 0.08f});
    draw_cube(object_matrix(actor, {0.105f, 2.08f, 0.218f}, {0.045f, 0.055f, 0.025f}),
        {0.12f, 0.10f, 0.08f});

    const float leg_swing = std::sin(walk_phase) * 0.48f;
    const Mat4 left_leg = multiply(actor, multiply(
        translation_matrix({-0.16f, 0.89f, 0.0f}), multiply(rotation_x_matrix(leg_swing),
            multiply(translation_matrix({0.0f, -0.42f, 0.0f}),
                scale_matrix({0.26f, 0.84f, 0.31f})))));
    const Mat4 right_leg = multiply(actor, multiply(
        translation_matrix({0.16f, 0.89f, 0.0f}), multiply(rotation_x_matrix(-leg_swing),
            multiply(translation_matrix({0.0f, -0.42f, 0.0f}),
                scale_matrix({0.26f, 0.84f, 0.31f})))));
    draw_cube(left_leg, {0.37f, 0.27f, 0.19f});
    draw_cube(right_leg, {0.37f, 0.27f, 0.19f});
    draw_cube(limb_matrix(actor, -0.39f, -leg_swing), {0.77f, 0.57f, 0.39f});
    draw_cube(limb_matrix(actor, 0.39f, leg_swing), {0.77f, 0.57f, 0.39f});
}

}