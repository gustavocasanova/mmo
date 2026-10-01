#pragma once
namespace mmo::renderer::opengl {
inline constexpr char kVertexShaderSource[] = R"(#version 330 core
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

inline constexpr char kFragmentShaderSource[] = R"(#version 330 core
in vec3 v_color;
in vec3 v_normal;
in vec3 v_world_position;
out vec4 out_color;
uniform int u_surface_pattern;
void main() {
    vec3 surface_color = v_color;
    if (u_surface_pattern == 1) {
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

}
