#pragma once

#include <string>

namespace mmo::ui {

struct TargetFrameData {
    std::string name;
    int level = 1;
    float hp = 0.0f;
    float max_hp = 1.0f;
    std::string status;
    bool dead = false;
};

// Draws the target frame with ImGui; must be called between begin_frame() and render().
void draw_target_frame(const TargetFrameData& data);

}
