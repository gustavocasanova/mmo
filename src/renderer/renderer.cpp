#include "renderer/renderer.hpp"
#include "renderer/mesh.hpp"
#include "renderer/shader.hpp"
#include <glad/gl.h>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace mmo::renderer {
namespace {
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
    Shader shader;
    std::vector<std::unique_ptr<Mesh>> meshes;
};
Renderer::Renderer(GraphicsLoader load, const assets::MeshCatalog& assets)
{
    if (gladLoadGL(load) == 0) {
        throw std::runtime_error("could not load OpenGL 3.3 entry points");
    }
    log_gl_info();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    impl_ = std::make_unique<Impl>();
    for (const auto& data : assets.meshes()) {
        impl_->meshes.push_back(std::make_unique<Mesh>(data));
    }
}
Renderer::~Renderer() = default;
void Renderer::update_mesh(assets::MeshId id, const assets::MeshData& data)
{
    impl_->meshes.at(id.value)->update(data);
}
void Renderer::render(int width, int height, const scene::Camera& camera,
    std::span<const scene::DrawItem> items) const
{
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.48f, 0.66f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto projection = camera.projection(static_cast<float>(width) / static_cast<float>(height));
    const auto view = camera.view();
    impl_->shader.bind();
    for (const auto& item : items) {
        const auto& mesh = impl_->meshes.at(item.mesh.value);
        impl_->shader.set_matrices(projection, view, item.transform);
        impl_->shader.set_tint(item.material.tint);
        impl_->shader.set_pattern(item.material.pattern);
        if (!item.skinning_matrices.empty() && mesh->required_bones() > item.skinning_matrices.size()) {
            throw std::invalid_argument("skinning palette does not cover the mesh bones");
        }
        impl_->shader.set_skinning(mesh->required_bones() > 0
            ? item.skinning_matrices : std::span<const math::Mat4>{});
        mesh->draw();
    }
}
}
