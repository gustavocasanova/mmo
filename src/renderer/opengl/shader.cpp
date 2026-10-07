#include "renderer/shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace mmo::renderer {
namespace {

std::string shader_info_log(GLuint shader)
{
    GLint length = 0;
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

std::string program_info_log(GLuint program)
{
    GLint length = 0;
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

GLuint compile_shader(GLenum type, std::string_view source)
{
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        throw std::runtime_error("glCreateShader returned 0");
    }
    const char* source_data = source.data();
    const GLint source_length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &source_data, &source_length);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        const std::string log = shader_info_log(shader);
        glDeleteShader(shader);
        throw std::runtime_error("shader compile failed:\n" + log);
    }
    return shader;
}

}

Shader::Shader(std::string_view vertex_source, std::string_view fragment_source)
{
    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = 0;
    try {
        fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
        id_ = glCreateProgram();
        if (id_ == 0) {
            throw std::runtime_error("glCreateProgram returned 0");
        }
        glAttachShader(id_, vertex_shader);
        glAttachShader(id_, fragment_shader);
        glLinkProgram(id_);

        GLint linked = GL_FALSE;
        glGetProgramiv(id_, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            throw std::runtime_error("shader program link failed:\n" + program_info_log(id_));
        }
    } catch (...) {
        if (id_ != 0) {
            glDeleteProgram(id_);
            id_ = 0;
        }
        glDeleteShader(vertex_shader);
        if (fragment_shader != 0) {
            glDeleteShader(fragment_shader);
        }
        throw;
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
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

void Shader::bind_uniform_block(const char* name, unsigned int binding) const
{
    const GLuint block = glGetUniformBlockIndex(id_, name);
    if (block == GL_INVALID_INDEX) {
        throw std::runtime_error(std::string("shader uniform block not found: ") + name);
    }
    glUniformBlockBinding(id_, block, binding);
}

int Shader::uniform_location(const char* name) const
{
    return glGetUniformLocation(id_, name);
}

void Shader::set_mat4(const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(uniform_location(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::set_mat4_array(const char* name, std::span<const glm::mat4> values) const
{
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::length_error("shader matrix array is too large");
    }
    if (!values.empty()) {
        glUniformMatrix4fv(uniform_location(name), static_cast<GLsizei>(values.size()),
            GL_FALSE, glm::value_ptr(values.front()));
    }
}

void Shader::set_vec3(const char* name, const glm::vec3& value) const
{
    glUniform3f(uniform_location(name), value.x, value.y, value.z);
}

void Shader::set_int(const char* name, int value) const
{
    glUniform1i(uniform_location(name), value);
}

}
