#include "animation/animation_controller.hpp"
#include "animation/layered_animation.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace mmo::animation;

void check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// root -> pelvis -> {thigh, spine -> arm}
Skeleton make_skeleton()
{
    Skeleton skeleton;
    const auto add = [&](const char* name, std::size_t parent) {
        Bone bone;
        bone.name = name;
        bone.parent_index = parent;
        return skeleton.add_bone(std::move(bone));
    };
    const std::size_t root = add("root", kNoBone);
    const std::size_t pelvis = add("pelvis", root);
    add("thigh", pelvis);
    const std::size_t spine = add("spine", pelvis);
    add("arm", spine);
    return skeleton;
}

AnimationClip make_clip(const char* name, float duration, std::size_t bone, float angle)
{
    AnimationClip clip;
    clip.name = name;
    clip.duration = duration;
    AnimationChannel channel;
    channel.bone_index = bone;
    const float half = angle * 0.5f;
    channel.rotations.times = {0.0f, duration};
    channel.rotations.values = {{0.0f, 0.0f, std::sin(half), std::cos(half)},
        {0.0f, 0.0f, std::sin(half), std::cos(half)}};
    clip.channels.push_back(std::move(channel));
    return clip;
}

float angle_of(const Transform& transform)
{
    return 2.0f * std::atan2(transform.rotation.z, transform.rotation.w);
}

void test_bone_mask_branch()
{
    const Skeleton skeleton = make_skeleton();
    const BoneMask mask = BoneMask::branch(skeleton, "spine");
    check(mask.active_bone_count() == 2, "mask must cover the spine and its descendants");
    check(mask.weight(*skeleton.find_bone("arm")) == 1.0f, "arm must be masked in");
    check(mask.weight(*skeleton.find_bone("pelvis")) == 0.0f, "pelvis must stay lower body");
    check(mask.weight(*skeleton.find_bone("thigh")) == 0.0f, "legs must stay lower body");
    bool threw = false;
    try {
        (void)BoneMask::branch(skeleton, "missing");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "unknown mask root must be rejected");
}

void test_root_translation_axis_is_held()
{
    const Skeleton skeleton = make_skeleton();
    const std::size_t pelvis = *skeleton.find_bone("pelvis");
    AnimationClip clip;
    clip.name = "left_strafe";
    clip.duration = 1.0f;
    AnimationChannel rotation_channel;
    rotation_channel.bone_index = pelvis;
    rotation_channel.rotations.times = {0.0f, 1.0f};
    rotation_channel.rotations.values = {{0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}};
    clip.channels.push_back(std::move(rotation_channel));

    AnimationChannel channel;
    channel.bone_index = pelvis;
    channel.translations.times = {0.0f, 0.5f, 1.0f};
    channel.translations.values = {{0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.5f},
        {3.0f, 0.0f, 1.0f}};
    clip.channels.push_back(std::move(channel));

    hold_root_translation_axis(clip, pelvis, 0);

    const auto& translations = clip.channels.back().translations.values;
    check(translations[0][0] == 0.0f && translations[1][0] == 0.0f &&
            translations[2][0] == 0.0f,
        "root lateral motion must not move the character model");
    check(translations[1][1] == 1.0f && translations[1][2] == 0.5f &&
            translations[2][2] == 1.0f,
        "holding lateral root motion must preserve vertical and forward animation");
}

void test_blend_is_shortest_path_and_weighted()
{
    Transform a;
    Transform b;
    b.translation = {2.0f, 0.0f, 0.0f};
    check(std::abs(AnimationMixer::blend(a, b, 0.5f).translation[0] - 1.0f) < 1e-5f,
        "blend must interpolate translation");
    check(AnimationMixer::blend(a, b, 0.0f).translation[0] == 0.0f, "weight 0 keeps lower");
    check(AnimationMixer::blend(a, b, 1.0f).translation[0] == 2.0f, "weight 1 uses upper");
}

void test_masked_animation_channels_replace_lower_body_only()
{
    const Skeleton skeleton = make_skeleton();
    const std::size_t pelvis = *skeleton.find_bone("pelvis");
    const std::size_t thigh = *skeleton.find_bone("thigh");
    const std::size_t arm = *skeleton.find_bone("arm");
    BoneMask lower_body(skeleton.bones.size());
    lower_body.set_weight(pelvis, 1.0f);
    lower_body.set_weight(thigh, 1.0f);

    AnimationClip base = make_clip("run", 1.0f, pelvis, 0.2f);
    base.channels.push_back(make_clip("base_arm", 1.0f, arm, 0.4f).channels.front());
    AnimationClip alternate = make_clip("alternate", 2.0f, pelvis, 1.2f);
    alternate.channels.push_back(make_clip("alternate_thigh", 2.0f, thigh, 0.8f)
            .channels.front());
    alternate.channels.push_back(make_clip("alternate_arm", 2.0f, arm, 2.0f)
            .channels.front());

    replace_masked_channels(base, alternate, lower_body);

    check(base.name == "run" && base.duration == 1.0f,
        "channel replacement must preserve the base clip identity and duration");
    check(base.channels.size() == 3, "lower-body channel must be added without extra upper channels");
    const auto channel_for = [&base](std::size_t bone) -> const AnimationChannel& {
        for (const AnimationChannel& channel : base.channels) {
            if (channel.bone_index == bone) {
                return channel;
            }
        }
        throw std::runtime_error("expected animation channel is missing");
    };
    check(std::abs(base.channels.front().rotations.values.front().z -
            alternate.channels.front().rotations.values.front().z) < 1e-5f,
        "pelvis must use the alternate pack's lower-body track");
    check(std::abs(channel_for(thigh).rotations.values.front().z -
            alternate.channels[1].rotations.values.front().z) < 1e-5f,
        "leg channels must be copied from the alternate pack");
    check(std::abs(channel_for(arm).rotations.values.front().z -
            make_clip("expected_arm", 1.0f, arm, 0.4f).channels.front().rotations.values.front().z)
            < 1e-5f,
        "unmasked upper-body channels must stay on the base pack");
    check(std::abs(channel_for(pelvis).rotations.times.back() - 1.0f) < 1e-5f,
        "alternate tracks must be retimed to the base clip duration");
}

void test_layers_move_only_masked_bones_and_emit_events()
{
    const Skeleton skeleton = make_skeleton();
    // Locomotion clip rotates the thigh and the arm; the action clip rotates the arm only.
    std::vector<AnimationClip> clips;
    AnimationClip walk = make_clip("walk", 1.0f, *skeleton.find_bone("thigh"), 0.8f);
    AnimationClip walk_arm = make_clip("walk_arm", 1.0f, *skeleton.find_bone("arm"), 0.2f);
    walk.channels.push_back(walk_arm.channels.front());
    clips.push_back(std::move(walk));
    clips.push_back(make_clip("attack", 1.0f, *skeleton.find_bone("arm"), 1.2f));
    clips.back().channels.push_back(
        make_clip("attack_thigh", 1.0f, *skeleton.find_bone("thigh"), -1.0f).channels.front());

    AnimationController controller(skeleton, std::move(clips));
    controller.set_upper_body_mask(BoneMask::branch(skeleton, "spine"));
    check(controller.play_animation("walk", true), "locomotion clip must start");
    const std::size_t walk_clip = controller.animator().active_clip();

    ActionSettings settings;
    settings.tag = 7;
    settings.duration = 0.8f;
    settings.hit_time = 0.35f;
    settings.blend_in = 0.1f;
    settings.blend_out = 0.2f;
    check(controller.play_action("attack", settings), "action must start");
    check(!controller.play_action("attack", settings), "running action must not restart");

    const std::size_t thigh = *skeleton.find_bone("thigh");
    const std::size_t arm = *skeleton.find_bone("arm");
    std::vector<AnimationEvent> events;
    controller.update(0.05f);
    check(controller.mixer().upper_weight() > 0.0f && controller.mixer().upper_weight() < 1.0f,
        "upper weight must blend in gradually");
    for (int step = 0; step < 4; ++step) {
        controller.update(0.05f);
    }
    // t = 0.25 s: fully blended in, hit not reached yet.
    check(std::abs(controller.mixer().upper_weight() - 1.0f) < 1e-4f, "upper weight must reach 1");
    const Pose& pose = controller.pose();
    check(std::abs(angle_of(pose.local_transforms[arm]) - 1.2f) < 1e-3f,
        "arm must follow the action while masked in");
    check(std::abs(angle_of(pose.local_transforms[thigh]) - 0.8f) < 1e-3f,
        "thigh must keep following locomotion");
    check(controller.animator().active_clip() == walk_clip && controller.animator().playing(),
        "locomotion must keep playing during the action");

    for (const AnimationEvent& event : controller.take_events()) {
        events.push_back(event);
    }
    check(events.size() == 1 && events[0].type == AnimationEventType::ActionStart,
        "only ActionStart is expected before the hit time");

    for (int step = 0; step < 6; ++step) {
        controller.update(0.05f);
    }
    // t = 0.55 s: hit emitted exactly once.
    for (const AnimationEvent& event : controller.take_events()) {
        events.push_back(event);
    }
    check(events.size() == 2 && events[1].type == AnimationEventType::ActionHit &&
            events[1].tag == 7,
        "ActionHit must fire once at hit time");

    for (int step = 0; step < 20; ++step) {
        controller.update(0.05f);
    }
    for (const AnimationEvent& event : controller.take_events()) {
        events.push_back(event);
    }
    check(events.size() == 3 && events[2].type == AnimationEventType::ActionEnd,
        "ActionEnd must fire once");
    check(!controller.mixer().active() && controller.mixer().upper_weight() == 0.0f,
        "upper layer must blend back to zero");
    check(std::abs(angle_of(controller.pose().local_transforms[arm]) - 0.2f) < 1e-3f,
        "arm must return to locomotion after the action");
    check(controller.animator().active_clip() == walk_clip && controller.animator().playing(),
        "locomotion must be unaffected after the action");
}

void test_action_can_retrigger_during_blend_out()
{
    const Skeleton skeleton = make_skeleton();
    std::vector<AnimationClip> clips;
    clips.push_back(make_clip("idle", 1.0f, *skeleton.find_bone("thigh"), 0.1f));
    clips.push_back(make_clip("attack", 1.0f, *skeleton.find_bone("arm"), 1.0f));
    AnimationController controller(skeleton, std::move(clips));
    controller.set_upper_body_mask(BoneMask::branch(skeleton, "spine"));
    ActionSettings settings;
    settings.duration = 0.5f;
    settings.blend_out = 0.2f;
    check(controller.play_action("attack", settings), "action must start");
    controller.update(0.2f);
    check(!controller.play_action("attack", settings), "must not restart mid-action");
    controller.update(0.2f);
    check(controller.play_action("attack", settings), "must accept a new action while blending out");
}

}

int main()
{
    try {
        test_bone_mask_branch();
        test_root_translation_axis_is_held();
        test_blend_is_shortest_path_and_weighted();
        test_masked_animation_channels_replace_lower_body_only();
        test_layers_move_only_masked_bones_and_emit_events();
        test_action_can_retrigger_during_blend_out();
    } catch (const std::exception& error) {
        std::cerr << "animation layer test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
