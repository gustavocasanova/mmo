#include "renderer/shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>

namespace mmo::renderer {
namespace {

std::string shader_log(unsigned int shader)
{
    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, result.data());
    return result;
}

std::string program_log(unsigned int program)
{
    int length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    glGetProgramInfoLog(program, length, nullptr, result.data());
    return result;
}

unsigned int compile(unsigned int type, std::string_view source)
{
    const unsigned int shader = glCreateShader(type);
    const char* source_data = source.data();
    const int source_length = static_cast<int>(source.size());
    glShaderSource(shader, 1, &source_data, &source_length);
    glCompileShader(shader);

    int compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        const std::string log = shader_log(shader);
        glDeleteShader(shader);
        throw std::runtime_error("shader compile failed:\n" + log);
    }
    return shader;
}

}

Shader::Shader(std::string_view vertex_source, std::string_view fragment_source)
{
    const unsigned int vertex = compile(GL_VERTEX_SHADER, vertex_source);
    const unsigned int fragment = compile(GL_FRAGMENT_SHADER, fragment_source);
    id_ = glCreateProgram();
    glAttachShader(id_, vertex);
    glAttachShader(id_, fragment);
    glLinkProgram(id_);

    int linked = GL_FALSE;
    glGetProgramiv(id_, GL_LINK_STATUS, &linked);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (linked != GL_TRUE) {
        const std::string log = program_log(id_);
        glDeleteProgram(id_);
        id_ = 0;
        throw std::runtime_error("program link failed:\n" + log);
    }
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
    if (values.empty()) {
        return;
    }
    glUniformMatrix4fv(uniform_location(name), static_cast<GLsizei>(values.size()),
        GL_FALSE, glm::value_ptr(values.front()));
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
