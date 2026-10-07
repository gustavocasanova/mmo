#include "animation/layered_animation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mmo::animation {
namespace {

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float smoothstep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}

BoneMask::BoneMask(std::size_t bone_count, float weight)
    : weights_(bone_count, std::clamp(weight, 0.0f, 1.0f))
{
}

BoneMask BoneMask::branch(const Skeleton& skeleton, std::string_view root_bone)
{
    const std::optional<std::size_t> root = skeleton.find_bone(root_bone);
    if (!root) {
        throw std::invalid_argument("bone mask root bone not found: " + std::string(root_bone));
    }
    BoneMask mask(skeleton.bones.size());
    for (const Bone& bone : skeleton.bones) {
        std::size_t current = bone.index;
        while (current != kNoBone) {
            if (current == *root) {
                mask.weights_[bone.index] = 1.0f;
                break;
            }
            current = skeleton.bones[current].parent_index;
        }
    }
    return mask;
}

void BoneMask::set_weight(std::size_t bone_index, float weight)
{
    if (bone_index >= weights_.size()) {
        throw std::out_of_range("bone mask index out of range");
    }
    weights_[bone_index] = std::clamp(weight, 0.0f, 1.0f);
}

float BoneMask::weight(std::size_t bone_index) const
{
    return bone_index < weights_.size() ? weights_[bone_index] : 0.0f;
}

std::size_t BoneMask::size() const
{
    return weights_.size();
}

std::size_t BoneMask::active_bone_count() const
{
    return static_cast<std::size_t>(std::count_if(
        weights_.begin(), weights_.end(), [](float weight) { return weight > 0.0f; }));
}

std::vector<std::string> BoneMask::active_bone_names(const Skeleton& skeleton) const
{
    std::vector<std::string> names;
    for (std::size_t index = 0; index < weights_.size() && index < skeleton.bones.size(); ++index) {
        if (weights_[index] > 0.0f) {
            names.push_back(skeleton.bones[index].name);
        }
    }
    return names;
}

AnimationMixer::AnimationMixer(BoneMask upper_body_mask)
    : mask_(std::move(upper_body_mask))
{
}

bool AnimationMixer::start_action(
    std::size_t clip_index,
    float clip_duration,
    const ActionSettings& settings)
{
    if (!std::isfinite(clip_duration) || clip_duration <= 0.0f ||
        !std::isfinite(settings.duration) || settings.duration < 0.0f ||
        !std::isfinite(settings.speed) || settings.speed <= 0.0f ||
        !std::isfinite(settings.blend_in) || settings.blend_in < 0.0f ||
        !std::isfinite(settings.blend_out) || settings.blend_out < 0.0f) {
        throw std::invalid_argument("invalid action settings");
    }
    if (running_ && time_ < duration_ - settings_.blend_out) {
        return false;
    }
    settings_ = settings;
    clip_index_ = clip_index;
    clip_duration_ = clip_duration;
    duration_ = settings.duration > 0.0f ? settings.duration : clip_duration;
    settings_.blend_out = std::min(settings_.blend_out, duration_);
    time_ = 0.0f;
    running_ = true;
    hit_emitted_ = false;
    started_event_pending_ = true;
    return true;
}

void AnimationMixer::cancel_action()
{
    running_ = false;
    weight_ = 0.0f;
    started_event_pending_ = false;
    clip_index_ = kNoBone;
}

void AnimationMixer::update(float delta_seconds, std::vector<AnimationEvent>& events)
{
    if (!running_ && weight_ <= 0.0f) {
        return;
    }
    const float elapsed = std::max(delta_seconds, 0.0f);
    if (started_event_pending_) {
        events.push_back({AnimationEventType::ActionStart, settings_.tag, 0.0f});
        started_event_pending_ = false;
    }

    float target = 0.0f;
    if (running_) {
        time_ = std::min(time_ + elapsed * settings_.speed, duration_);
        if (!hit_emitted_ && settings_.hit_time >= 0.0f && time_ >= settings_.hit_time) {
            hit_emitted_ = true;
            events.push_back({AnimationEventType::ActionHit, settings_.tag, time_});
        }
        target = time_ < duration_ - settings_.blend_out ? 1.0f : 0.0f;
        if (time_ >= duration_) {
            running_ = false;
            events.push_back({AnimationEventType::ActionEnd, settings_.tag, time_});
        }
    }

    const float blend_time = target > weight_ ? settings_.blend_in : settings_.blend_out;
    const float step = blend_time > 0.0f ? elapsed / blend_time : 1.0f;
    if (target > weight_) {
        weight_ = std::min(weight_ + step, target);
    } else {
        weight_ = std::max(weight_ - step, target);
    }
    if (!running_ && weight_ <= 0.0f) {
        clip_index_ = kNoBone;
    }
}

Transform AnimationMixer::blend(const Transform& lower, const Transform& upper, float weight)
{
    weight = std::clamp(weight, 0.0f, 1.0f);
    Transform result;
    for (std::size_t component = 0; component < 3; ++component) {
        result.translation[component] =
            lerp(lower.translation[component], upper.translation[component], weight);
        result.scale[component] = lerp(lower.scale[component], upper.scale[component], weight);
    }
    const float dot = lower.rotation.x * upper.rotation.x + lower.rotation.y * upper.rotation.y +
        lower.rotation.z * upper.rotation.z + lower.rotation.w * upper.rotation.w;
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    Quaternion q{
        lerp(lower.rotation.x, upper.rotation.x * sign, weight),
        lerp(lower.rotation.y, upper.rotation.y * sign, weight),
        lerp(lower.rotation.z, upper.rotation.z * sign, weight),
        lerp(lower.rotation.w, upper.rotation.w * sign, weight),
    };
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length > 0.0f) {
        q = {q.x / length, q.y / length, q.z / length, q.w / length};
    } else {
        q = lower.rotation;
    }
    result.rotation = q;
    return result;
}

void AnimationMixer::mix(const Animator& animator, const Pose& lower, Pose& output) const
{
    output.local_transforms = lower.local_transforms;
    const float layer_weight = smoothstep(weight_);
    if (clip_index_ != kNoBone && layer_weight > 0.0f) {
        const float clip_time = std::min(time_, duration_) / duration_ * clip_duration_;
        std::vector<BoneTransform> upper;
        animator.sample_clip_pose(clip_index_, clip_time, upper);
        const std::size_t count = std::min(upper.size(), output.local_transforms.size());
        for (std::size_t bone = 0; bone < count; ++bone) {
            const float weight = mask_.weight(bone) * layer_weight;
            if (weight > 0.0f) {
                output.local_transforms[bone] =
                    blend(output.local_transforms[bone], upper[bone], weight);
            }
        }
    }
    animator.resolve_pose(output);
}

bool AnimationMixer::active() const
{
    return running_ || weight_ > 0.0f;
}

bool AnimationMixer::acting() const
{
    return running_;
}

float AnimationMixer::upper_weight() const
{
    return smoothstep(weight_);
}

float AnimationMixer::lower_weight() const
{
    return 1.0f;
}

float AnimationMixer::action_time() const
{
    return time_;
}

float AnimationMixer::action_duration() const
{
    return duration_;
}

std::size_t AnimationMixer::action_clip() const
{
    return clip_index_;
}

const ActionSettings& AnimationMixer::settings() const
{
    return settings_;
}

const BoneMask& AnimationMixer::mask() const
{
    return mask_;
}

std::size_t AnimationMixer::active_layer_count() const
{
    return active() ? 2 : 1;
}

}
