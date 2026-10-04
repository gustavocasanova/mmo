#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <span>
#include <string_view>

namespace mmo::renderer {

class Shader {
public:
    Shader(std::string_view vertex_source, std::string_view fragment_source);
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    ~Shader();

    void bind() const;
    void bind_uniform_block(const char* name, unsigned int binding) const;
    void set_mat4(const char* name, const glm::mat4& value) const;
    void set_mat4_array(const char* name, std::span<const glm::mat4> values) const;
    void set_vec3(const char* name, const glm::vec3& value) const;
    void set_int(const char* name, int value) const;

private:
    int uniform_location(const char* name) const;
    unsigned int id_ = 0;
};

}
