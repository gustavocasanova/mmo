#include "apps/client/character_setup.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mmo::app {
namespace {

void reverse_keyframes(animation::Vector3Keyframes& keyframes)
{
    const float last_time = keyframes.times.empty() ? 0.0f : keyframes.times.back();
    std::reverse(keyframes.times.begin(), keyframes.times.end());
    std::reverse(keyframes.values.begin(), keyframes.values.end());
    for (float& time : keyframes.times) {
        time = last_time - time;
    }
}

void reverse_keyframes(animation::QuaternionKeyframes& keyframes)
{
    const float last_time = keyframes.times.empty() ? 0.0f : keyframes.times.back();
    std::reverse(keyframes.times.begin(), keyframes.times.end());
    std::reverse(keyframes.values.begin(), keyframes.values.end());
    for (float& time : keyframes.times) {
        time = last_time - time;
    }
}

std::size_t find_clip_index(
    const std::vector<animation::AnimationClip>& clips,
    std::string_view name)
{
    const auto clip = std::find_if(clips.begin(), clips.end(),
        [name](const animation::AnimationClip& candidate) {
            return candidate.name == name;
        });
    if (clip == clips.end()) {
        throw std::runtime_error(
            "Universal Animation Library is missing required clip: " + std::string(name));
    }
    return static_cast<std::size_t>(std::distance(clips.begin(), clip));
}

}

std::size_t add_backward_walk_clip(assets::Model& model)
{
    const auto existing = std::find_if(model.animations.begin(), model.animations.end(),
        [](const animation::AnimationClip& clip) {
            return clip.name == "Walk_Backward_Loop";
        });
    if (existing != model.animations.end()) {
        return static_cast<std::size_t>(std::distance(model.animations.begin(), existing));
    }
    const auto forward_walk = std::find_if(model.animations.begin(), model.animations.end(),
        [](const animation::AnimationClip& clip) { return clip.name == "Walk_Loop"; });
    if (forward_walk == model.animations.end()) {
        throw std::runtime_error("Universal Animation Library is missing Walk_Loop");
    }
    animation::AnimationClip backward_walk = *forward_walk;
    backward_walk.name = "Walk_Backward_Loop";
    for (animation::AnimationChannel& channel : backward_walk.channels) {
        reverse_keyframes(channel.translations);
        reverse_keyframes(channel.rotations);
        reverse_keyframes(channel.scales);
    }
    if (!backward_walk.is_valid(model.skeleton.bones.size())) {
        throw std::runtime_error("Could not construct a valid backward-walking clip");
    }
    model.animations.push_back(std::move(backward_walk));
    return model.animations.size() - 1;
}

void bind_animations(character::Character& character)
{
    animation::AnimationController& controller = character.model().animation();
    const auto& clips = character.model().body().model().animations;
    const bool bound =
        controller.bind_state(animation::AnimationState::Idle,
            find_clip_index(clips, "Idle_Loop")) &&
        controller.bind_state(animation::AnimationState::Walk,
            find_clip_index(clips, "Walk_Loop")) &&
        controller.bind_state(animation::AnimationState::Run,
            find_clip_index(clips, "Jog_Fwd_Loop")) &&
        controller.bind_state(animation::AnimationState::Jump,
            find_clip_index(clips, "Jump_Loop"));
    (void)find_clip_index(clips, "Sprint_Loop");
    (void)find_clip_index(clips, "Jump_Start");
    (void)find_clip_index(clips, "Jump_Land");
    if (!bound || !controller.set_state(animation::AnimationState::Idle, 0.0f)) {
        throw std::runtime_error("Could not bind required locomotion animations");
    }
}

}
