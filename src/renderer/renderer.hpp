#pragma once
#include "assets/mesh_catalog.hpp"
#include "scene/camera.hpp"
#include "scene/draw_item.hpp"
#include <memory>
#include <span>

namespace mmo::renderer {
using GraphicsProc = void (*)();
using GraphicsLoader = GraphicsProc (*)(const char*);
class Renderer {
public:
    // Uploads a snapshot of this catalog. Draw IDs must come from that catalog.
    Renderer(GraphicsLoader load, const assets::MeshCatalog& assets);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void update_mesh(assets::MeshId id, const assets::MeshData& data);
    void render(int width, int height, const scene::Camera& camera,
        std::span<const scene::DrawItem> items) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
