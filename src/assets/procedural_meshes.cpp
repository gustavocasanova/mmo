#include "assets/procedural_meshes.hpp"
#include "math/transform.hpp"
#include <utility>

namespace mmo::assets {
namespace {
using math::Vec3;
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

}

MeshData make_cube()
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
    return {std::move(vertices)};
}

MeshData make_ground()
{
    std::vector<Vertex> vertices;
    vertices.reserve(6);
    append_quad(vertices, {-48, 0, -48}, {-48, 0, 48}, {48, 0, 48}, {48, 0, -48},
        {0, 1, 0}, {1, 1, 1});
    return {std::move(vertices)};
}
}
