#pragma once

#include "animation/skeleton.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace mmo::animation {

enum class AnimationState : std::size_t {
    Idle,
    Walk,
    Run,
    Jump,
    Attack,
    Hit,
    Death,
    Count,
};

class AnimationController {
public:
    AnimationController(const Skeleton& skeleton, std::vector<AnimationClip> clips = {});

    [[nodiscard]] bool bind_state(AnimationState state, std::size_t clip_index, bool loop = true);
    [[nodiscard]] bool play_animation(std::string_view name, bool loop = true);
    [[nodiscard]] bool cross_fade(std::string_view name, float duration_seconds, bool loop = true);
    [[nodiscard]] bool set_state(AnimationState state, float fade_seconds = 0.15f);
    void update(float delta_seconds);
    void stop();

    [[nodiscard]] AnimationState state() const;
    [[nodiscard]] const Pose& pose() const;
    [[nodiscard]] const Animator& animator() const;

private:
    struct StateBinding {
        std::optional<std::size_t> clip;
        bool loop = true;
    };

    static constexpr std::size_t kStateCount = static_cast<std::size_t>(AnimationState::Count);

    Animator animator_;
    std::array<StateBinding, kStateCount> bindings_{};
    AnimationState state_ = AnimationState::Idle;
    bool has_state_ = false;
};

}