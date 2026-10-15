#pragma once

#include "animation/layered_animation.hpp"
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
    [[nodiscard]] bool cross_fade(std::string_view name, float duration_seconds, bool loop = true,
        bool keep_phase = false);
    [[nodiscard]] bool set_state(AnimationState state, float fade_seconds = 0.15f);
    void update(float delta_seconds);
    void stop();

    // Upper-body layer: configure the mask once, then play one-shot actions over any locomotion.
    void set_upper_body_mask(BoneMask mask);
    [[nodiscard]] bool play_action(std::string_view clip_name, const ActionSettings& settings);
    void cancel_action();
    // Events produced since the last call (ActionStart/ActionHit/ActionEnd).
    [[nodiscard]] std::vector<AnimationEvent> take_events();
    [[nodiscard]] const AnimationMixer& mixer() const;
    [[nodiscard]] std::string_view lower_clip_name() const;
    [[nodiscard]] std::string_view upper_clip_name() const;

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
    AnimationMixer mixer_;
    Pose mixed_pose_;
    std::vector<AnimationEvent> events_;
    std::array<StateBinding, kStateCount> bindings_{};
    AnimationState state_ = AnimationState::Idle;
    bool has_state_ = false;
};

}