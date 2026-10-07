#include "animation/animation_controller.hpp"

#include <utility>

namespace mmo::animation {

AnimationController::AnimationController(const Skeleton& skeleton, std::vector<AnimationClip> clips)
    : animator_(skeleton, std::move(clips))
{
}

bool AnimationController::bind_state(AnimationState state, std::size_t clip_index, bool loop)
{
    const std::size_t index = static_cast<std::size_t>(state);
    if (index >= bindings_.size() || clip_index >= animator_.clip_count()) {
        return false;
    }
    bindings_[index] = {clip_index, loop};
    return true;
}

bool AnimationController::play_animation(std::string_view name, bool loop)
{
    const std::optional<std::size_t> clip = animator_.find_clip(name);
    if (!clip) {
        return false;
    }
    animator_.play(*clip, loop);
    has_state_ = false;
    return true;
}

bool AnimationController::cross_fade(std::string_view name, float duration_seconds, bool loop)
{
    const std::optional<std::size_t> clip = animator_.find_clip(name);
    if (!clip || !animator_.cross_fade(*clip, duration_seconds, loop)) {
        return false;
    }
    has_state_ = false;
    return true;
}

bool AnimationController::set_state(AnimationState state, float fade_seconds)
{
    const std::size_t index = static_cast<std::size_t>(state);
    if (index >= bindings_.size() || !bindings_[index].clip) {
        return false;
    }
    if (has_state_ && state_ == state) {
        return true;
    }

    const StateBinding& binding = bindings_[index];
    if (has_state_ && fade_seconds > 0.0f) {
        if (!animator_.cross_fade(*binding.clip, fade_seconds, binding.loop)) {
            return false;
        }
    } else {
        animator_.play(*binding.clip, binding.loop);
    }
    state_ = state;
    has_state_ = true;
    return true;
}

void AnimationController::update(float delta_seconds)
{
    animator_.update(delta_seconds);
    mixer_.update(delta_seconds, events_);
    if (mixer_.active()) {
        mixer_.mix(animator_, animator_.pose(), mixed_pose_);
    }
}

void AnimationController::set_upper_body_mask(BoneMask mask)
{
    mixer_ = AnimationMixer(std::move(mask));
}

bool AnimationController::play_action(std::string_view name, const ActionSettings& settings)
{
    const std::optional<std::size_t> clip = animator_.find_clip(name);
    if (!clip || mixer_.mask().size() == 0) {
        return false;
    }
    return mixer_.start_action(*clip, animator_.clip(*clip).duration, settings);
}

void AnimationController::cancel_action()
{
    mixer_.cancel_action();
}

std::vector<AnimationEvent> AnimationController::take_events()
{
    return std::exchange(events_, {});
}

const AnimationMixer& AnimationController::mixer() const
{
    return mixer_;
}

std::string_view AnimationController::lower_clip_name() const
{
    return animator_.playing() && animator_.active_clip() < animator_.clip_count()
        ? std::string_view(animator_.clip(animator_.active_clip()).name)
        : std::string_view("<none>");
}

std::string_view AnimationController::upper_clip_name() const
{
    return mixer_.active() && mixer_.action_clip() < animator_.clip_count()
        ? std::string_view(animator_.clip(mixer_.action_clip()).name)
        : std::string_view("<none>");
}

void AnimationController::stop()
{
    animator_.stop();
    has_state_ = false;
}

AnimationState AnimationController::state() const
{
    return state_;
}

const Pose& AnimationController::pose() const
{
    return mixer_.active() && !mixed_pose_.skin_matrices.empty()
        ? mixed_pose_ : animator_.pose();
}

const Animator& AnimationController::animator() const
{
    return animator_;
}

}