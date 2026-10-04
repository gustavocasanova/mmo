#pragma once

#include "assets/model.hpp"
#include "animation/skeleton.hpp"
#include "renderer/character.hpp"

#include <memory>
#include <span>

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

    void set_character_equipment(const CharacterEquipment& equipment);
    void set_character_model(const assets::Model& model);
    void set_skinning_matrices(std::span<const animation::Matrix4> matrices);

    void render(
        int framebuffer_width,
        int framebuffer_height,
        float player_x,
        float player_z,
        float player_yaw,
        const CameraView& camera) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}