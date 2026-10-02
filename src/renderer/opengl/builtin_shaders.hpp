#pragma once
namespace mmo::renderer::opengl {
inline constexpr char kVertexShaderSource[] = R"(#version 330 core
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
    if (u_is_skinned == 1 && dot(a_bone_weights, vec4(1.0)) == 0.0) skin = mat4(1.0);
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
        float cell_shade = mix(0.92, 1.0, checker);
        surface_color = mix(vec3(0.16, 0.24, 0.14), grass, smoothstep(0.0, 0.035, edge))
            * v_color * cell_shade;
    }
    float diffuse = max(dot(normalize(v_normal), normalize(vec3(-0.4, 1.0, 0.3))), 0.0);
    float lighting = 0.38 + diffuse * 0.62;
    out_color = vec4(surface_color * lighting, 1.0);
}
)";

}
