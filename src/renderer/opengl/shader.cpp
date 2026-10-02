#include "renderer/shader.hpp"
#include "assets/mesh_data.hpp"
#include <cmath>
#include "renderer/opengl/builtin_shaders.hpp"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mmo::renderer {
using math::Mat4;
using math::Vec3;
namespace {
using namespace opengl;
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

}
Shader::Shader()
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
    pattern_location_ = glGetUniformLocation(id_, "u_surface_pattern");
    normal_locations_ = {glGetUniformLocation(id_, "u_normal_column0"),
        glGetUniformLocation(id_, "u_normal_column1"), glGetUniformLocation(id_, "u_normal_column2")};
    skinning_location_ = glGetUniformLocation(id_, "u_bone_matrices[0]");
    skinned_location_ = glGetUniformLocation(id_, "u_is_skinned");
}

Shader::~Shader()
{
    if (id_ != 0) {
        glDeleteProgram(id_);
    }
}

void Shader::bind() const
{
    glUseProgram(id_);
}

void Shader::set_matrices(const Mat4& projection, const Mat4& view, const Mat4& model) const
{
    glUniformMatrix4fv(projection_location_, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(view_location_, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(model_location_, 1, GL_FALSE, glm::value_ptr(model));
    const glm::mat3 linear(model);
    const glm::mat3 normal = std::abs(glm::determinant(linear)) <= 0.000001f
        ? glm::mat3(1.0f) : glm::transpose(glm::inverse(linear));
    for (std::size_t i = 0; i < normal_locations_.size(); ++i) {
        const auto column = normal[static_cast<int>(i)];
        glUniform3f(normal_locations_[i], column.x, column.y, column.z);
    }
}

void Shader::set_tint(Vec3 tint) const
{
    glUniform3f(tint_location_, tint.x, tint.y, tint.z);
}

void Shader::set_pattern(scene::SurfacePattern pattern) const
{
    glUniform1i(pattern_location_, pattern == scene::SurfacePattern::checker_grid ? 1 : 0);
}

void Shader::set_skinning(std::span<const math::Mat4> matrices) const
{
    if (matrices.size() > assets::kMaxSkinningBones) {
        throw std::length_error("OpenGL skinning palette exceeds the 48-bone limit");
    }
    glUniform1i(skinned_location_, matrices.empty() ? 0 : 1);
    if (!matrices.empty()) {
        glUniformMatrix4fv(skinning_location_, static_cast<GLsizei>(matrices.size()),
            GL_FALSE, glm::value_ptr(matrices.front()));
    }
}
}
