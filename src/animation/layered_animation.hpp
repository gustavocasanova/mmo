#pragma once

#include "animation/skeleton.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mmo::animation {

enum class AnimationLayer {
    LowerBody,
    UpperBody,
};

// Per-bone influence of the upper-body layer, 0 = lower layer only, 1 = upper layer only.
class BoneMask {
public:
    BoneMask() = default;
    explicit BoneMask(std::size_t bone_count, float weight = 0.0f);

    // Weight 1 for `root_bone` and every descendant. Throws if the bone does not exist.
    [[nodiscard]] static BoneMask branch(const Skeleton& skeleton, std::string_view root_bone);

    void set_weight(std::size_t bone_index, float weight);
    [[nodiscard]] float weight(std::size_t bone_index) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t active_bone_count() const;
    [[nodiscard]] std::vector<std::string> active_bone_names(const Skeleton& skeleton) const;

private:
    std::vector<float> weights_;
};

enum class AnimationEventType {
    ActionStart,
    ActionHit,
    ActionEnd,
};

struct AnimationEvent {
    AnimationEventType type = AnimationEventType::ActionStart;
    std::uint32_t tag = 0;
    float time_seconds = 0.0f;
};

struct ActionSettings {
    std::uint32_t tag = 0;
    // Length of the action in seconds at speed 1; 0 uses the clip duration.
    float duration = 0.0f;
    float speed = 1.0f;
    // Moment (action seconds) at which ActionHit is emitted; negative disables the event.
    float hit_time = -1.0f;
    float blend_in = 0.1f;
    float blend_out = 0.15f;
};

// Plays a one-shot clip on the upper-body layer and blends it over the lower-body pose
// through a bone mask. The lower pose is never replaced, only overridden on masked bones.
class AnimationMixer {
public:
    AnimationMixer() = default;
    explicit AnimationMixer(BoneMask upper_body_mask);

    // Returns false when an action is already running (it can be retriggered during blend-out).
    [[nodiscard]] bool start_action(std::size_t clip_index, float clip_duration,
        const ActionSettings& settings);
    void cancel_action();
    void update(float delta_seconds, std::vector<AnimationEvent>& events);
    // Writes lower-body locals blended with the upper clip and rebuilds the matrices.
    void mix(const Animator& animator, const Pose& lower, Pose& output) const;

    [[nodiscard]] static Transform blend(const Transform& lower, const Transform& upper, float weight);

    [[nodiscard]] bool active() const;
    [[nodiscard]] bool acting() const;
    [[nodiscard]] float upper_weight() const;
    [[nodiscard]] float lower_weight() const;
    [[nodiscard]] float action_time() const;
    [[nodiscard]] float action_duration() const;
    [[nodiscard]] std::size_t action_clip() const;
    [[nodiscard]] const ActionSettings& settings() const;
    [[nodiscard]] const BoneMask& mask() const;
    [[nodiscard]] std::size_t active_layer_count() const;

private:
    BoneMask mask_;
    ActionSettings settings_{};
    std::size_t clip_index_ = kNoBone;
    float clip_duration_ = 0.0f;
    float duration_ = 0.0f;
    float time_ = 0.0f;
    float weight_ = 0.0f;
    bool running_ = false;
    bool hit_emitted_ = false;
    bool started_event_pending_ = false;
};

}
