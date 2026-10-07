#include "apps/client/target_frame.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace mmo::ui {

void draw_target_frame(const TargetFrameData& data)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, 16.0f}, ImGuiCond_Always, {0.5f, 0.0f});
    ImGui::SetNextWindowSize({260.0f, 0.0f});
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##target_frame", nullptr, flags)) {
        ImGui::Text("%s%s", data.name.c_str(), data.dead ? " (dead)" : "");
        ImGui::Text("Level %d", data.level);
        const float fraction = std::clamp(data.hp / std::max(data.max_hp, 1.0f), 0.0f, 1.0f);
        char label[48];
        std::snprintf(label, sizeof(label), "%.0f / %.0f", data.hp, data.max_hp);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, {0.75f, 0.12f, 0.1f, 1.0f});
        ImGui::ProgressBar(fraction, {-1.0f, 18.0f}, label);
        ImGui::PopStyleColor();
        if (!data.status.empty()) {
            ImGui::TextDisabled("%s", data.status.c_str());
        }
    }
    ImGui::End();
}

}
