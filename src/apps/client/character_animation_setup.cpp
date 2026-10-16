#include "apps/client/character_animation_setup.hpp"

#include "animation/layered_animation.hpp"
#include "assets/model.hpp"
#include "character/character.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace mmo::game::client {
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

animation::BoneMask make_lower_body_mask(const animation::Skeleton& skeleton)
{
    const std::optional<std::size_t> pelvis = skeleton.find_bone("pelvis");
    const std::optional<std::size_t> spine = skeleton.find_bone("spine_01");
    if (!pelvis || !spine) {
        throw std::runtime_error(
            "running animation composition requires pelvis and spine_01 bones");
    }

    animation::BoneMask mask(skeleton.bones.size());
    for (const animation::Bone& bone : skeleton.bones) {
        bool below_pelvis = false;
        bool below_spine = false;
        for (std::size_t current = bone.index; current != animation::kNoBone;
             current = skeleton.bones[current].parent_index) {
            below_pelvis = below_pelvis || current == *pelvis;
            below_spine = below_spine || current == *spine;
        }
        if (below_pelvis && !below_spine) {
            mask.set_weight(bone.index, 1.0f);
        }
    }
    if (mask.active_bone_count() <= 1) {
        throw std::runtime_error("running animation composition found no leg bones");
    }
    return mask;
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

void compose_running_animations(assets::Model& model)
{
    const std::vector<animation::AnimationClip> source_clips = model.animations;
    const animation::BoneMask lower_body_mask = make_lower_body_mask(model.skeleton);
    const std::size_t jog_index = find_clip_index(source_clips, "Jog_Fwd_Loop");
    const std::size_t sprint_index = find_clip_index(source_clips, "Sprint_Loop");
    struct RunningClip {
        std::string_view target;
        std::string_view source;
        std::size_t base_index;
    };
    const RunningClip running_clips[] = {
        {"Jog_Fwd_Loop", "Melee_Run_Forward", jog_index},
        {"Sprint_Loop", "Melee_Run_Forward", sprint_index},
        {"Melee_Run_Forward", "Melee_Run_Forward", jog_index},
        {"Melee_Run_Backward", "Melee_Run_Backward", jog_index},
        {"Melee_StrafeRun_ForwardLeft", "Melee_StrafeRun_ForwardLeft", jog_index},
        {"Melee_StrafeRun_ForwardRight", "Melee_StrafeRun_ForwardRight", jog_index},
        {"Melee_StrafeRun_BackwardLeft", "Melee_StrafeRun_BackwardLeft", jog_index},
        {"Melee_StrafeRun_BackwardRight", "Melee_StrafeRun_BackwardRight", jog_index},
    };

    for (const RunningClip& binding : running_clips) {
        const auto source = std::find_if(source_clips.begin(), source_clips.end(),
            [&binding](const animation::AnimationClip& clip) {
                return clip.name == binding.source;
            });
        const auto target = std::find_if(model.animations.begin(), model.animations.end(),
            [&binding](const animation::AnimationClip& clip) {
                return clip.name == binding.target;
            });
        if (source == source_clips.end() || target == model.animations.end()) {
            continue;
        }

        animation::AnimationClip composed = source_clips[binding.base_index];
        composed.name = binding.target;
        animation::replace_masked_channels(composed, *source, lower_body_mask);
        *target = std::move(composed);
    }
}

std::optional<std::string_view> find_weapon_animation(
    const items::WeaponDefinition& weapon,
    const std::vector<animation::AnimationClip>& clips)
{
    for (const std::string_view name : weapon.attack_animations) {
        if (name.empty()) {
            continue;
        }
        const auto clip = std::find_if(clips.begin(), clips.end(),
            [name](const animation::AnimationClip& candidate) {
                return candidate.name == name;
            });
        if (clip != clips.end()) {
            return clip->name;
        }
    }
    return std::nullopt;
}

bool has_animation(
    const std::vector<animation::AnimationClip>& clips,
    std::string_view name)
{
    return std::any_of(clips.begin(), clips.end(),
        [name](const animation::AnimationClip& clip) {
            return clip.name == name;
        });
}

void bind_locomotion_animations(character::Character& character)
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