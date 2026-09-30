#pragma once

#include <memory>

namespace mmo::renderer {

struct CameraView {
    float eye_x;
    float eye_y;
    float eye_z;
    float focus_x;
    float focus_y;
    float focus_z;
};

class Renderer {
public:
    Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void render(
        int framebuffer_width,
        int framebuffer_height,
        float player_x,
        float player_z,
        float player_yaw,
        float walk_phase,
        const CameraView& camera) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}